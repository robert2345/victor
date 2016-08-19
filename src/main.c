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

#define TRAIN_DATA_FILENAME "trainingdata_%din_%dout.dat"

bool trainingEnabled = TRUE;
const float desired_error = (const float) 0.0001;
struct fann_train_data *trainingData;
const unsigned int max_epochs = 1;
const unsigned int epochs_between_reports = 1;
struct fann *ann;

typedef struct
{
    double intensity;
    int route_x; // x-componenet of the step to the next node
    int route_y; // y-component of the step to the next node
    int dir; // direction to the next node. North east south or west
    gboolean visited;
    double cost;
    gboolean chosen;
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

static double calcCost(int parentX, int parentY, int parentRX, int parentRY, double parentIntensity)
{
    int thisX = parentX + parentRX;
    int thisY = parentY + parentRY;

    dataNode *n = &theData[thisX][thisY];
    if (n->visited == FALSE && (thisX != 0 || thisY != 0))
    {
        n->visited = TRUE;
        n->cost = DBL_MAX/2; // This is good in a way because loops can not be resolved, but will return extremely high value and will not be chosen.
        for (int dir = 0; dir < 4; dir++)
        {
            int sign = 1 - (dir & 2);
            int r_x = ((~dir) & 1) * sign;
            int r_y = ((dir) & 1) * sign;

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

static void prepareData()
{
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
        for(int j = 0; j < SIZE; j++)
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


static void hello(GtkWidget *widget,
                  gpointer data )
{
    g_print ("Hello World\n");
}

static gboolean delete_event(GtkWidget *widget,
                             GdkEvent *event,
                             gpointer data )
{
    g_print ("delete event occurred\n");
    return FALSE;
}

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    gtk_main_quit();
}

static void drawing_clicked(GtkWidget *widget,
                            GdkEvent  *event,
                            gpointer   user_data)
{
    GdkEventButton *buttonPress = (GdkEventButton*)event;
    if (event->type == GDK_BUTTON_PRESS)
    {
        //Toggle the color of one element
        int draw_area_origin_x;
        int draw_area_origin_y;
        int theX;
        int theY;
        theX = (int)buttonPress->x/ELEMENT_SIZE;
        theY = (int)buttonPress->y/ELEMENT_SIZE;
        if (theX >= SIZE*ELEMENT_SIZE || theY >= SIZE*ELEMENT_SIZE || theX <0 || theY < 0)
        {
            printf("Out of bounds! theX: %d, theY:%d, click X:%lf, Y:%lf\n", theX, theY, buttonPress->x, buttonPress->y);
        }
        else
        {
            if (theData[theX][theY].intensity == 0)
            {
                theData[theX][theY].intensity = 1;
            }
            else
            {
               theData[theX][theY].intensity = 0;
            }
            gtk_widget_queue_draw(widget);
        }
    }
}

static gboolean draw_callback (GtkWidget *widget, cairo_t *cr, gpointer data)
{
    guint width, height;
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background (context, cr, 0, 0, width, height);
    for (int i = 0; i < SIZE; i++)
    {
        for(int j = 0; j < SIZE; j++)
        {
            double intensity = theData[i][j].intensity / maxIntensity;
            if (theData[i][j].chosen)
        {
            cairo_set_source_rgb(cr, 1, 0, 0);
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

    gtk_style_context_get_color (context,
                               gtk_style_context_get_state (context),
                               &color);
    gdk_cairo_set_source_rgba (cr, &color);

    cairo_fill (cr);

    return FALSE;
}


static void calculateAndDraw(GtkWidget *widget,
                             gpointer data )
{
    GtkWidget *drawing_area = (GtkWidget*)data;

    // Generate the data
    printf("Preparing the data\n");
    prepareData();

    // Find path
    printf("The cost from SIZE, SIZE to 0,0 is %lf\n", calcCost(SIZE-1 ,SIZE-1,0,0,theData[SIZE-1][SIZE-1].intensity));

    // Prepare ANN tranining Data
    for (int i = 0; i < SIZE * SIZE; i++)
    {
        trainingData->input[0][i] = theData[i/SIZE][i%SIZE].intensity;
    }

    // Go throught the selected nodes and mark them.
    // At the same time, train the neural net to make that correct choice.
    int x = SIZE - 1;
    int y = x;
    dataNode * n = &theData[x][y];
    dataSetIndex = trainingData->num_data;
    while(n->route_x != 0 || n->route_y != 0)
    {
        if (EXTEND_TRAINING_DATA)
        {

            for (int i = 0; i < SIZE * SIZE; i++)
            {
                trainingData->input[0][i] = theData[i/SIZE][i%SIZE].intensity;
            }
            trainingData->input[0][SIZE * SIZE] = x;
            trainingData->input[0][SIZE * SIZE + 1] = y;
            trainingData->output[0][0] = -1.0;
            trainingData->output[0][1] = -1.0;
            trainingData->output[0][2] = -1.0;
            trainingData->output[0][3] = -1.0;
            trainingData->output[0][n->dir] = 1.0;
            FANN_EXTERNAL struct fann_train_data *FANN_API fann_merge_train_data(struct fann_train_data *data1,
                                                                                                                                        struct fann_train_data *data2)
            if (trainingEnabled)
            {
                fann_train_on_data(ann, trainingData, max_epochs, epochs_between_reports, desired_error);
            }
            else
            {
                fann_type *annResults;
                annResults = fann_run(ann, trainingData->input[0]);
            }
        }
        n->chosen = TRUE;
        x += n->route_x;
        y += n->route_y;
        n = &theData[x][y];
    }

    // Redraw
    gtk_widget_queue_draw(drawing_area);
}


int main( int   argc,
          char *argv[] )
{
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *closeButton;
    GtkWidget *drawing_area;
    GtkWidget *box;
    GtkWidget *drawingBox;

    const uint32_t nameLength = 50;
    char filename[nameLength];
    FILE *trainingFile;
    snprintf(filename, nameLength, TRAIN_DATA_FILENAME, num_inputs, num_outputs);

    printf("About to create the ANN\n");
    fann
    ann = fann_create_standard(ANN_NUM_LAYERS, ANN_NUM_INPUT, ANN_NUM_NEURONS_HIDDEN, ANN_NUM_NEURONS_HIDDEN, ANN_NUM_NEURONS_HIDDEN, ANN_NUM_OUTPUT);
    printf("About to create the training data\n");
    // Open the training data file.
    trainingFile = fopen(filename, 'r');
    // Check that the file pointer is not NULL
    if (trainingFile != NULL)
    {
        trainingData = fann_read_train_from_file(const char *filename); // What happens here if the data is badly formatted?
        // Check that the training data has the correct format
        uint32_t num_inputs = fann_num_input_train_data(struct fann_train_data *data);
        uint32_t num_outputs = fann_num_output_train_data(struct fann_train_data *data);
        if (num_inputs != ANN_NUM_INPUT|| num_outputs != ANN_NUM_OUTPUT)
        {
            snprintf(filename, nameLength, TRAIN_DATA_FILENAME, num_inputs, num_outputs);
            printf("The saved training data is of the wrong format. Saving it as %s and creating new empty training data.", filename);
            fclose(trainingFile); // Close the file with the bad data.
            remove(trainingFile); // And remove the bad file.
            trainingFile = fopen(filename, "w");
            fann_save_train(trainingData, filename);
            fclose(filename);
            fann_destroy_train(trainingData);
            trainingData = fann_create_train(1, ANN_NUM_INPUT, ANN_NUM_OUTPUT);
        }
    }
    else
    {
        // There was no training data file, so creat new training data.
        trainingData = fann_create_train(0, ANN_NUM_INPUT, ANN_NUM_OUTPUT); // Lets start with an empty data set and then merge a new one for each trial. Perhaps slow but lets see.
    }

    fann_set_activation_function_hidden(ann, FANN_SIGMOID);
    fann_set_activation_function_output(ann, FANN_SIGMOID);
    fann_set_training_algorithm(ann, FANN_TRAIN_RPROP);

    gtk_init (&argc, &argv);

    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    g_signal_connect (window, "delete-event",
              G_CALLBACK (delete_event), NULL);
    g_signal_connect (window, "destroy",
              G_CALLBACK (destroy), NULL);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, SIZE*ELEMENT_SIZE, SIZE*ELEMENT_SIZE);
    gtk_widget_add_events (drawing_area,
                           GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(draw_callback), NULL);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    drawingBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

    button = gtk_button_new_with_label ("Generate\nand\ncalculate");
    closeButton = gtk_button_new_with_label ("Close");

    for (int i = 0; i < NR_OF_WAVES; i++)
    {
        char frequency[10];
        snprintf (frequency, 6, "%4lf", ((double)rand())/(RAND_MAX*4.0));
        waveEntries[i].frequencyBuffer = gtk_entry_buffer_new(frequency, 4);
        waveEntries[i].frequencyEntry = gtk_entry_new_with_buffer(waveEntries[i].frequencyBuffer);
    }

    g_signal_connect(button, "clicked",
              G_CALLBACK (calculateAndDraw), drawing_area);

    g_signal_connect(closeButton, "clicked",
              G_CALLBACK (destroy), NULL);

    /* This packs the button into the window (a gtk container). */
    gtk_container_add (GTK_CONTAINER (window), drawingBox);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 3);
    for(int i = 0; i < NR_OF_WAVES; i++)
    {
        gtk_box_pack_start (GTK_BOX (box), waveEntries[i].frequencyEntry, FALSE, FALSE, 3);
    }
    gtk_box_pack_start (GTK_BOX (box), closeButton, FALSE, FALSE, 3);
    gtk_box_pack_start (GTK_BOX (drawingBox), box, FALSE, FALSE, 3);
    gtk_box_pack_start (GTK_BOX (drawingBox), drawing_area, FALSE, FALSE, 3);

    gtk_widget_show_all (window);
    gtk_main ();

    // Save and destroy training data
    snprintf(filename, nameLength, TRAIN_DATA_FILENAME, num_inputs, num_outputs);
    trainingFile = fopen(filename, "w");
    fann_save_train(trainingData, filename);
    fann_destroy_train(trainingData);

    // Save and destroy ANN.
    FILE *configurationFile = fopen("pathFinding.net");
    fann_save(ann, configurationFile);
    fclose(configurationFile);
    fann_destroy(ann);

    return 0;
}