#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <doublefann.c>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h> // O_ constants...
#include <errno.h> // Just to be able to clear last error. Remove this include later.


// DEFINES
#define SIZE 35
#define ELEMENT_SIZE (600 / SIZE)
#define NR_OF_WAVES 6
#define COST_OF_ONE_STEP 0.05
#define ANN_NUM_INPUT (SIZE * SIZE + 2)
#define ANN_NUM_OUTPUT 4
#define ANN_NUM_NEURONS_HIDDEN_1 (ANN_NUM_INPUT)
#define ANN_NUM_NEURONS_HIDDEN_2 (ANN_NUM_NEURONS_HIDDEN_1 / 3)
#define ANN_NUM_NEURONS_HIDDEN_3 (ANN_NUM_OUTPUT * 4)
#define ANN_NUM_LAYERS 5

#define NAME_LENGTH 50
#define TRAIN_DATA_FILENAME "trainingdata_%din_%dout.dat"
#define ANN_FILENAME "ann_%din_%dout.dat"

#define MAX_EPOCHS 3
#define EPOCHS_BETWEEN_REPORTS 10
#define DESIRED_ERROR 0.1
#define LEARNING_RATE 0.9 // No impact on RPROP
#define ERROR_FUNCTION FANN_ERRORFUNC_TANH
#define ANN_TRAIN_ALGO FANN_TRAIN_QUICKPROP

struct args{
    int argc;
    char ** argv;
};

static struct args arguments;

typedef enum{
    ACTION_RUN,
    ACTION_TRAIN,
    ACTION_GATHER,
    ACTION_CLOSE,
    ACTION_NR_OF
} actionEnum;

actionEnum actions[ACTION_NR_OF] =
{
    [ACTION_RUN] = ACTION_RUN,
    [ACTION_TRAIN] = ACTION_TRAIN,
    [ACTION_GATHER] = ACTION_GATHER,
    [ACTION_CLOSE] = ACTION_CLOSE
};

static bool g_stopTraining = FALSE;
static GtkWidget *drawingArea;
static struct fann_train_data *trainingData;
static struct fann *ann;
const char messageQueuePath[] = "/pathFinderMessageQueue1";

static char trainDataFilename[NAME_LENGTH];
static char configFilename[NAME_LENGTH];

typedef struct
{
    // Data used as input to the pathfinding
    double intensity; // "Height" of the terrain.
    gboolean visited; // allready processed by pathf. algo.
    double cost; // The minimum cost of reaching the goal from this node
    gboolean chosen; // True menas this node is part of the path.
    gboolean chosenBySimple;
    gboolean chosenByAnn;
    // The route chosen by the reference path finding algorithm
    int route_x; // x-componenet of the step to the next node
    int route_y; // y-component of the step to the next node
    int dir; // direction to the next node. North east south or west
    //The route chosen by the ANN
    int annRoute_x; // x-componenet of the step to the next node
    int annRoute_y; // y-component of the step to the next node
} dataNode;

typedef struct
{
    GtkWidget *frequencyEntry;
    GtkEntryBuffer *frequencyBuffer;
} waveEntry;

typedef struct
{
    double amplitude; // 0-1
    double frequency; // revolutions per pixel
    double x; // origo of the wave
    double y; //  origo of the wave
} wave;

static dataNode theData[SIZE][SIZE] = {0};
static waveEntry waveEntries[NR_OF_WAVES];

static wave waves[NR_OF_WAVES] = {
    {.amplitude = 0.4, .frequency = 0.15, .x = 0, .y = -SIZE},
    {.amplitude = 0.25, .frequency = 0.3, .x = SIZE / 3, .y = -3},
    {.amplitude = 0.15, .frequency = 0.52, .x = SIZE / 4, .y = SIZE / 2}};

FANN_EXTERNAL typedef int (FANN_API * fann_callback_type) (struct fann *ann, struct fann_train_data *train, 
														   unsigned int max_epochs, 
														   unsigned int epochs_between_reports, 
														   float desired_error, unsigned int epochs);

