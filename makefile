TOOL = c99
LIBS = -L ~/repo/victor/fann/src
LIB_FILES = -l:libdoublefann.a -l:libfann.a 
GTKLIBS = `pkg-config --libs gtk+-3.0`
CFLAGS = `pkg-config --cflags gtk+-3.0` -lm -fopenmp


SOURCES = ./src/*.c
EXTRA_SOURCE = ./fann/src/
INCLUDE_DIRS = ./inc/ ./fann/src/include/
HEADERS = $(addsuffix *.h, $(INCLUDE_DIRS))
INCLUDES = $(addprefix -I, $(INCLUDE_DIRS))

OBJS = $(SOURCES:.c=.o)


#Make targets for all object files
#($ foreach
#$(OBJS):$(HEADERS)
	#$(TOOL) $(%: .o=.c)


all: $(SOURCES) $(HEADERS)
	#cmake ./fann/.
	#make ./fann/
	$(TOOL) $(SOURCES) -I$(EXTRA_SOURCE) $(INCLUDES) $(GTKLIBS) $(CFLAGS)
