#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>
#include "list.h"

#define MAX_LEN 40
#define NORM_PRIO_MIN_QUANTUM 3
#define LOW_PRIO_MIN_QUANTUM 7


typedef struct PCBStruct {
    int PID;
    int priority;
    int state;
    char* replyMsg;
    //-1 for not blocked by semaphore
    //0-4 for the ID of the current semaphore
    int blockingSemaphoreID;
    bool unblockedRecv;
}ProcessControlBlock;

typedef struct SemaphoreStruct {
    bool inUse;
    int value;
} Semaphore;

typedef struct msgStruct {
    int sourcePID;
    int destinationPID;
    char* msg;
}Message;

enum ProcessState {
    PROCESS_READY,
    PROCESS_BLOCKED,
    PROCESS_RUNNING
};

enum ProcessPriority {
    HIGH_PRIORITY,
    NORMAL_PRIORITY,
    LOW_PRIORITY,
    INIT_PROCESS
};

Semaphore semaphores[5];
List* highPrio;
List* normPrio;
List* lowPrio;
List* sendWait;
List* recvWait;
List* semaphoreBlocked;
List* waitingMessages;
int PIDNumbers;
ProcessControlBlock* currentlyRunning = NULL;
char* initRepliedTo;
int normQuantumCheck;
int lowQuantumCheck;


void initialization();
int priorityManager();
void runInit();
bool inputOperator();
int createProcess(int priority);
int generatePID();
void processMadeOutput(char* command, int returnCheck);
void freeProcess(void* pItem);
void freeMessage(void* pItem);
bool comparatorRecvMsgPID(void* pItem, void* pComparisonArg);
bool comparatorPCBPID(void* pItem, void* pComparisonArg);
bool comparatorSemaphoreID(void* pItem, void* pComparisonArg);
int queueSearch(int processPID);
bool createMessage(char* msg, int destPID);
void getNextProcess(int priority);
bool moveProcess(int destination, int semaphoreID, ProcessControlBlock* process);
void totalInfo();

int main(){
    bool status = true;
    initialization();
    while (status){
        printf("Enter Command: ");
        fflush(stdin);
        status = inputOperator();
    }
    return 0;
}

void initialization(){
    highPrio = List_create();
    normPrio = List_create();
    lowPrio = List_create();
    sendWait = List_create();
    recvWait = List_create();
    semaphoreBlocked = List_create();
    waitingMessages = List_create();
    normQuantumCheck = NORM_PRIO_MIN_QUANTUM;
    lowQuantumCheck = LOW_PRIO_MIN_QUANTUM;
    PIDNumbers = 0;

    runInit();
    initRepliedTo = NULL;

    for(int i = 0; i < 5; i++){
        semaphores[i].inUse = false;
        semaphores[i].value = 0;
    }
}

int priorityManager(){
    if(normQuantumCheck == 0){
        normQuantumCheck = NORM_PRIO_MIN_QUANTUM;
        return NORMAL_PRIORITY;
    }
    if (lowQuantumCheck == 0){
        lowQuantumCheck = LOW_PRIO_MIN_QUANTUM;
        return LOW_PRIORITY;
    }
    normQuantumCheck--;
    lowQuantumCheck--;
    return HIGH_PRIORITY;
}

void runInit(){
    ProcessControlBlock *newProcess;
    newProcess = malloc(sizeof(struct PCBStruct));

    newProcess->PID = 0;
    newProcess->priority = INIT_PROCESS;
    newProcess->state = PROCESS_RUNNING;
    if (initRepliedTo != NULL){
        printf("Init process, reply received: %s", initRepliedTo);
        free(initRepliedTo);
        initRepliedTo = NULL;
    }
    newProcess->replyMsg = NULL;

    currentlyRunning = newProcess;
}

