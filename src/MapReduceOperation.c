#include <stdio.h>
#include <string.h>
#include "../defs/MapReduceOperation.h"

/**
 * Check if there are doable or undergoing operations
 * @param operations The collection of operations that need to be done
 * @param numberOfOperations The maximum number of operations that each MapReduce phase has to do
 * @return True or false, whether there is an active or doable operation
 */
bool doableOperations(struct Operation * operations, int numberOfOperations) {
    for (int i = 0 ; i < numberOfOperations; i++) {
        if ((operations + i)->currentOperation == Available &&
                (operations + i)->lastOperation != Done) {
            return true;
        }
    }

    printf("No doable operations found, exiting!\n");
    return false;
}

/**
 * Get the next doable operation for
 * @param operations The collection of operations
 * @param numberOfOperations The total number of operations that each MapReduce phase has to do
 * @return The next operation to assign to a worker
 */
struct Operation * getNextOperation(struct Operation * operations, int numberOfOperations) {
    for (int i = 0; i < numberOfOperations; i++) {
        if ((operations + i)->currentOperation == Available &&
            (operations + i)->lastOperation != Done) {
            printf("\x1B[31mOperation \"%s\" available\x1B[0m\n", (operations + i)->filename);
            return (operations + i);
        }
    }

    printf("No possible operations found! You might have an error in doubleOperations() function!\n");
    return NULL;
}

/**
 * Change the current status of the operation with the given name
 * @param operations The collections of operations
 * @param numberOfOperations The maximum number of operations that each MapReduce phase has to do
 * @param operationName The name of the operations to change the status to
 * @param currentStatus The status to move to
 */
void changeOperationCurrentStatusByName(struct Operation *operations, int numberOfOperations, char *operationName,
                                        enum OperationTag currentStatus) {
    struct Operation * operation;

    for (int i = 0; i < numberOfOperations; i++) {
        operation = operations + i;

        if (strcasecmp(operation->filename, operationName) == 0) {
            operation->currentOperation = currentStatus;

            printf("Changed operation with name %s currentStatus to %d\n", operationName, currentStatus);
            return;
        }
    }

    printf("No operation with name %s could be found\n", operationName);
}
/**
 * Change the status of the operation with the given name
 * @param operations The collections of operations
 * @param numberOfOperations The maximum number of operations that each MapReduce phase has to do
 * @param operationName The name of the operations to change the status to
 * @param lastStatus The status to move to
 */
void changeOperationLastStatusByName(struct Operation *operations, int numberOfOperations, char *operationName,
                                        enum OperationTag lastStatus) {
    struct Operation * operation;

    for (int i = 0; i < numberOfOperations; i++) {
        operation = operations + i;

        if (strcasecmp(operation->filename, operationName) == 0) {
            operation->lastOperation = lastStatus;

            printf("Changed operation with name %s lastStatus to %d\n", operationName, lastStatus);
            return;
        }
    }

    printf("No operation with name %s could be found\n", operationName);
}

/**
 * Return the next task code to send to a worker
 * @param lastTag The last operation tag
 * @return The next task code to send to a worker
 */
int getNextTaskForTag(enum OperationTag lastTag) {
    switch (lastTag) {
        default:
            return TASK_PROCESS_WORDS;

        case GetWords:
            return TASK_INDEX_FILE;
    }
}