int PrintStatusAndConsiderStop(struct fann *ann, struct fann_train_data *train, 
                               unsigned int max_epochs, 
                               unsigned int epochs_between_reports, 
                               float desired_error, 
                               unsigned int epochs)
{
    printf("Epochs     %8d. Current error: %.10f. Bit fail %d.\n",
           epochs,
           desired_error,
           ann->num_bit_fail);
    if (g_stopTraining == TRUE)
    {
        g_stopTraining = FALSE;
        return -1;
    }
    
    return 0;
}

static void configureAnn(struct fann *annToConfigure)
{
    fann_set_activation_function_hidden(annToConfigure, FANN_SIGMOID);
    fann_set_activation_function_output(annToConfigure, FANN_SIGMOID);
    fann_set_training_algorithm(annToConfigure, ANN_TRAIN_ALGO);
    fann_set_learning_rate(annToConfigure, LEARNING_RATE);
    fann_set_train_error_function(annToConfigure, ERROR_FUNCTION); 
    fann_set_callback(annToConfigure, PrintStatusAndConsiderStop);
}

static void randomizeWaveFrequencies()
{
    // Randomize frequency of waves that build up the terrain
    for (int i = 0; i < NR_OF_WAVES; i++)
    {
        char frequency[10];
        snprintf(frequency, 6, "%4lf", ((double)rand())/(RAND_MAX*4.0));
        waveEntries[i].frequencyBuffer = gtk_entry_buffer_new(frequency, 4);
        waveEntries[i].frequencyEntry = gtk_entry_new_with_buffer(waveEntries[i].frequencyBuffer);
    }
}

static void directionToXY(int dir, int *x, int *y)
{
    int sign = 1 - (dir & 2);
    *x = ((~dir) & 1) * sign;
    *y = ((dir) & 1) * sign;
}

static double costOfOneStep(double fromIntensity, double toIntensity)
{
	return COST_OF_ONE_STEP + fmax(fromIntensity - toIntensity, 0.0);
}


static double calcCost(int parentX, int parentY, int thisX, int thisY)
{
    int deltaX;
    int deltaY;
    int nextX;
    int nextY;
    double currentCost;
    double parentIntensity = theData[parentX][parentY].intensity;

    dataNode *n = &theData[thisX][thisY];
    if (n->visited == FALSE && (thisX != 0 || thisY != 0))
    {
        n->visited = TRUE;
        /* Set init value so that a loop route will return extremely
        * high value and will not be chosen. */
        n->cost = DBL_MAX/2;
        for (int dir = 0; dir < 4; dir++)
        {
            directionToXY(dir, &deltaX, &deltaY);
            nextX = thisX + deltaX;
            nextY = thisY + deltaY;

            if ((nextX != parentX || nextY != parentY) &&
                (nextX >= 0) &&
                (nextY >= 0) &&
                (nextX < SIZE) &&
                (nextY < SIZE))
            {
                currentCost = calcCost(thisX, thisY, nextX, nextY);
                if(n->cost > currentCost)
                {
                    n->cost = currentCost;
                    n->route_x = deltaX;
                    n->route_y = deltaY;
                    n->dir = dir;
                }
            }

        }
    }
    return n->cost + costOfOneStep(parentIntensity, n->intensity);
}