bool inputOperator(){
    char input[MAX_LEN];
    int intInput;
    scanf("%s", input);
    int returnCheck;
    switch (toupper(input[0])) {
        //Create
        case 'C': {
            printf("Enter priority: ");
            scanf("%d", &intInput);
            if ((intInput > 2) || (intInput < 0)) {
                printf("Create: Invalid input\n");
                return true;
            }
            returnCheck = createProcess(intInput);
            processMadeOutput("Create", returnCheck);
            return true;
        }
        //Fork
        case 'F': {
            if (currentlyRunning->priority == INIT_PROCESS) {
                printf("Fork: Cannot fork init process\n");
                return true;
            }
            returnCheck = createProcess(currentlyRunning->priority);
            processMadeOutput("Fork", returnCheck);
            return true;
        }
        //Kill
        case 'K': {
            printf("Enter PID of process you want to kill:");
            int tempPID;
            int listNumber;
            scanf("%d", &tempPID);
            listNumber = queueSearch(tempPID);
            if (listNumber == -1){
                printf("Kill: PID not found\n");
                return true;
            }
            ProcessControlBlock* tempPCB = NULL;
            List *tempList = NULL;
            switch (listNumber) {
                case 0:
                    tempList = highPrio;
                    break;
                case 1:
                    tempList = normPrio;
                    break;
                case 2:
                    tempList = lowPrio;
                    break;
                case 3:
                    tempList = sendWait;
                    break;
                case 4:
                    tempList = recvWait;
                    break;
                case 5:
                    tempList = semaphoreBlocked;
                    break;
            }
            tempPCB = List_remove(tempList);
            List_first(waitingMessages);
            while (List_search(waitingMessages, comparatorRecvMsgPID, tempPCB) != NULL){
                freeMessage(List_remove(waitingMessages));
            }
            if (tempPCB == currentlyRunning){
                getNextProcess(priorityManager());
            }
            freeProcess(tempPCB);
            printf("Kill process (PID: %d) successful\n", tempPID);
            return true;
        }
        //Exit
        case 'E': {
            if (currentlyRunning->priority == INIT_PROCESS) {
                List_free(highPrio, freeProcess);
                List_free(normPrio, freeProcess);
                List_free(lowPrio, freeProcess);
                List_free(waitingMessages, freeMessage);
                List_free(sendWait, freeProcess);
                List_free(recvWait, freeProcess);
                List_free(semaphoreBlocked, freeProcess);
                freeProcess(currentlyRunning);
                return false;
            }
            List_first(waitingMessages);
            while (List_search(waitingMessages, comparatorRecvMsgPID, currentlyRunning) != NULL) {
                freeMessage(List_remove(waitingMessages));
            }
            freeProcess(currentlyRunning);
            getNextProcess(priorityManager());
            return true;
        }
        //Quantum
        case 'Q':
        {
            if (currentlyRunning->priority != INIT_PROCESS) {
                if (!moveProcess(currentlyRunning->priority, -1, currentlyRunning)){
                    printf("Quantum: Out of Nodes kill or exit a process first\n");
                    return true;
                }
            } else {
                free(currentlyRunning);
                currentlyRunning = NULL;
            }
            getNextProcess(priorityManager());
            return true;
        }
        //Send
        case 'S':
        {
            if (currentlyRunning->priority == INIT_PROCESS){
                printf("Send: Init Process cannot send, Init only runs when there are no other processes\n");
                return true;
            }
            printf("Enter PID of process you want the message sent to:");
            char *tempMsg = malloc(sizeof(char) * MAX_LEN + 1);
            int tempPID;
            int listNumber;
            scanf("%d", &tempPID);
            listNumber = queueSearch(tempPID);
            if (listNumber == -1){
                printf("Send: PID not found\n");
                free(tempMsg);
                return true;
            } else if (listNumber == 4){
                ProcessControlBlock* tempPCB = NULL;
                tempPCB = List_remove(recvWait);
                tempPCB->unblockedRecv = true;
                moveProcess(tempPCB->priority, -1, tempPCB);
                printf("PID %d: Unblocked and back on ready queue\n", tempPID);
            }
            printf("Type message (%d char max):", MAX_LEN);
            fgetc(stdin);
            fgets(tempMsg, 40, stdin);
            if (!createMessage(tempMsg, tempPID)){
                printf("Send: Message could not be created, exit or kill and try again\n");
                free(tempMsg);
                return true;
            }
            printf("Message sent! Blocking current process (PID: %d)\n", currentlyRunning->PID);
            if (!moveProcess(3, -1, currentlyRunning)) {
                printf("Send: Out of Nodes for process\n");
                free(tempMsg);
                return true;
            }
            getNextProcess(priorityManager());
            free(tempMsg);
            return true;
        }
        //Receive
        case 'R':
        {
            if (currentlyRunning->priority == INIT_PROCESS){
                printf("Receive: Init Process cannot receive, Init only runs when there are no other processes\n");
                return true;
            }
            Message* tempMsg = NULL;
            List_first(waitingMessages);
            tempMsg = List_search(waitingMessages, comparatorRecvMsgPID, currentlyRunning);
            if (tempMsg == NULL){
                printf("No current messages to receive, blocking process (PID: %d)\n", currentlyRunning->PID);
                if (!moveProcess(4, -1, currentlyRunning)){
                    printf("Receive: Out of Nodes for process\n");
                    return true;
                }
                getNextProcess(priorityManager());
            } else {
                List_remove(waitingMessages);
                printf("Message found from PID: %d : %s", tempMsg->sourcePID, tempMsg->msg);
                freeMessage(tempMsg);
                tempMsg = NULL;
            }
            return true;
        }
        //Reply
        case 'Y':
        {
            printf("Enter PID of process you want to reply to:");
            char *tempMsg = malloc(sizeof(char) * MAX_LEN + 1);
            int tempPID;
            int listNumber;
            scanf("%d", &tempPID);
            listNumber = queueSearch(tempPID);
            if (listNumber == -1){
                printf("Reply: PID not found\n");
                free(tempMsg);
                return true;
            }
            ProcessControlBlock* tempPCB = NULL;
            if (listNumber == 3){
                tempPCB = List_remove(sendWait);
                moveProcess(tempPCB->priority, -1, tempPCB);
                printf("PID %d: Unblocked and back on ready queue\n", tempPID);
            } else {
                List *tempList = NULL;
                switch (listNumber) {
                    case 0:
                        tempList = highPrio;
                        break;
                    case 1:
                        tempList = normPrio;
                        break;
                    case 2:
                        tempList = lowPrio;
                        break;
                    case 4:
                        tempList = recvWait;
                        break;
                    case 5:
                        tempList = semaphoreBlocked;
                        break;
                }
                tempPCB = List_curr(tempList);
                if (tempPCB == NULL) {
                    printf("Reply: Something is very wrong");
                    free(tempMsg);
                    return false;
                }
            }
            printf("Type reply (%d char max):", MAX_LEN);
            fgetc(stdin);
            fgets(tempMsg, 40, stdin);
            if (tempPCB->replyMsg == NULL){
                tempPCB->replyMsg = malloc(sizeof(char ) * MAX_LEN + 1);
            }
            strcpy(tempPCB->replyMsg, tempMsg);
            printf("Reply successful! Sent to PID: %d\n", tempPID);
            free(tempMsg);
            return true;
        }
        //New Semaphore
        case 'N':
        {
            printf("Enter an ID for semaphore (0-4):");
            int semaphoreID;
            int value;
            scanf("%d", &semaphoreID);
            if (semaphores[semaphoreID].inUse){
                printf("Semaphore with ID:%d already exists\n", semaphoreID);
                return true;
            }
            semaphores[semaphoreID].inUse = true;
            printf("Enter value of the semaphore (0 >): ");
            scanf("%d", &value);
            if (value < 0){
                printf("Value must equal or exceed 0\n");
                semaphores[semaphoreID].inUse = false;
                return true;
            }
            semaphores[semaphoreID].value = value;
            printf("Semaphore with ID: %d and value: %d created!\n", semaphoreID, value);
            return true;
        }
        //Semaphore P
        case 'P':
        {
            if (currentlyRunning->priority == INIT_PROCESS){
                printf("Cannot call semaphore P on init process\n");
                return true;
            }
            printf("Enter ID for semaphore you want to use: ");
            int semaphoreID;
            scanf("%d", &semaphoreID);
            if (!semaphores[semaphoreID].inUse){
                printf("Semaphore ID: %d does not exist, create it or try another ID\n", semaphoreID);
                return true;
            }
            semaphores[semaphoreID].value--;
            if (semaphores[semaphoreID].value < 0){
                printf("Semaphore P (Current Value: %d) has resulted in current process (PID: %d) blocked\n", semaphores[semaphoreID].value, currentlyRunning->PID);
                if (!moveProcess(5, semaphoreID, currentlyRunning)){
                    printf("Semaphore: Out of Nodes for process\n");
                    return true;
                }
                getNextProcess(priorityManager());
            } else {
                printf("Semaphore P (Current Value %d) has been successful, current process (PID: %d) not blocked\n", semaphores[semaphoreID].value, currentlyRunning->PID);
            }
            return true;
        }
        //Semaphore V
        case 'V':
        {
            printf("Enter ID for semaphore you want to use: ");
            int semaphoreID;
            scanf("%d", &semaphoreID);
            if (!semaphores[semaphoreID].inUse) {
                printf("Semaphore ID: %d does not exist, create it or try another ID\n", semaphoreID);
                return true;
            }
            semaphores[semaphoreID].value++;
            if (semaphores[semaphoreID].value <= 0){
                ProcessControlBlock* tempPCB = NULL;
                List_first(semaphoreBlocked);
                tempPCB = List_search(semaphoreBlocked, comparatorSemaphoreID, &semaphoreID);
                if (tempPCB == NULL){
                    printf("Semaphore(v): Value incremented (Current Value: %d), but no blocked process found\n", semaphores[semaphoreID].value);
                } else {
                    List_remove(semaphoreBlocked);
                    moveProcess(tempPCB->priority, -1, tempPCB);
                    printf("Process (PID: %d) unblocked and on ready queue, Current semaphore value: %d\n", tempPCB->PID,semaphores[semaphoreID].value);
                }
            } else {
                printf("Semaphore value incremented, Current value: %d", semaphores[semaphoreID].value);
            }
            return true;
        }
        //Procinfo
        case 'I':
        {
            printf("Enter PID of process you want to information on to:");
            int tempPID;
            scanf("%d", &tempPID);
            if (tempPID == 0){
                printf("Init (PID: 0)process, runs when no ready processes are running, Use exit(E) on init to quit\n");
                return true;
            }
            int listNumber;
            listNumber = queueSearch(tempPID);
            if (listNumber == -1){
                printf("ProcInfo: PID not found\n");
                return true;
            }
            ProcessControlBlock* tempPCB = NULL;
            List *tempList = NULL;
            switch (listNumber) {
                case 0:
                    tempList = highPrio;
                    break;
                case 1:
                    tempList = normPrio;
                    break;
                case 2:
                    tempList = lowPrio;
                    break;
                case 3:
                    tempList = sendWait;
                    break;
                case 4:
                    tempList = recvWait;
                    break;
                case 5:
                    tempList = semaphoreBlocked;
                    break;
            }
            tempPCB = List_curr(tempList);
            char* prio = malloc(sizeof(char ) * 7);
            switch (tempPCB->state) {
                case PROCESS_READY:
                    strcpy(prio, "inQueue");
                    break;
                case PROCESS_BLOCKED:
                    if (listNumber == 3){
                        strcpy(prio, "blocked: waiting on Reply");
                    } else if (listNumber == 4){
                        strcpy(prio, "blocked: waiting on Send");
                    } else {
                        strcpy(prio, "blocked: On semaphore");
                    }
                    break;
                case PROCESS_RUNNING:
                    strcpy(prio, "running");
                    break;
            }
            char* cReply = malloc(sizeof(char ) * 5);
            if (tempPCB->replyMsg == NULL){
                strcpy(cReply, "false");
            } else {
                strcpy(cReply, "true");
            }
            printf("Process PID: %d\nPriority(0-High, 1-Norm, 2-Low): %d\nState: %s\nContains Reply: %s\nSemaphore blocked ID(-1 if none apply):%d\n"
            , tempPCB->PID, tempPCB->priority, prio, cReply, tempPCB->blockingSemaphoreID);
            free(prio);
            free(cReply);
            return true;
        }
        //TotalInfo
        case 'T':
            totalInfo();
            return true;
    }
    return false;
}

