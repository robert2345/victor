#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>

typedef enum{
    ACTION_RUN,
    ACTION_TRAIN,
    ACTION_GATHER,
    ACTION_CLOSE,
    ACTION_NR_OF
} actionEnum;

bool ModelCheckTrainingOngoing();
void ModelSetup(const char *messageQueuePath, int size);
void ModelTrainAnn();
void ModelTakeDown();
void ModelExtractTrainingData();
void ModelGetRGB(int x, int y, double *red, double *green, double *blue);
double ModelGetTrainingResults(int x, int xMax);
void ModelRunAnn();
void ModelPrepareData();
void ModelRunReferenceAlgo();
void ModelRunSimpleAlgo();
void ModelSetStopTraining(bool value);
bool ModelGetStopTraining();

#endif
