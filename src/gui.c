#include <gui.h>
#include <mqueue.h>
#include <fcntl.h> // O_ constants...
#include <gtk/gtk.h>
#include <model.h>

struct args{
    int argc;
    char ** argv;
};

actionEnum actions[ACTION_NR_OF] =
{
    [ACTION_RUN] = ACTION_RUN,
    [ACTION_TRAIN] = ACTION_TRAIN,
    [ACTION_GATHER] = ACTION_GATHER,
    [ACTION_CLOSE] = ACTION_CLOSE
};

static struct args arguments = {0};
static GtkWidget *g_drawingArea;
static const char *g_messageQueuePath_p;
static int g_size = 30; // Size of one dimension of the square drawing area.
static int g_elementSize = 10;

static gboolean DeleteEvent(GtkWidget *widget,
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

static void AbortTraining(void)
{
	ModelSetStopTraining(TRUE);
}

static gboolean DrawCallback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    int width, height;
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);
    double red;
    double green;
    double blue;

    gtk_render_background(context, cr, 0, 0, width, height);
    for (int i = 0; i < g_size; i++)
    {
        for(int j = 0; j < g_size; j++)
        {
			ModelGetRGB(i, j, &red, &green, &blue);
			cairo_set_source_rgb(cr,
								 red,
								 green,
								 blue);
		
            cairo_rectangle(cr,
                            i * g_elementSize,
                            j * g_elementSize,
                            i * g_elementSize + g_elementSize,
                            j * g_elementSize + g_elementSize);
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


static void SendMessages(GtkWidget *clickedWidget, actionEnum *actionToTake)
{
    mqd_t messageQueue;
    int ret;
    
    messageQueue = mq_open(g_messageQueuePath_p, O_WRONLY);
    if (messageQueue == -1) perror("SendMessages mq_open");
    
    ret = mq_send(messageQueue, (char*)actionToTake, sizeof(actionEnum), 0);
    if (ret == -1) perror("SendMessages mq_send");
    
    if (*actionToTake == ACTION_CLOSE)
    {
        gtk_main_quit();
    }
}

void GUI_Redraw()
{
	gtk_widget_queue_draw(g_drawingArea); // IS THIS SAFE IN MULTITHREAD?
}

void GUI_Initialization(int argc, char *argv[], const char *messageQueuePath, int size)
{
    arguments.argc = argc;
    arguments.argv = argv;
    g_messageQueuePath_p = messageQueuePath;
	g_size = size;
	g_elementSize = (600 / size);
}

void GUI_Start(void *dummy)
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
                      G_CALLBACK(DeleteEvent), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(destroy), NULL);

    // Create the drawing area on which we will paint terrain and paths.
    g_drawingArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(g_drawingArea,
								g_size * g_elementSize,
								g_size * g_elementSize);
    gtk_widget_add_events(g_drawingArea,
                          GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(g_drawingArea), "draw", G_CALLBACK(DrawCallback), NULL);

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
                     G_CALLBACK(SendMessages),
                     &actions[ACTION_RUN]);
    g_signal_connect(gatherDataButton, "clicked",
                     G_CALLBACK(SendMessages),
                     &actions[ACTION_GATHER]);
    g_signal_connect(trainButton, "clicked",
                     G_CALLBACK(SendMessages),
                     &actions[ACTION_TRAIN]);
    g_signal_connect(closeButton, "clicked",
                     G_CALLBACK(SendMessages),
                     &actions[ACTION_CLOSE]);
    g_signal_connect(stopButton, "clicked",
                     G_CALLBACK(AbortTraining),
                     NULL);          

    // Pack the buttons and field into the box. top to bottom:
    gtk_box_pack_start(GTK_BOX(box), runAnnButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), gatherDataButton, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(box), trainButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), stopButton, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), closeButton, FALSE, FALSE, 3);
    /* This packs the  box and drawingBox into the window (a gtk container). */
    gtk_box_pack_start(GTK_BOX(drawingBox), box, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(drawingBox), g_drawingArea, FALSE, FALSE, 3);
    gtk_container_add(GTK_CONTAINER (window), drawingBox);
	
    gtk_widget_show_all(window);
    
    gtk_main();
}