//Returns PID on success
// -1 on failure
int createProcess(int priority){
    int tempPID = generatePID();
    if (tempPID == -1){
        return tempPID;
    }

    ProcessControlBlock *newProcess;
    newProcess = malloc(sizeof(struct PCBStruct));
    newProcess->replyMsg = NULL;
    newProcess->PID = tempPID;
    newProcess->priority = priority;
    newProcess->state = PROCESS_READY;
    newProcess->blockingSemaphoreID = -1;
    newProcess->unblockedRecv = false;

    int failCheck;
    switch (priority) {
        case 0:
            failCheck = List_append(highPrio, newProcess);
            break;
        case 1:
            failCheck = List_append(normPrio, newProcess);
            break;
        case 2:
            failCheck = List_append(lowPrio, newProcess);
            break;
    }
    if (failCheck == -1){
        return -1;
    }
    return tempPID;
}

//Return -1 on lack of PID left
int generatePID(){
    return ++PIDNumbers;
}

//Creates Message on success or fail for Fork and Create
void processMadeOutput(char* command, int returnCheck){
    if (returnCheck != -1) {
        printf("%s: Process (PID: %d) Created and placed in ready queue on requested priority\n",command, returnCheck);
    } else {
        printf("%s: ERROR failed to create new process\n", command);
    }
}

