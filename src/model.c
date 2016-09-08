#include <gui.h>
#include <model.h>
#include <time.h>
#include <math.h>
#include <doublefann.c>
#include <stdbool.h>
#include <model.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

// DEFINES
#define NR_OF_WAVES 6
#define COST_OF_ONE_STEP 0.05

#define ANN_NUM_OUTPUT 4
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


/* Internal Variables */
typedef struct
{
    // Data used as input to the pathfinding
    double intensity; // "Height" of the terrain.
    bool chosenByReference; // True menas this node is part of the path.
    bool chosenBySimple;
    bool chosenByAnn;
    
    bool visited; // allready processed by pathf. algo.
    double cost; // The minimum cost of reaching the goal from this node

    
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
    double amplitude; // 0-1
    double frequency; // revolutions per pixel
    double x; // origo of the wave
    double y; // origo of the wave
} wave;

static const char *g_messageQueuePath_p;
static wave g_waves[NR_OF_WAVES] = {0};
static struct fann_train_data *g_trainingData;
static struct fann *g_ann;
static char g_trainDataFilename[NAME_LENGTH];
static char g_configFilename[NAME_LENGTH];
static bool g_stopTraining = false;
static dataNode *theData;
static int g_size = 0;
static int g_annNumOfInputs = 0;

/* Function prototypes */
int PrintStatusAndConsiderStop(struct fann *ann, struct fann_train_data *train, 
                               unsigned int max_epochs, 
                               unsigned int epochs_between_reports, 
                               float desired_error, 
                               unsigned int epochs);

/* Internal functions */
static dataNode* GetNodePointer(int x, int y)
{
	return &theData[y * g_size + x];
}

static void ConfigureAnn()
{
    fann_set_activation_function_hidden(g_ann, FANN_SIGMOID);
    fann_set_activation_function_output(g_ann, FANN_SIGMOID);
    fann_set_training_algorithm(g_ann, ANN_TRAIN_ALGO);
    fann_set_learning_rate(g_ann, LEARNING_RATE);
    fann_set_train_error_function(g_ann, ERROR_FUNCTION); 
    fann_set_callback(g_ann, PrintStatusAndConsiderStop);
}

static void IntensityToInput(fann_type *input)
{
    for (int i = 0; i < g_size * g_size; i++)
    {
        input[i] = theData[i].intensity;
    }
}

static void DirectionToVector(int dir, int *x, int *y)
{
    int sign = 1 - (dir & 2);
    *x = ((~dir) & 1) * sign;
    *y = ((dir) & 1) * sign;
}

static double GetCostOfStep(double fromIntensity, double toIntensity)
{
	return COST_OF_ONE_STEP + fmax(fromIntensity - toIntensity, 0.0);
}

