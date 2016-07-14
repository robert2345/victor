#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// DEFINES
#define SIZE 100
#define ELEMENT_SIZE 6
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
				}
			}

        }
    }
    return n->cost + fmax(parentIntensity - n->intensity, 0);
}

static void prepareData()
{
	memset(theData, 0, sizeof(theData));
    for (int i = 0; i < SIZE; i++)
    {
        for(int j = 0; j < SIZE; j++)
        {
            theData[i][j].intensity = 0.5;
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

	context = gtk_widget_get_style_context (widget);

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	gtk_render_background (context, cr, 0, 0, width, height);
	for (int i = 0; i < SIZE; i++)
	{
		for(int j = 0; j < SIZE; j++)
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

	// Go throught the selected nodes and mark them.
    int x = SIZE - 1;
    int y = x;
    dataNode *n = &theData[x][y];
    while(n->route_x != 0 || n->route_y != 0)
    {
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
    GtkWidget *drawing_area;
    GtkWidget *box;
    GtkWidget *drawingBox;


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
    
    /* Creates a new button with the label "Hello World". */
    button = gtk_button_new_with_label ("Generate\nand\ncalculate");
    
    for (int i = 0; i < NR_OF_WAVES; i++)
    {
        waveEntries[i].frequencyBuffer = gtk_entry_buffer_new("0,02", 4);
        waveEntries[i].frequencyEntry = gtk_entry_new_with_buffer(waveEntries[i].frequencyBuffer);
    }
    
    g_signal_connect(button, "clicked",
		      G_CALLBACK (calculateAndDraw), drawing_area);
    
    
    /* This packs the button into the window (a gtk container). */
    gtk_container_add (GTK_CONTAINER (window), drawingBox);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 3);
    for(int i = 0; i < NR_OF_WAVES; i++)
    {
        gtk_box_pack_start (GTK_BOX (box), waveEntries[i].frequencyEntry, FALSE, FALSE, 3);
    }
    gtk_box_pack_start (GTK_BOX (drawingBox), box, FALSE, FALSE, 3);
    gtk_box_pack_start (GTK_BOX (drawingBox), drawing_area, FALSE, FALSE, 3);

    gtk_widget_show_all (window);
    gtk_main ();
    
    return 0;
}