void freeProcess(void* pItem){
    ProcessControlBlock* process = (struct PCBStruct*) pItem;
    free(process->replyMsg);
    process->replyMsg = NULL;
    free(process);
    process = NULL;
}

void freeMessage(void* pItem){
    Message* msg = (struct msgStruct*) pItem;
    free(msg->msg);
    msg->msg = NULL;
    free(msg);
    msg = NULL;
}

//Finds messages intended for Process pComparisonArg
bool comparatorRecvMsgPID(void* pItem, void* pComparisonArg){
    ProcessControlBlock* arg = (struct PCBStruct*) pComparisonArg;
    Message* curItem = (struct msgStruct*) pItem;
    if (arg->PID == curItem->destinationPID){
        return true;
    }
    return false;
}

//Finds PCB given PID as int
bool comparatorPCBPID(void* pItem, void* pComparisonArg){
    ProcessControlBlock* item = (struct PCBStruct*) pItem;
    int arg = *(int *) pComparisonArg;

    return item->PID == arg;
}

//Finds process blocked by semaphore
bool comparatorSemaphoreID(void* pItem, void* pComparisonArg){
    ProcessControlBlock* item = (struct PCBStruct*) pItem;
    int arg = *(int *) pComparisonArg;

    return item->blockingSemaphoreID == arg;
}