static bool insideAndNotChosenByAnn(const int x, const int y)
{
    if ((x < SIZE) &&
        (y < SIZE) &&
        (x >= 0) &&
        (y >= 0) &&
        (theData[x][y].chosenByAnn == FALSE))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void findNextCoord(fann_type *fann_input, int *x, int *y)
{
    fann_run(ann, fann_input);
    int direction = 0;
    int deltaX = 0;
    int deltaY = 0;
    fann_type value = ann->output[0];
    
    for (int i = 0; i < 4; i++)
    {
		fann_type tmp = ann->output[i];
        if (tmp >= value)
        {
			direction = i;
			value = tmp;
		}
    }
    directionToXY(direction, &deltaX, &deltaY);
    *x += deltaX;
    *y += deltaY;
}

static void intensityToInput(fann_type *input)
{
    for (int i = 0; i < SIZE * SIZE; i++)
    {
        input[i] = theData[i/SIZE][i%SIZE].intensity;
    }
}

static void runAnn()
{
    fann_type fann_input[ANN_NUM_INPUT];
    intensityToInput(fann_input);
    int x = SIZE - 1;
    int y = SIZE - 1;
    double cost = 0;
	double_t intensityOfPreviousNode = theData[x][y].intensity;
    while (insideAndNotChosenByAnn(x,y))
    {
		cost += costOfOneStep(intensityOfPreviousNode, theData[x][y].intensity);
        theData[x][y].chosenByAnn = TRUE;
        if (x == 0 && y == 0)
        {
            printf("Ann cost is %lf\n", cost);
            break;
        }
        fann_input[SIZE * SIZE] = (fann_type)x;
        fann_input[SIZE * SIZE + 1] = (fann_type)y;
        findNextCoord(fann_input, &x,&y);
    }
}

static void prepareData()
{
#define START_INTENSITY 0.5
    double maxIntensity = -1000;
    double minIntensity = 10000;
    for (int w = 0; w < NR_OF_WAVES; w++)
    {
        srand(time(NULL)); 
        waves[w].frequency = (double)rand() / RAND_MAX / 2;
        waves[w].amplitude = (double)rand() / RAND_MAX;
        waves[w].x = (double)rand() / RAND_MAX * 2 * SIZE;
        waves[w].y = (double)rand() / RAND_MAX * 2 * SIZE;
    }
    
    memset(theData, 0, sizeof(theData));
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            theData[i][j].intensity = START_INTENSITY;
            for (int w = 0; w < NR_OF_WAVES; w++)
            {
                srand(time(NULL));
                double frequency = atof(gtk_entry_buffer_get_text(waveEntries[w].frequencyBuffer));
                double distanceToSignalOrigo = sqrt(pow(waves[w].x - i, 2.0) + pow(waves[w].y - j, 2.0));
                theData[i][j].intensity += waves[w].amplitude * sin(distanceToSignalOrigo * frequency);
                maxIntensity = MAX(maxIntensity, theData[i][j].intensity);
                minIntensity = MIN(minIntensity, theData[i][j].intensity);
            }
        }
    }
    
    //Scale the visible data
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            theData[i][j].intensity = (theData[i][j].intensity - minIntensity) / (maxIntensity - minIntensity); 
        }
    }
}



static gboolean delete_event(GtkWidget *widget,
                             GdkEvent *event,
                             gpointer data )
{
    g_print("delete event occurred\n");
    return FALSE;
}

static void destroy(GtkWidget *widget,
                    gpointer   data )
{
    gtk_main_quit();
}

static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    guint width, height;
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);
    for (int i = 0; i < SIZE; i++)
    {
        for(int j = 0; j < SIZE; j++)
        {
            dataNode *dat = &theData[i][j];

            // Paint landscape or the paths chosen.
            if (dat->chosen || dat->chosenByAnn || dat->chosenBySimple)
            {
                cairo_set_source_rgb(cr, (double)dat->chosen, (double)dat->chosenByAnn, (double)dat->chosenBySimple);
            }
            else
            {
                double intensity = theData[i][j].intensity;
                cairo_set_source_rgb(cr,
                                     intensity,
                                     intensity,
                                     intensity);
            }

//            cairo_set_source_rgb(cr, dat->cost/8, dat->cost/8, dat->cost/8);

            cairo_rectangle(cr,
                            i * ELEMENT_SIZE,
                            j * ELEMENT_SIZE,
                            i * ELEMENT_SIZE + ELEMENT_SIZE,
                            j * ELEMENT_SIZE + ELEMENT_SIZE);
            cairo_fill(cr);
        }
    }

    gtk_style_context_get_color(context,
                                gtk_style_context_get_state(context),
                                &color);
    gdk_cairo_set_source_rgba(cr, &color);

    cairo_fill(cr);

    return FALSE;
}



