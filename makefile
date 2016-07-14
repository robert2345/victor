LIBS = `pkg-config --libs gtk+-3.0`

CFLAGS = `pkg-config --cflags gtk+-3.0` -lm

all: test.c
	c99 test.c $(LIBS) $(CFLAGS)