//Does the searching for queueSearch
bool queueSearchHelper(List* list, int processPID){
    ProcessControlBlock* temp;
    List_first(list);
    temp = List_search(list, comparatorPCBPID, &processPID);
    if (temp != NULL){
        return true;
    }
    return false;
}

//Searches all lists for given PID
//Returns which queue it's in given by int
//List current will be on the found PCB
//-1-Not found 0-High 1-Norm 2-Low 3-SendWait 4-RecvWait 5-SemaphoreBlock
int queueSearch(int processPID){
    if(queueSearchHelper(highPrio, processPID)){
        return HIGH_PRIORITY;
    }
    if(queueSearchHelper(normPrio, processPID)){
        return NORMAL_PRIORITY;
    }
    if(queueSearchHelper(lowPrio, processPID)){
        return LOW_PRIORITY;
    }
    if(queueSearchHelper(sendWait, processPID)){
        return 3;
    }
    if(queueSearchHelper(recvWait, processPID)){
        return 4;
    }
    if(queueSearchHelper(semaphoreBlocked, processPID)){
        return 5;
    }
    return -1;
}

//Creates message struct and puts it in waitingMessages list
//False on failure
bool createMessage(char* msg, int destPID){
    Message* newMsg = malloc(sizeof(struct msgStruct));
    newMsg->msg = malloc(sizeof(char ) *MAX_LEN +1);

    strcpy(newMsg->msg, msg);
    newMsg->destinationPID = destPID;
    newMsg->sourcePID = currentlyRunning->PID;

    int failCheck;
    failCheck = List_append(waitingMessages, newMsg);
    if (failCheck == -1){
        return false;
    }
    return true;
}