// For one solution recorded in "theData" - extract data for future ANN training;
static void extractTrainingData()
{
    // Go throught the selected nodes and add trainingdata
    int x = SIZE - 1;
    int y = SIZE - 1;
    int currDiag;
    double likelyhood;
    dataNode * n = &theData[x][y];

    struct fann_train_data *tmpTrainingData = fann_create_train(1, ANN_NUM_INPUT, ANN_NUM_OUTPUT);

    while(n->route_x != 0 || n->route_y != 0)
    {       
        /* Likelyhood of point being included in the dataset is 100%
         * on the diagonal and 1/ numberOfPointsOnDiagonal at the
         * start and end. */
        
        if (x + y > (SIZE - 1))
        {
            currDiag = MAX(abs(SIZE - 1 - x), (SIZE - 1 - y)); 
        }
        else
        {
            currDiag = MAX(x, y); 
        }
        
        likelyhood = hypot(currDiag, currDiag) / hypot(SIZE - 1 , SIZE - 1); 
        
        double randomNum = (double)rand() / RAND_MAX;

        if ((randomNum) < likelyhood)
        {
            intensityToInput(tmpTrainingData->input[0]);
            tmpTrainingData->input[0][SIZE * SIZE] = x;
            tmpTrainingData->input[0][SIZE * SIZE + 1] = y;
            tmpTrainingData->output[0][0] = 0;
            tmpTrainingData->output[0][1] = 0;
            tmpTrainingData->output[0][2] = 0;
            tmpTrainingData->output[0][3] = 0;
            tmpTrainingData->output[0][n->dir] = 1.0;

            // Merge the training data with existing set.
            trainingData = fann_merge_train_data(trainingData, tmpTrainingData);
        }
        x += n->route_x;
        y += n->route_y;
        n = &theData[x][y];
    }

    fann_destroy_train(tmpTrainingData);
}

static void findPath()
{
    // Find cost of each path
    calcCost(SIZE-1,
             SIZE-1,
             SIZE-1,
             SIZE-1);

    // Go throught the selected nodes.
    // Mark them and print the cost of the path from bottom right.
    int x = SIZE - 1;
    int y = x;
    dataNode * n = &theData[x][y];

    while(n->route_x != 0 || n->route_y != 0)
    {
        n->chosen = TRUE;
        x += n->route_x;
        y += n->route_y;
        n = &theData[x][y];
    }
    printf("Reference cost is %lf\n", theData[SIZE - 1][SIZE - 1].cost);
}

double calcSimpleCost(double fromIntensity, double toIntensity, double targetIntensity)
{
    double hightLossCost = 0;
    double stepcost = costOfOneStep(fromIntensity, toIntensity);
    // dont go down if target is above us. We will suffer later.
    if (stepcost == COST_OF_ONE_STEP)
    {
        hightLossCost = COST_OF_ONE_STEP + costOfOneStep(toIntensity, targetIntensity) - costOfOneStep(fromIntensity, targetIntensity);
    }
    return fmax(stepcost, hightLossCost);
}

void runSimple()
{
    int x = SIZE - 1;
    int y = SIZE - 1;
    double targetIntensity = theData[0][0].intensity;
    double totalCost = 0.0;
    theData[x][y].chosenBySimple = TRUE;
    while (x != 0 || y != 0)
    {
        double fromIntensity = theData[x][y].intensity;
        double upIntensity = theData[x][y - 1].intensity;
        double leftIntensity = theData[x - 1][y].intensity;
        double toIntensity;
        
        if (y == 0)
        {
            x--;
            toIntensity = leftIntensity;
            
        }
        else if (x == 0)
        {
            y--;
            toIntensity = upIntensity;
        }
        else
        {
            double costOfUp = calcSimpleCost(fromIntensity,
                                      upIntensity,
                                      targetIntensity);
            double costOfLeft = calcSimpleCost(fromIntensity,
                                        leftIntensity,
                                        targetIntensity);
            if (costOfUp > costOfLeft)
            {
                x--;
                toIntensity = leftIntensity;
            }
            else if(costOfUp < costOfLeft)
            {
                y--;
                toIntensity = upIntensity;
            }
            else
            {
                // If both directions are downhill from good hight compared to target, loose as little as possible
                // I.e. go to the lowest intensity.
                if (leftIntensity < upIntensity)
                {
                    x--;
                    toIntensity = leftIntensity;
                }
                else
                {
                    y--;
                    toIntensity = upIntensity;
                }
                
            }
        }
        totalCost += costOfOneStep(fromIntensity, toIntensity);
        theData[x][y].chosenBySimple = TRUE;
    }
    printf("Simple cost is: %lf\n", totalCost);
}


static void tryAlgorithms()
{
    // Generate the data
    prepareData();

    // Run the reference algorithm
    findPath();
    
    // Run the simple algorithm
    runSimple();

    // Run the Neural Network
    runAnn();

    // Redraw
    gtk_widget_queue_draw(drawingArea);
}

