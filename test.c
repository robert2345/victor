#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>

#define size 100
#define elementSize 8
#define NR_OF_WAVES 3

typedef struct
{
    double intensity;
    int route_x;
    int route_y;
    gboolean visited;
    double cost;
    gboolean chosen;
} dataNode;

static dataNode theData[size][size] = {0};


typedef struct
{
    double amplitude; // 0-1
    double frequency; // revolutions per pixel
    double x; // origo of the wave
    double y; //  origo of the wave
    int dampening; // how much is left of the signal after one revolution. 1 means all. 0 means nothing.
} wave;

static char bokstav;

static double calcCost(int parentX, int parentY, int parentRX, int parentRY, double parentIntensity)
{
    int thisX = parentX + parentRX;
    int thisY = parentY + parentRY;
    //printf("Started checking node %d, %d\n", thisX, thisY);
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

            //printf("r_x %d\n", r_x);
            //printf("r_y %d\n", r_y);
                if ((r_x != -parentRX || r_y != -parentRY) &&
                    (thisX + r_x >= 0) &&
                    (thisY + r_y >= 0) &&
                    (thisX + r_x < size) &&
                    (thisY + r_y < size))
                {
                    //printf("About to investigate %d, %d\n", thisX + r_x, thisY + r_y);
                    double currentCost = calcCost(thisX, thisY, r_x, r_y, n->intensity);
                    if(n->cost > currentCost)
                    {
                        n->cost = currentCost;
                        n->route_x = r_x;
                        n->route_y = r_y;
                    }
                }
                //scanf("Stop\n");
        }
        //printf("Checked %d, %d and found lowest cost %lf\n", thisX, thisY, n->cost);
        //scanf("Stop\n");
    }
    /*if (n->cost < 10000){
    printf("Checked %d, %d and found lowest cost %lf from %d, %d\n", thisX, thisY, n->cost, thisX+n->route_x, thisY+n->route_y);
    }*/
    //scanf("\n%c",&bokstav);
    return n->cost + fmax(parentIntensity - n->intensity, 0);
}

static void prepareData(wave* waves, int length)
{
    for (int i = 0; i < size; i++)
    {
        for(int j = 0; j < size; j++)
        {
            theData[i][j].intensity = 0.5;
            for (int w = length-1; w >= 0; w--)
    	    {
                double distanceToSignalOrigo = sqrt(pow(waves[w].x - i, 2.0) + pow(waves[w].y - j, 2.0));
                theData[i][j].intensity += waves[w].amplitude * sin(distanceToSignalOrigo * waves[w].frequency);
                //printf("The Data at %d, %d is %lf\n", i, j, theData[i][j]);
            }
        }
    }
}

/* This is a callback function. The data arguments are ignored
 * in this example. More on callbacks below. */
static void hello( GtkWidget *widget,
                   gpointer   data )
{
    g_print ("Hello World\n");
}

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    /* If you return FALSE in the "delete-event" signal handler,
     * GTK will emit the "destroy" signal. Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit?'
     * type dialogs. */

    g_print ("delete event occurred\n");

    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete-event". */

    return TRUE;
}

/* Another callback */
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
        theX = (int)buttonPress->x/elementSize;
        theY = (int)buttonPress->y/elementSize;
	if (theX >= size*elementSize || theY >= size*elementSize || theX <0 || theY < 0)
	{
            printf("Out of bounds! theX: %d, theY:%d, click X:%lf, Y:%lf\n", theX, theY, buttonPress->x, buttonPress->y);
	}
	else
	{
            printf("theX: %d, theY:%d, click X:%lf, Y:%lf\n", theX, theY, buttonPress->x, buttonPress->y);
            if (theData[theX][theY].intensity == 0)
	    {
	        theData[theX][theY].intensity = 1;
            }else{
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

  context = gtk_widget_get_style_context (widget);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
    for (int i = 0; i < size; i++)
    {
        for(int j = 0; j < size; j++)
        {
            double intensity = theData[i][j].intensity;
            if (theData[i][j].chosen)
{
            cairo_set_source_rgb(cr, 1, 0, 0);
}
else
{
 cairo_set_source_rgb(cr, intensity*0.5, intensity, intensity*1.75);
}
            cairo_rectangle(cr,
                            i*elementSize,
                            j*elementSize,
                            i*elementSize+elementSize,
                            j*elementSize+elementSize);
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

int main( int   argc,
          char *argv[] )
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *drawing_area;
    GtkWidget *box;

    int length = NR_OF_WAVES;
    wave waves[NR_OF_WAVES] = {
        {.amplitude = 0.4, .frequency = 0.15, .x = 0, .y = -size},
        {.amplitude = 0.25, .frequency = 0.3, .x = size / 3, .y = -3},
        {.amplitude = 0.15, .frequency = 0.52, .x = size / 4, .y = size / 2}};

    printf("Preparing the data\n");
    prepareData(waves, length);
    printf("Calculating the cost...\n");
    printf("The cost from size, size to 0,0 is %lf\n", calcCost(size-1 ,size-1,0,0,theData[size-1][size-1].intensity));    
    int x = size - 1;
    int y = x;
    dataNode *n = &theData[x][y];
    printf("Route node: %d, %d\n", x,y);
    while(n->route_x != 0 || n->route_y != 0)
    {
       printf("Route node: %d, %d\n", x,y);
       n->chosen = TRUE;
       x += n->route_x;
       y += n->route_y;
       n = &theData[x][y];
    }
    /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    gtk_init (&argc, &argv);
    
    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, size*elementSize, size*elementSize);
    gtk_widget_add_events (drawing_area,
                           GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(draw_callback), NULL);

    g_signal_connect (drawing_area, "button_press_event", G_CALLBACK (drawing_clicked), NULL);
    
    /* When the window is given the "delete-event" signal (this is given
     * by the window manager, usually by the "close" option, or on the
     * titlebar), we ask it to call the delete_event () function
     * as defined above. The data passed to the callback
     * function is NULL and is ignored in the callback function. */
    g_signal_connect (window, "delete-event",
		      G_CALLBACK (delete_event), NULL);
    
    /* Here we connect the "destroy" event to a signal handler.  
     * This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return FALSE in the "delete-event" callback. */
    g_signal_connect (window, "destroy",
		      G_CALLBACK (destroy), NULL);
    
    /* Sets the border width of the window. */
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
    
    /* Creates a new button with the label "Hello World". */
    button = gtk_button_new_with_label ("Hello World");
    
    /* When the button receives the "clicked" signal, it will call the
     * function hello() passing it NULL as its argument.  The hello()
     * function is defined above. */
    g_signal_connect (button, "clicked",
		      G_CALLBACK (hello), NULL);
    
    /* This will cause the window to be destroyed by calling
     * gtk_widget_destroy(window) when "clicked".  Again, the destroy
     * signal could come from here, or the window manager. */
    g_signal_connect_swapped (button, "clicked",
			      G_CALLBACK (gtk_widget_destroy),
                              window);
    
    /* This packs the button into the window (a gtk container). */
    gtk_container_add (GTK_CONTAINER (window), box);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 3);
    gtk_box_pack_start (GTK_BOX (box), drawing_area, FALSE, FALSE, 3);
    /* The final step is to display this newly created widget. */
    //gtk_widget_show (button);
    //gtk_widget_show(drawing_area);
    
    /* and the window */
    //gtk_widget_show (window);
    gtk_widget_show_all (window);
    /* All GTK applications must have a gtk_main(). Control ends here
     * and waits for an event to occur (like a key press or
     * mouse event). */
    gtk_main ();
    
    return 0;
}