static double CalcCost(int parentX, int parentY, int thisX, int thisY)
{
    int deltaX;
    int deltaY;
    int nextX;
    int nextY;
    double currentCost;
    double parentIntensity = GetNodePointer(parentX, parentY)->intensity;

    dataNode *n = GetNodePointer(thisX, thisY);
    if (n->visited == false && (thisX != 0 || thisY != 0))
    {
        n->visited = true;
        /* Set init value so that a loop route will return extremely
        * high value and will not be chosen. */
        n->cost = DBL_MAX/2;
        for (int dir = 0; dir < 4; dir++)
        {
            DirectionToVector(dir, &deltaX, &deltaY);
            nextX = thisX + deltaX;
            nextY = thisY + deltaY;

            if ((nextX != parentX || nextY != parentY) &&
                (nextX >= 0) &&
                (nextY >= 0) &&
                (nextX < g_size) &&
                (nextY < g_size))
            {
                currentCost = CalcCost(thisX, thisY, nextX, nextY);
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
    return n->cost + GetCostOfStep(parentIntensity, n->intensity);
}

static bool InsideAndNotChosenByAnn(const int x, const int y)
{
    if ((x < g_size) &&
        (y < g_size) &&
        (x >= 0) &&
        (y >= 0) &&
        (GetNodePointer(x, y)->chosenByAnn == false))
    {
        return true;
    }
    else
    {
        return false;
    }
}

static double CalcSimpleCost(double fromIntensity, double toIntensity, double targetIntensity)
{
    double hightLossCost = 0;
    double stepcost = GetCostOfStep(fromIntensity, toIntensity);
    // dont go down if target is above us. We will suffer later.
    if (stepcost == COST_OF_ONE_STEP)
    {
        hightLossCost = COST_OF_ONE_STEP + GetCostOfStep(toIntensity, targetIntensity) - GetCostOfStep(fromIntensity, targetIntensity);
    }
    return fmax(stepcost, hightLossCost);
}

static void TakeOneStepWithAnn(fann_type *fann_input, int *x, int *y)
{
    fann_run(g_ann, fann_input);
    int direction = 0;
    int deltaX = 0;
    int deltaY = 0;
    fann_type value = g_ann->output[0];
    
    for (int i = 0; i < 4; i++)
    {
		fann_type tmp = g_ann->output[i];
        if (tmp >= value)
        {
			direction = i;
			value = tmp;
		}
    }
    DirectionToVector(direction, &deltaX, &deltaY);
    *x += deltaX;
    *y += deltaY;
}

/* External functions */
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
    if (ModelGetStopTraining())
    {
        ModelSetStopTraining(false);
        return -1;
    }
    
    return 0;
}

void ModelTrainAnn()
{
    fann_train_on_data(g_ann,
                       g_trainingData,
                       MAX_EPOCHS,
                       EPOCHS_BETWEEN_REPORTS,
                       DESIRED_ERROR);
}

void ModelTakeDown()
{
	int ret = 0;
	
	free(theData);
    
    // Save and destroy training data
    ret = fann_save_train(g_trainingData, g_trainDataFilename);
    if (ret == -1) perror("Save train");
    fann_destroy_train(g_trainingData);
    if (ret == -1) perror("Destroy train");

    // Save and destroy ANN.
    fann_save(g_ann, g_configFilename);
    if (ret == -1) perror("Save ANN");
    fann_destroy(g_ann);
    if (ret == -1) perror("Destroy ANN");
}

void ModelSetup(const char *messageQueuePath, int size)
{
    FILE *trainingFile;
    FILE *configFile;
    
    g_messageQueuePath_p = messageQueuePath;
    g_size = size;
    g_annNumOfInputs = (g_size * g_size + 2);

	theData = (dataNode*)calloc(g_size * g_size, sizeof(dataNode));
	if (theData == NULL)
	{
		printf("Allocation of memory to the data failed\n");
		exit (1);
	}

	printf("About to create the ANN\n");
    // Open the neuron network config file.
    snprintf(g_configFilename, NAME_LENGTH, ANN_FILENAME, g_annNumOfInputs, ANN_NUM_OUTPUT);
    g_ann = fann_create_from_file(g_configFilename);
    if (g_ann == NULL)
    {
		int annNumHiddenLayer1 = (g_annNumOfInputs);
		int annNumHiddenLayer2 = (annNumHiddenLayer1 / 3);
		int annNumHiddenLayer3 = (ANN_NUM_OUTPUT * 4);
		
        // Unable to load existing ann. Creating a new one.
        g_ann = fann_create_standard(ANN_NUM_LAYERS,
                                   g_annNumOfInputs,
                                   annNumHiddenLayer1,
                                   annNumHiddenLayer2,
                                   annNumHiddenLayer3,
                                   ANN_NUM_OUTPUT);

        ConfigureAnn();
    }

    printf("About to create the training data\n");
    // Open the training data file.
    snprintf(g_trainDataFilename, NAME_LENGTH, TRAIN_DATA_FILENAME, g_annNumOfInputs, ANN_NUM_OUTPUT);
    g_trainingData = fann_read_train_from_file(g_trainDataFilename);
    if (g_trainingData == NULL)
    {
        // There was no training data file, so creat new training data.
        g_trainingData = fann_create_train(0,
                                           g_annNumOfInputs,
                                           ANN_NUM_OUTPUT); // Lets start with an empty data set and then merge a new one for each trial. Perhaps slow but lets see.
    }
    else
    {
		printf("Loaded %d training sets\n", g_trainingData->num_data);
	}
}

// For one solution recorded - extract data for future ANN training;
void ModelExtractTrainingData()
{
    // Go throught the selected nodes and add trainingdata
    int x = g_size - 1;
    int y = g_size - 1;
    int currDiag;
    double likelyhood;
    dataNode * n = GetNodePointer(x, y);

    struct fann_train_data *tmpTrainingData = fann_create_train(1, g_annNumOfInputs, ANN_NUM_OUTPUT);

    while(n->route_x != 0 || n->route_y != 0)
    {       
        /* Likelyhood of point being included in the dataset is 100%
         * on the diagonal and 1/ numberOfPointsOnDiagonal at the
         * start and end. */
        
        if (x + y > (g_size - 1))
        {
            currDiag = fmax(abs(g_size - 1 - x), (g_size - 1 - y)); 
        }
        else
        {
            currDiag = fmax(x, y); 
        }
        
        likelyhood = hypot(currDiag, currDiag) / hypot(g_size - 1 , g_size - 1); 
        
        double randomNum = (double)rand() / RAND_MAX;

        if ((randomNum) < likelyhood)
        {
            IntensityToInput(tmpTrainingData->input[0]);
            tmpTrainingData->input[0][g_size * g_size] = x;
            tmpTrainingData->input[0][g_size * g_size + 1] = y;
            tmpTrainingData->output[0][0] = 0;
            tmpTrainingData->output[0][1] = 0;
            tmpTrainingData->output[0][2] = 0;
            tmpTrainingData->output[0][3] = 0;
            tmpTrainingData->output[0][n->dir] = 1.0;

            // Merge the training data with existing set.
            g_trainingData = fann_merge_train_data(g_trainingData, tmpTrainingData);
        }
        x += n->route_x;
        y += n->route_y;
        n = GetNodePointer(x, y);
    }

    fann_destroy_train(tmpTrainingData);
}

void ModelGetRGB(int x, int y, double *red, double *green, double *blue)
{
	dataNode *dat = GetNodePointer(x, y);
	
	// Choose the color of the pixel from the paths found.
	if (dat->chosenByReference || dat->chosenByAnn || dat->chosenBySimple)
	{
		*red = (double)dat->chosenByReference;
		*green = (double)dat->chosenByAnn;
		*blue = (double)dat->chosenBySimple;
	}
	else
	{
		*red = dat->intensity;
		*green = dat->intensity;
		*blue = dat->intensity;
	}
}

void ModelRunAnn()
{
    fann_type fann_input[g_annNumOfInputs];
    IntensityToInput(fann_input);
    int x = g_size - 1;
    int y = g_size - 1;
    double cost = 0;
    dataNode *n = GetNodePointer(x, y);
	double_t intensityOfPreviousNode = n->intensity;
    while (InsideAndNotChosenByAnn(x,y))
    {
		cost += GetCostOfStep(intensityOfPreviousNode, n->intensity);
        n->chosenByAnn = true;
        if (x == 0 && y == 0)
        {
            printf("Ann cost is %lf\n", cost);
            break;
        }
        fann_input[g_size * g_size] = (fann_type)x;
        fann_input[g_size * g_size + 1] = (fann_type)y;
        TakeOneStepWithAnn(fann_input, &x,&y);
        n = GetNodePointer(x, y);
    }
}

void ModelPrepareData()
{
#define START_INTENSITY 0.5
    double maxIntensity = -1000;
    double minIntensity = 10000;
    for (int w = 0; w < NR_OF_WAVES; w++)
    {
        srand(time(NULL)); 
        g_waves[w].frequency = (double)rand() / RAND_MAX / 2;
        g_waves[w].amplitude = (double)rand() / RAND_MAX;
        g_waves[w].x = (double)rand() / RAND_MAX * 2 * g_size;
        g_waves[w].y = (double)rand() / RAND_MAX * 2 * g_size;
    }
    
    memset(theData, 0, g_size * g_size * sizeof(dataNode));
    for (int i = 0; i < g_size; i++)
    {
        for (int j = 0; j < g_size; j++)
        {
			dataNode *n = GetNodePointer(i, j);
            n->intensity = START_INTENSITY;
            for (int w = 0; w < NR_OF_WAVES; w++)
            {
                srand(time(NULL));
                double distanceToSignalOrigo = sqrt(pow(g_waves[w].x - i, 2.0) +
                                                    pow(g_waves[w].y - j, 2.0));
                n->intensity += g_waves[w].amplitude *
                                sin(distanceToSignalOrigo *
                                g_waves[w].frequency);
                maxIntensity = fmax(maxIntensity, n->intensity);
                minIntensity = fmin(minIntensity, n->intensity);
            }
        }
    }
    
    //Scale the visible data
    for (int i = 0; i < g_size; i++)
    {
        for (int j = 0; j < g_size; j++)
        {
			dataNode *n = GetNodePointer(i, j);
            n->intensity = (n->intensity - minIntensity) /
                           (maxIntensity - minIntensity); 
        }
    }
}

void ModelRunReferenceAlgo()
{
    // Find cost of each path
    CalcCost(g_size - 1,
             g_size - 1,
             g_size - 1,
             g_size - 1);

    // Go throught the selected nodes.
    // Mark them and print the cost of the path from bottom right.
    int x = g_size - 1;
    int y = x;
    dataNode * n = GetNodePointer(x, y);

    while(n->route_x != 0 || n->route_y != 0)
    {
        n->chosenByReference = true;
        x += n->route_x;
        y += n->route_y;
        n = GetNodePointer(x, y);
    }
    printf("Reference cost is %lf\n",
	       GetNodePointer(g_size - 1, g_size - 1)->cost);
}

void ModelRunSimpleAlgo()
{
    int x = g_size - 1;
    int y = g_size - 1;
    dataNode *n = GetNodePointer(x, y);
    double targetIntensity = n->intensity;
    double totalCost = 0.0;
    n->chosenBySimple = true;
    while (x != 0 || y != 0)
    {
        double fromIntensity = n->intensity;
        double upIntensity = GetNodePointer(x, y - 1)->intensity;
        double leftIntensity = GetNodePointer(x - 1, y)->intensity;
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
            double costOfUp = CalcSimpleCost(fromIntensity,
                                      upIntensity,
                                      targetIntensity);
            double costOfLeft = CalcSimpleCost(fromIntensity,
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
        totalCost += GetCostOfStep(fromIntensity, toIntensity);
        n = GetNodePointer(x, y);
        n->chosenBySimple = true;
    }
    printf("Simple cost is: %lf\n", totalCost);
}

void ModelSetStopTraining(bool value)
{
	g_stopTraining = value;
}

bool ModelGetStopTraining()
{
	return g_stopTraining;
}