static void gatherTrainingData()
{
    for (int i = 0; i < 10; i++)
    {
        // Generate the data
        prepareData();

        // Run the reference algorithm
        findPath();

        // Extract training data from the results
        extractTrainingData();
        
        // Redraw the image
        gtk_widget_queue_draw(drawingArea); // IS THIS SAFE IN MULTITHREAD?
    }
}

static void trainAnn()
{
    fann_train_on_data(ann,
                       trainingData,
                       MAX_EPOCHS,
                       EPOCHS_BETWEEN_REPORTS,
                       DESIRED_ERROR);
}

void AbortTraining()
{
    g_stopTraining = TRUE;
}

void sendMsg(GtkWidget *clickedWidget, actionEnum *actionToTake)
{
    mqd_t messageQueue;
    int ret;
    messageQueue = mq_open(messageQueuePath, O_WRONLY);
    if (messageQueue == -1)
    {
        perror("sendMsg1: ");
    }
    ret = mq_send(messageQueue, (char*)actionToTake, sizeof(actionEnum), 0);
    if (ret == -1)
    {
        perror("sendMsg2: ");
    }
    
    if (*actionToTake == ACTION_CLOSE)
    {
        gtk_main_quit();
    }
}

static void startGUI(void *dummy)
{
    GtkWidget *window;
    // Buttons:
    GtkWidget *runAnnButton;
    GtkWidget *gatherDataButton;
    GtkWidget *trainButton;
    GtkWidget *closeButton;
    GtkWidget *stopButton;

    GtkWidget *box;
    GtkWidget *drawingBox;

    gtk_init(&arguments.argc, &arguments.argv);

    /* create a new window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "delete-event",
                      G_CALLBACK(delete_event), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(destroy), NULL);


    // Create the drawing area on which we will paint terrain and paths.
    drawingArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawingArea, SIZE*ELEMENT_SIZE, SIZE*ELEMENT_SIZE);
    gtk_widget_add_events(drawingArea,
                          GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(drawingArea), "draw", G_CALLBACK(draw_callback), NULL);

    // Create a box (container) for the buttons and fields.
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    // Create a box to contain the drawin area
    drawingBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    runAnnButton = gtk_button_new_with_label("Run Neural Net");
    gatherDataButton = gtk_button_new_with_label("Gather training data");
    trainButton = gtk_button_new_with_label("Train Neural Net");
    closeButton = gtk_button_new_with_label("Close");
    stopButton = gtk_button_new_with_label("Stop training");

    // Assign tasks for the buttons
    g_signal_connect(runAnnButton, "clicked",
                     G_CALLBACK(sendMsg),
                     &actions[ACTION_RUN]);
    g_signal_connect(gatherDataButton, "clicked",
                     G_CALLBACK(sendMsg),
                     &actions[ACTION_GATHER]);
    g_signal_connect(trainButton, "clicked",
                     G_CALLBACK(sendMsg),
                     &actions[ACTION_TRAIN]);
    g_signal_connect(closeButton, "clicked",
                     G_CALLBACK(sendMsg),
                     &actions[ACTION_CLOSE]);
    g_signal_connect(stopButton, "clicked",
                     G_CALLBACK(AbortTraining),
                     NULL);          



    // Pack the buttons and field into the box. top to bottom:
    gtk_box_pack_start(GTK_BOX(box), runAnnButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), gatherDataButton, FALSE, FALSE, 3);
    randomizeWaveFrequencies();
    for(int i = 0; i < NR_OF_WAVES; i++)
    {
        gtk_box_pack_start(GTK_BOX(box), waveEntries[i].frequencyEntry, FALSE, FALSE, 3);
    }
    gtk_box_pack_start(GTK_BOX(box), trainButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), stopButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), closeButton, FALSE, FALSE, 3);

    /* This packs the  box and drawingBox into the window (a gtk container). */
    gtk_box_pack_start(GTK_BOX(drawingBox), box, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(drawingBox), drawingArea, FALSE, FALSE, 3);
    gtk_container_add(GTK_CONTAINER (window), drawingBox);

    gtk_widget_show_all(window);
    gtk_main();
}

