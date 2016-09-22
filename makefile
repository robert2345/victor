NAME = pathfind
TESTNAME = $(NAME)_test

GTKLIBS = `pkg-config --libs gtk+-3.0`
CFLAGS += `pkg-config --cflags gtk+-3.0` -lm -fopenmp

SOURCE = $(wildcard ./src/*.c)

TESTSOURCE = $(wildcard ./src/test/*.c )
OBJS = ${$(SOURCE), .c=.o}

EXTRA_SOURCE = ./fann/src/

INCLUDE_DIRS = ./fann/src/include/ ./inc/

CFLAGS += -std=c99 
CFLAGS += $(foreach includedir,$(INCLUDE_DIRS),-I$(includedir))
LDFLAGS += $(addprefix -I, $(INCLUDE_DIRS) $(EXTRA_SOURCE))

all: $(NAME)

$(NAME): $(SOURCE) $(OBJS)
	$(CC) $(CFLAGS) $(SOURCE) -o $(NAME) $(LDFLAGS) $(GTKLIBS) 

debug: $(SOURCE) $(OBJS)
	$(CC) -g $(CFLAGS) $(SOURCE) -o $(NAME)_debug $(LDFLAGS) $(GTKLIBS) 
    
clean:
	rm -rf $(OBJS) $(NAME) $(TEST_OBJS) $(TESTNAME)

