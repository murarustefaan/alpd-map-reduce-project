#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mpi.h>

#include "defs/ErrorHandling.h"
#include "defs/FileOperations.h"
#include "defs/Utils.h"
#include "defs/MapReduceOperation.h"
#include "defs/Logging.h"

#define FILES_DIRECTORY "input-files"
#define TEMP_DIRNAME "/mnt/alpd/_temp"
#define DIRECT_INDEX_LOCATION "/mnt/alpd/direct-index"

int main(int argc, char ** argv) {
    signal(SIGSEGV, handler);

    MPI_Init(&argc, &argv);

    int NUMBER_OF_PROCESSES;
    MPI_Comm_size(MPI_COMM_WORLD, &NUMBER_OF_PROCESSES);

    int CURRENT_RANK;
    MPI_Comm_rank(MPI_COMM_WORLD, &CURRENT_RANK);

    MPI_Status status;

    if (CURRENT_RANK == ROOT) {
        // retrieve the list of files from the directory
        struct DirectoryFiles df = getFileNamesForDirectory(FILES_DIRECTORY);
        int fileIndex;

        int tempDirectoryCreated = mkdir(TEMP_DIRNAME, 0777);
        int directIndexDirectoryCreated = mkdir(DIRECT_INDEX_LOCATION, 0777);
        if (tempDirectoryCreated == -1 ||
            directIndexDirectoryCreated == -1) {
            printf("%s_temp or direct-index directory could not be created!%s\n", KRED, KNRM);
            for(int processRank = 1; processRank < NUMBER_OF_PROCESSES; processRank++) {
                printf("SENDING KILL TO %d\n", processRank);
                MPI_Send(NULL, 0, MPI_CHAR, processRank, TASK_KILL, MPI_COMM_WORLD);
            }
            MPI_Finalize();
            return 0;
        }

        struct Operation * reduceOperations = (struct Operation *) malloc(df.numberOfFiles * sizeof(struct Operation));
        int numberOfOperations = df.numberOfFiles;

        // Create a list of operations that need to be done on the found files
        for (fileIndex = 0; fileIndex < numberOfOperations; fileIndex++) {
            reduceOperations[fileIndex].filename = df.filenames[fileIndex]->d_name;
            reduceOperations[fileIndex].currentOperation = reduceOperations[fileIndex].lastOperation = Available;
        }

        while(doableOperations(reduceOperations, numberOfOperations)) {
            MPI_Request req;
            int flag;
            MPI_Status status;
            char * processedFile = (char *)malloc(FILENAME_MAX);
            MPI_Irecv(processedFile, FILENAME_MAX, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &req);

            MPI_Test(&req, &flag, &status);
            if (flag == true) {
                int destination = status.MPI_SOURCE;
                int receivedTag = status.MPI_TAG;

                // Handle the finish of a worker operation
                switch (receivedTag) {
                    case TASK_INDEX_FILE: {
                        printf("%sROOT -> Worker %d direct-indexed file %s%s\n", KGRN, destination, processedFile, KNRM);

                        changeOperationCurrentStatusByName(reduceOperations, numberOfOperations, processedFile, Available);
                        changeOperationLastStatusByName(reduceOperations, numberOfOperations, processedFile, DirectIndex);

                        break;
                    }

                    case TASK_PROCESS_WORDS: {
                        printf("%sROOT -> Worker %d processed the words of file %s%s\n", KBLU, destination, processedFile, KNRM);

                        changeOperationCurrentStatusByName(reduceOperations, numberOfOperations, processedFile, Available);
                        changeOperationLastStatusByName(reduceOperations, numberOfOperations, processedFile, GetWords);

                        break;
                    }

                    case TASK_REVERSE_INDEX_FILE: {
                        changeOperationCurrentStatusByName(reduceOperations, numberOfOperations, processedFile, Done);
                        changeOperationLastStatusByName(reduceOperations, numberOfOperations, processedFile, Done);
                        printf("%sROOT -> Worker %d reverse-indexed file %s%s\n", KYEL, destination, processedFile, KNRM);
                    }
                }

                struct Operation * nextOperation = getNextOperation(reduceOperations, numberOfOperations);
                if (!nextOperation) { printf("No next Operation found!\n"); continue; }

                changeOperationCurrentStatusByName(reduceOperations, numberOfOperations,
                                                   nextOperation->filename, InProgress);
                int nextTask = getNextTaskForTag(nextOperation->lastOperation);

                printf("ROOT -> Sending file %s to %d on task %d\n", nextOperation->filename, destination, nextTask);

                MPI_Send(nextOperation->filename,
                         strlen(nextOperation->filename) + 1,
                         MPI_CHAR,
                         destination,
                         nextTask,
                         MPI_COMM_WORLD);
            } else {
                MPI_Cancel(&req);
                MPI_Request_free(&req);
            }

            free(processedFile);
        }

        for(int processRank = 1; processRank < NUMBER_OF_PROCESSES; processRank++) {
            printf("SENDING KILL TO %d\n", processRank);
            MPI_Send(NULL, 0, MPI_CHAR, processRank, TASK_KILL, MPI_COMM_WORLD);
        }
    }

    if (CURRENT_RANK != ROOT) {
        int tag = 0;
        char * fileName;
        MPI_Send(NULL, 0, MPI_CHAR, ROOT, TASK_ACK, MPI_COMM_WORLD);

        do {
            fileName = (char *)malloc(FILENAME_MAX);
            MPI_Recv(fileName, FILENAME_MAX, MPI_CHAR, ROOT, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            switch(status.MPI_TAG) {
                case TASK_PROCESS_WORDS: {
                    char * fullPath = buildFilePath(FILES_DIRECTORY, fileName);

                    FILE * file = fopen(fullPath, "r");
                    if (!file) {
                        printf("%sWorker %d -> Could not open file at \"%s\"!%s\n", KRED, CURRENT_RANK, fullPath, KNRM);
                        free(fullPath);
                        break;
                    }

                    char * tempDirName = buildFilePath(TEMP_DIRNAME, fileName);
                    mkdir(tempDirName, 0777);

                    printf("%sWorker %d -> Opened file \"%s\"%s\n", KBLU, CURRENT_RANK, fullPath, KNRM);
                    free(fullPath);
                    char * word;
                    int numberOfWords = 0;
                    while ((word = readWord(file)) != NULL) {
                        // Create a file with format "{fileName}.{timestamp}"
                        FILE * written;
                        char * pathToWrite;
                        char * fileToWrite;

                        for (int i = 0; i < 5; i++) {
                            char timestamp[42];
                            sprintf(timestamp, "%ld", getCurrentTimestamp());
                            fileToWrite = (char *)malloc(strlen(timestamp) + strlen(word) + 2);
                            sprintf(fileToWrite, "%s.%s", word, timestamp);
                            pathToWrite = buildFilePath(tempDirName, fileToWrite);

                            written = createFile(pathToWrite);

                            free(fileToWrite);
                            free(pathToWrite);

                            if (written != NULL) {
                                break;
                            }
                        }

                        numberOfWords++;

                        free(word);

                        if (written != NULL) {
                            fclose(written);
                        }
                    }

                    printf("%sWorker %d -> Found %d words in file \"%s\"%s\n", KBLU, CURRENT_RANK, numberOfWords, fileName, KNRM);

                    free(tempDirName);
                    fclose(file);

                    MPI_Send(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_PROCESS_WORDS,
                             MPI_COMM_WORLD);
                    break;
                }
                case TASK_INDEX_FILE: {
                    char * directoryPath = buildFilePath(TEMP_DIRNAME, fileName);
                    struct DirectoryFiles df = getFileNamesForDirectory(directoryPath);
                    if (df.numberOfFiles == 2) {
                        printf("%sWorker %d -> No words found in directory %s%s\n", KRED, CURRENT_RANK, directoryPath, KNRM);
                        free(directoryPath);

                        MPI_Send(fileName,
                                 strlen(fileName) + 1,
                                 MPI_CHAR,
                                 ROOT,
                                 TASK_INDEX_FILE,
                                 MPI_COMM_WORLD);
                        break;
                    }

                    char * directIndexFilePath = buildFilePath(DIRECT_INDEX_LOCATION, fileName);
                    fclose(createFile(directIndexFilePath));

                    FILE * file = fopen(directIndexFilePath, "a");
                    if (!file) {
                        printf("%sWorker %d -> Could not write direct-index file %s%s\n", KRED, CURRENT_RANK, directIndexFilePath, KNRM);

                        MPI_Send(fileName,
                                 strlen(fileName) + 1,
                                 MPI_CHAR,
                                 ROOT,
                                 TASK_INDEX_FILE,
                                 MPI_COMM_WORLD);
                        break;
                    }

                    char * word;
                    char * lastWord = strtok(df.filenames[2]->d_name, ".");
                    int wordCount = 1;

                    for(int i = 3; i < df.numberOfFiles; i++) {
                        word = strtok(df.filenames[i]->d_name, ".");

                        if (strcmp(lastWord, word) == 0) {
                            wordCount++;
                        } else {
                            fprintf(file, "%s %d\n", lastWord, wordCount);

                            lastWord = word;
                            wordCount = 1;
                        }
                    }

                    printf("%sWorker %d -> Indexed file %s%s\n", KGRN, CURRENT_RANK, fileName, KNRM);

                    fclose(file);

                    for (int i = 3; i < df.numberOfFiles; i++) {
                        free(df.filenames[i]);
                    }

                    MPI_Send(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_INDEX_FILE,
                             MPI_COMM_WORLD);

                    break;
                }

                case TASK_REVERSE_INDEX_FILE: {
                    printf("%sWorker %d -> Received file %s for reverse-indexing%s\n", KYEL, CURRENT_RANK, fileName, KNRM);

                    MPI_Send(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_REVERSE_INDEX_FILE,
                             MPI_COMM_WORLD);

                    break;
                }
            }

            free(fileName);
            tag = status.MPI_TAG;
        } while (tag != TASK_KILL);

    }

    MPI_Finalize();

    return 0;
}