//Assumes CurrentlyRunning is already cleared
void getNextProcess(int priority){
    ProcessControlBlock* temp;

    for (; priority < INIT_PROCESS; priority++){
        if (priority == HIGH_PRIORITY){
            temp = List_first(highPrio);
            if (temp != NULL){
                List_remove(highPrio);
                break;
            }
        }
        if (priority == NORMAL_PRIORITY){
            temp = List_first(normPrio);
            if (temp != NULL){
                List_remove(normPrio);
                break;
            }
        }
        if (priority == LOW_PRIORITY){
            temp = List_first(lowPrio);
            if (temp != NULL){
                List_remove(lowPrio);
                break;
            }
        }
    }

    if (temp == NULL){
        runInit();
        printf("No ready processes\nNow Running Init process\n");
    } else {
        printf("Process PID: %d (Priority %d) is now running\nNorm priority in %d cycles\n"
               "Low priority in %d cycles\n", temp->PID, temp->priority, normQuantumCheck, lowQuantumCheck);
        temp->state = PROCESS_RUNNING;
        if (temp->replyMsg != NULL){
            printf("Reply received: %s", temp->replyMsg);
            free(temp->replyMsg);
            temp->replyMsg = NULL;
        }
        if (temp->unblockedRecv){
            Message* tempMsg = NULL;
            List_first(waitingMessages);
            tempMsg = List_search(waitingMessages, comparatorRecvMsgPID, temp);
            List_remove(waitingMessages);
            printf("Message found from PID: %d : %s", tempMsg->sourcePID, tempMsg->msg);
            freeMessage(tempMsg);
            tempMsg = NULL;
            temp->unblockedRecv = false;
        }
        currentlyRunning = temp;
    }
}

//Destination 0-High 1-Norm 2-Low 3-SendWait 4-RecvWait 5-SemaphoreBlock
//SemaphoreID 0-4 Semaphore IDs
//Assumption Valid semaphore ID
//Returns false on failure
bool moveProcess(int destination, int semaphoreID, ProcessControlBlock* process){
    int failCheck;
    switch (destination) {
        case 0:
        {
            failCheck = List_append(highPrio, process);
            process->state = PROCESS_READY;
            break;
        }
        case 1:
        {
            failCheck = List_append(normPrio, process);
            process->state = PROCESS_READY;
            break;
        }
        case 2:
        {
            failCheck = List_append(lowPrio, process);
            process->state = PROCESS_READY;
            break;
        }
        case 3:
        {
            failCheck = List_append(sendWait, process);
            process->state = PROCESS_BLOCKED;
            break;
        }
        case 4:
        {
            failCheck = List_append(recvWait, process);
            process->state = PROCESS_BLOCKED;
            break;
        }
        case 5:
        {
            process->blockingSemaphoreID = semaphoreID;
            failCheck = List_append(semaphoreBlocked, process);
            process->state = PROCESS_BLOCKED;
            break;
        }
    }
    if (failCheck == -1){
        return false;
    }
    if (process == currentlyRunning){
        currentlyRunning = NULL;
    }
    return true;
}

void totalInfoHelper(List* list){
    if(List_count(list) == 0){
        printf("Queue empty\n");
        return;
    }
    ProcessControlBlock* tempPCB = List_first(list);
    for(int count = 1; tempPCB != NULL; count++, tempPCB = List_next(list)){
        printf("%d) Process PID: %d - Priority: %d\n", count, tempPCB->PID, tempPCB->priority);
    }
}

void totalInfo(){
    printf("Six Queues:\n1.High Priority\n2.Normal Priority\n3.Low Priority\n4.SendWait\n5.RecvWait\n6.SemaphoreBlocked"
           "\nPriorities 0-High, 1-Normal, 2-Low\n");
    printf("High Priority Queue Contains %d processes:\n", List_count(highPrio));
    totalInfoHelper(highPrio);
    printf("Normal Priority Queue Contains %d processes:\n", List_count(normPrio));
    totalInfoHelper(normPrio);
    printf("Low Priority Queue Contains %d processes:\n", List_count(lowPrio));
    totalInfoHelper(lowPrio);
    printf("Send Blocked Queue Contains %d processes:\n", List_count(sendWait));
    totalInfoHelper(sendWait);
    printf("Receive Blocked Queue Contains %d processes:\n", List_count(recvWait));
    totalInfoHelper(recvWait);
    printf("Semaphore Blocked Queue Contains %d processes:\n", List_count(semaphoreBlocked));
    totalInfoHelper(semaphoreBlocked);
}