void model(void *dummy)
{
    mqd_t messageQueue;
    bool receiveMessages = TRUE;
    int ret = 0;
    
    typedef union
    {
        actionEnum action;
        uint8_t buf[10 * sizeof(actionEnum)];
    } messageBuf;
    
    messageBuf message;
    
    messageQueue = mq_open(messageQueuePath, (O_RDONLY));
    if (messageQueue == -1)
    {            
        perror("Opening message queue in Model:");
    }
    
    while(receiveMessages)
    {
        ret = mq_receive(messageQueue, (char*)&message.buf[0], sizeof(message), NULL);
        if (ret == -1)
        {
            perror("Model");    
        }
        else
        {
            switch (message.action)
            {
                case ACTION_CLOSE:
                    receiveMessages = FALSE;
                    break;
                case ACTION_GATHER:
                    gatherTrainingData();
                    break;
                case ACTION_TRAIN:
                    g_stopTraining = FALSE;
                    trainAnn();
                    break;
                case ACTION_RUN:
                    tryAlgorithms();
                    break;
            }
        }
    }
    
    mq_close(messageQueue);
    
}


int main(int argc, char *argv[])
{
    arguments.argc = argc;
    arguments.argv = argv;
    
    pthread_t guiThread;
    pthread_t modelThread;
    int ret;
    mqd_t messageQueue;
    
    FILE *trainingFile;
    snprintf(trainDataFilename, NAME_LENGTH, TRAIN_DATA_FILENAME, ANN_NUM_INPUT, ANN_NUM_OUTPUT);
    FILE *configFile;
    snprintf(configFilename, NAME_LENGTH, ANN_FILENAME, ANN_NUM_INPUT, ANN_NUM_OUTPUT);


    printf("About to create the ANN\n");
    // Open the neuron network config file.

    ann = fann_create_from_file(configFilename);
    if (ann == NULL)
    {
        // Unable to load existing ann. Creating a new one.
        ann = fann_create_standard(ANN_NUM_LAYERS,
                                   ANN_NUM_INPUT,
                                   ANN_NUM_NEURONS_HIDDEN_1,
                                   ANN_NUM_NEURONS_HIDDEN_2,
                                   ANN_NUM_NEURONS_HIDDEN_3,
                                   ANN_NUM_OUTPUT);

        configureAnn(ann);
    }

    printf("About to create the training data\n");
    // Open the training data file.
    trainingData = fann_read_train_from_file(trainDataFilename);
    if (trainingData == NULL)
    {
        // There was no training data file, so creat new training data.
        trainingData = fann_create_train(0,
                                         ANN_NUM_INPUT,
                                         ANN_NUM_OUTPUT); // Lets start with an empty data set and then merge a new one for each trial. Perhaps slow but lets see.
    }
    else
    {
		printf("Loaded %d training sets\n", trainingData->num_data);
	}

    struct mq_attr attr;  
    attr.mq_flags = 0;  
    attr.mq_maxmsg = 10;  
    attr.mq_msgsize = sizeof(actionEnum);  
    attr.mq_curmsgs = 0;
    messageQueue = mq_open(messageQueuePath, (int)(O_CREAT | O_RDWR), 0666, &attr);
    
    if (messageQueue == -1)
    {
        perror("main: ");
    }
    else
    {
        ret |= pthread_create(&guiThread, NULL, (void *)startGUI, NULL);
        ret |= pthread_create(&modelThread, NULL, (void *)model, NULL);

        if (ret == 0)
        {
            pthread_join(guiThread, NULL);
            pthread_join(modelThread, NULL);
        }
        else
        {
            printf("Something went wrong. Start guessing...\n");
        }
    }
    
    ret = mq_close(messageQueue);
    if (ret == -1) perror("Closing mq");

    // Save and destroy training data
    ret = fann_save_train(trainingData, trainDataFilename);
    if (ret == -1) perror("Save train");
    fann_destroy_train(trainingData);
    if (ret == -1) perror("Destroy train");

    // Save and destroy ANN.
    fann_save(ann, configFilename);
    if (ret == -1) perror("Save ANN");
    fann_destroy(ann);
    if (ret == -1) perror("Destroy ANN");
    
    return 0;
}
