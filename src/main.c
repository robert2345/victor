#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h> // O_ constants...
#include <errno.h> // Just to be able to clear last error. Remove this include later.
#include <gui.h>
#include <model.h>

#define SIZE 35
const char messageQueuePath[] = "/pathFinderMessageQueue1";

static void TryAlgorithms()
{
    // Generate the data
    ModelPrepareData();

    // Run the reference algorithm
    ModelRunReferenceAlgo();
    
    // Run the simple algorithm
    ModelRunSimpleAlgo();

    // Run the Neural Network
    ModelRunAnn();

    // Redraw
    GUI_Redraw();
}

static void GatherTrainingData()
{
    for (int i = 0; i < 10; i++)
    {
        // Generate the data
        ModelPrepareData();

        // Run the reference algorithm
        ModelRunReferenceAlgo();

        // Extract training data from the results
        ModelExtractTrainingData();
        
        // Redraw the image
        GUI_Redraw();
    }
}


static void ReceiveMessages(void *dummy)
{
    mqd_t messageQueue;
    bool receiveMessages = true;
    int ret = 0;
    
    typedef union
    {
        actionEnum action;
        uint8_t buf[10 * sizeof(actionEnum)];
    } messageBuf;
    
    messageBuf message;
    
    messageQueue = mq_open(messageQueuePath, (O_RDONLY));
    if (messageQueue == -1)
    {            
        perror("Opening message queue in Model:");
    }
    
    while(receiveMessages)
    {
        ret = mq_receive(messageQueue, (char*)&message.buf[0], sizeof(message), NULL);
        if (ret == -1)
        {
            perror("Model");    
        }
        else
        {
            switch (message.action)
            {
                case ACTION_CLOSE:
                    receiveMessages = false;
                    break;
                case ACTION_GATHER:
                    GatherTrainingData();
                    break;
                case ACTION_TRAIN:
                    ModelTrainAnn();
                    break;
                case ACTION_RUN:
                    TryAlgorithms();
                    break;
            }
        }
    }
    
    mq_close(messageQueue);
}



int main(int argc, char *argv[])
{
    pthread_t guiThread;
    pthread_t controllerThread;
    int ret;
    mqd_t messageQueue;
    struct mq_attr attr;

    GUI_Initialization(argc, argv, messageQueuePath, SIZE);
    ModelSetup(messageQueuePath, SIZE);
    
    attr.mq_flags = 0;  
    attr.mq_maxmsg = 10;  
    attr.mq_msgsize = sizeof(actionEnum);  
    attr.mq_curmsgs = 0;
    messageQueue = mq_open(messageQueuePath, (int)(O_CREAT | O_RDWR), 0666, &attr);
    
    if (messageQueue == -1)
    {
        perror("main: ");
    }
    else
    {
        ret |= pthread_create(&guiThread, NULL, (void *)GUI_Start, NULL);
        ret |= pthread_create(&controllerThread, NULL, (void *)ReceiveMessages, NULL);

        if (ret == 0)
        {
            pthread_join(guiThread, NULL);
            pthread_join(controllerThread, NULL);
        }
        else
        {
            printf("Something went wrong. Start guessing...\n");
        }
    }
    
    ret = mq_close(messageQueue);
    if (ret == -1) perror("Closing mq");

    ModelTakeDown();
    
    return 0;
}
