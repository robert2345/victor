#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <doublefann.c>
#include <stdbool.h>

// DEFINES
#define SIZE 10
#define ELEMENT_SIZE 60
#define NR_OF_WAVES 6
#define COST_OF_ONE_STEP 0.05
#define ANN_NUM_INPUT (SIZE * SIZE + 2)
#define ANN_NUM_OUTPUT 4
#define ANN_NUM_NEURONS_HIDDEN (ANN_NUM_INPUT)
#define ANN_NUM_LAYERS 5

#define NAME_LENGTH 50
#define TRAIN_DATA_FILENAME "trainingdata_%din_%dout.dat"
#define ANN_FILENAME "ann_%din_%dout.dat"

static const bool EXTEND_TRAINING_DATA = TRUE;
static const float desired_error = (const float) 0.0001;
static struct fann_train_data *trainingData;
static const unsigned int max_epochs = 1;
static const unsigned int epochs_between_reports = 1;
static struct fann *ann;

char trainDataFilename[NAME_LENGTH];
char configFilename[NAME_LENGTH];

typedef struct
{
    // Data used as input to the pathfinding
    double intensity; // "Height" of the terrain.
    gboolean visited; // allready processed by pathf. algo.
    double cost; // The minimum cost of reaching the goal from this node
    gboolean chosen; // True menas this node is part of the path.
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
static double maxIntensity = 0;
static waveEntry waveEntries[NR_OF_WAVES];

static wave waves[NR_OF_WAVES] = {
    {.amplitude = 0.4, .frequency = 0.15, .x = 0, .y = -SIZE},
    {.amplitude = 0.25, .frequency = 0.3, .x = SIZE / 3, .y = -3},
    {.amplitude = 0.15, .frequency = 0.52, .x = SIZE / 4, .y = SIZE / 2}};

static void configureAnn(struct fann *annToConfigure)
{
    fann_set_activation_function_hidden(annToConfigure, FANN_SIGMOID);
    fann_set_activation_function_output(annToConfigure, FANN_SIGMOID);
    fann_set_training_algorithm(annToConfigure, FANN_TRAIN_RPROP);
}

static void randomizeWavFrequencies()
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


static double calcCost(int parentX, int parentY, int parentRX, int parentRY, double parentIntensity)
{
    int thisX = parentX + parentRX;
    int thisY = parentY + parentRY;

    dataNode *n = &theData[thisX][thisY];
    if (n->visited == FALSE && (thisX != 0 || thisY != 0))
    {
        n->visited = TRUE;
        /* Set init value so that a loop route will return extremely
        * high value and will not be chosen. */
        n->cost = DBL_MAX/2;
        for (int dir = 0; dir < 4; dir++)
        {
            int r_x;
            int r_y;
            directionToXY(dir, &r_x, &r_y);

            if ((r_x != -parentRX || r_y != -parentRY) &&
                (thisX + r_x >= 0) &&
                (thisY + r_y >= 0) &&
                (thisX + r_x < SIZE) &&
                (thisY + r_y < SIZE))
            {
                double currentCost = calcCost(thisX, thisY, r_x, r_y, n->intensity);
                if(n->cost > currentCost)
                {
                    n->cost = currentCost;
                    n->route_x = r_x;
                    n->route_y = r_y;
                    n->dir = dir;
                }
            }

        }
    }
    return n->cost + COST_OF_ONE_STEP + fmax(parentIntensity - n->intensity, 0);
}

static bool insideAndNotChosenByAnn(const int x, const int y)
{
    if ((x < SIZE) &&
        (y < SIZE) &&
        (x >= 0) &&
        (x >= 0) &&
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
    int direction = -1;
    fann_type value = ann->output[0];
    for (int i = 1; i < 4; i++)
    {
        if (ann->output[i] > value) direction = i;
    }

    directionToXY(direction, x, y);
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
    int x, y = SIZE - 1;

    while (insideAndNotChosenByAnn(x,y))
    {
        theData[x][y].chosenByAnn = TRUE;
        fann_input[SIZE * SIZE] = (fann_type)x;
        fann_input[SIZE * SIZE + 1] = (fann_type)y;
        findNextCoord(fann_input, &x,&y);
    }
}

static void prepareData()
{
    printf("Preparing the data\n");
#define START_INTENSITY 0.5
    maxIntensity = START_INTENSITY;
    for (int w = 0; w < NR_OF_WAVES; w++)
    {
        waves[w].amplitude = (double)rand() / RAND_MAX;
        waves[w].x = (double)rand() / RAND_MAX * 2 * SIZE;
        waves[w].y = (double)rand() / RAND_MAX * 2 * SIZE;
        maxIntensity += waves[w].amplitude;
    }
    memset(theData, 0, sizeof(theData));
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            theData[i][j].intensity = START_INTENSITY;
            for (int w = 0; w < NR_OF_WAVES; w++)
            {
                double frequency = atof(gtk_entry_buffer_get_text(waveEntries[w].frequencyBuffer));
                double distanceToSignalOrigo = sqrt(pow(waves[w].x - i, 2.0) + pow(waves[w].y - j, 2.0));
                theData[i][j].intensity += waves[w].amplitude * sin(distanceToSignalOrigo * frequency);

            }
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
            // Paint landscape or the paths chosen.
            double intensity = theData[i][j].intensity / maxIntensity;
            if (theData[i][j].chosen && theData[i][j].chosenByAnn)
            {
                cairo_set_source_rgb(cr, 1, 1, 0);
            }
            else if (theData[i][j].chosen)
            {
                cairo_set_source_rgb(cr, 1, 0, 0);
            }
            else if (theData[i][j].chosenByAnn)
            {
                cairo_set_source_rgb(cr, 0, 1, 0);
            }
            else
            {
                cairo_set_source_rgb(cr,
                                     intensity,
                                     intensity,
                                     intensity);
            }
            cairo_rectangle(cr,
                            i*ELEMENT_SIZE,
                            j*ELEMENT_SIZE,
                            i*ELEMENT_SIZE+ELEMENT_SIZE,
                            j*ELEMENT_SIZE+ELEMENT_SIZE);
            cairo_fill(cr);
        }
    }

    gtk_style_context_get_color(context,
                                gtk_style_context_get_state (context),
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
    int y = x;
    dataNode * n = &theData[x][y];

    struct fann_train_data *tmpTrainingData = fann_create_train(1, ANN_NUM_INPUT, ANN_NUM_OUTPUT);

<<<<<<< HEAD
    while(n->route_x != 0 || n->route_y != 0)
    {
        intensityToInput(tmpTrainingData->input[0]);
        tmpTrainingData->input[0][SIZE * SIZE] = x;
        tmpTrainingData->input[0][SIZE * SIZE + 1] = y;
        tmpTrainingData->output[0][0] = -1.0;
        tmpTrainingData->output[0][1] = -1.0;
        tmpTrainingData->output[0][2] = -1.0;
        tmpTrainingData->output[0][3] = -1.0;
        tmpTrainingData->output[0][n->dir] = 1.0;

        // Merge the training data with existing set.
        trainingData = fann_merge_train_data(trainingData, tmpTrainingData);

        x += n->route_x;
        y += n->route_y;
        n = &theData[x][y];
    }

    fann_destroy_train(tmpTrainingData);
}

static void findPath()
{
    // Find path
    printf("The cost from SIZE, SIZE to 0,0 is %lf\n", calcCost(SIZE-1 ,SIZE-1,0,0,theData[SIZE-1][SIZE-1].intensity));

    // Go throught the selected nodes and mark them.
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
}


static void tryAlgorithms(GtkWidget *widget,
                          gpointer data)
{
    GtkWidget *drawing_area = (GtkWidget*)data;

    // Generate the data
    prepareData();

    // Run the reference algorithm
    findPath();

    // Run the Neural Network
    runAnn();

    // Redraw
    gtk_widget_queue_draw(drawing_area);
}

static void gatherTrainingData()
{
    // Generate the data
    prepareData();

    // Run the reference algorithm
    findPath();

    // Extract training data from the results
    extractTrainingData();
}

static void trainAnn()
{
    #define MAX_EPOCHS 25
    #define EPOCHS_BETWEEN_REPORTS 5
    #define DESIRED_ERROR 0.1

    fann_train_on_data(ann,
                       trainingData,
                       MAX_EPOCHS,
                       EPOCHS_BETWEEN_REPORTS,
                       DESIRED_ERROR);
}

static void startGUI(int argc, char *argv[])
{

    GtkWidget *window;
    // Buttons:
    GtkWidget *runAnnButton;
    GtkWidget *gatherDataButton;
    GtkWidget *trainButton;
    GtkWidget *closeButton;

    GtkWidget *drawing_area;
    GtkWidget *box;
    GtkWidget *drawingBox;

    gtk_init(&argc, &argv);

    /* create a new window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "delete-event",
                      G_CALLBACK(delete_event), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(destroy), NULL);


    // Create the drawing area on which we will paint terrain and paths.
    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, SIZE*ELEMENT_SIZE, SIZE*ELEMENT_SIZE);
    gtk_widget_add_events(drawing_area,
                          GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(draw_callback), NULL);

    // Create a box (container) for the buttons and fields.
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    // Create a box to contain the drawin area
    drawingBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    runAnnButton = gtk_button_new_with_label("Run Neural Net");
    gatherDataButton = gtk_button_new_with_label("Gather training data");
    trainButton = gtk_button_new_with_label("Train Neural Net");
    closeButton = gtk_button_new_with_label("Close");

    // Assign tasks for the buttons
    g_signal_connect(runAnnButton, "clicked",
              G_CALLBACK(tryAlgorithms), drawing_area);

    g_signal_connect(gatherDataButton, "clicked",
              G_CALLBACK(gatherTrainingData), NULL);

    g_signal_connect(trainButton, "clicked",
              G_CALLBACK(trainAnn), NULL);

    g_signal_connect(closeButton, "clicked",
              G_CALLBACK(destroy), NULL);


    // Pack the buttons and field into the box. top to bottom:
    gtk_box_pack_start(GTK_BOX(box), runAnnButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), gatherDataButton, FALSE, FALSE, 3);
    for(int i = 0; i < NR_OF_WAVES; i++)
    {
        gtk_box_pack_start(GTK_BOX(box), waveEntries[i].frequencyEntry, FALSE, FALSE, 3);
    }
    gtk_box_pack_start(GTK_BOX(box), trainButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), closeButton, FALSE, FALSE, 3);

    /* This packs the  box and drawingBox into the window (a gtk container). */
    gtk_box_pack_start(GTK_BOX(drawingBox), box, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(drawingBox), drawing_area, FALSE, FALSE, 3);
    gtk_container_add(GTK_CONTAINER (window), drawingBox);

    gtk_widget_show_all(window);
    gtk_main();
}


int main(int argc, char *argv[])
{


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
                                   ANN_NUM_NEURONS_HIDDEN,
                                   ANN_NUM_NEURONS_HIDDEN,
                                   ANN_NUM_NEURONS_HIDDEN,
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

    startGUI(argc, argv);

    // Save and destroy training data
    fann_save_train(trainingData, trainDataFilename);
    fann_destroy_train(trainingData);

    // Save and destroy ANN.
    fann_save(ann, configFilename);
    fann_destroy(ann);

    return 0;
}
