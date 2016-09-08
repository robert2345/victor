#ifndef GUI_H
#define GUI_H

/*
 * Starts a GUI with buttons and a pixelated drawing area.
 */
void GUI_Start(void *dummy);

/*
 * Initialize the GUI context.
 * */
void GUI_Initialization(int argc, char *argv[], const char *messageQueuePath_p, int size);

/*
 * Trigger a redraw of the drawing area
 * */
void GUI_Redraw(void);

#endif
