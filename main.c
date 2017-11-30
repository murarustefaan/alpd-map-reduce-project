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

#define ROOT 0
#define TASK_ACK 101
#define TASK_INDEX_FILE 102
#define TASK_PROCESS_WORDS 103
#define TASK_KILL 999

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
            printf("_temp or direct-index directory could not be created!\n");
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
            reduceOperations[fileIndex].currentOperation = Available;
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


                switch (receivedTag) {
                    case TASK_INDEX_FILE: {
                        if (processedFile == NULL) {
                            break;
                        }
                        printf("ROOT -> Received filename %s from %d on tag %d\n", processedFile, destination, receivedTag);

                        changeOperationCurrentStatusByName(reduceOperations, numberOfOperations, processedFile, Done);
                        changeOperationLastStatusByName(reduceOperations, numberOfOperations, processedFile, Done);
                    }

                    default:
                    case TASK_ACK: {
                        struct Operation * nextOperation = getNextOperation(reduceOperations, numberOfOperations);
                        if (!nextOperation) { printf("No next Operation found!\n"); break; }

                        printf("Sending file to %d to be word-processed \"%s\"\n", destination,nextOperation->filename);

                        changeOperationCurrentStatusByName(reduceOperations, numberOfOperations,
                                                           nextOperation->filename, InProgress);
                        MPI_Send(nextOperation->filename,
                                 strlen(nextOperation->filename) + 1,
                                 MPI_CHAR,
                                 destination,
                                 TASK_PROCESS_WORDS,
                                 MPI_COMM_WORLD);

                        break;
                    }

                    case TASK_PROCESS_WORDS: {
                        if (processedFile == NULL) {
                            break;
                        }
                        printf("ROOT -> Received filename %s from %d on tag %d\n", processedFile, destination, receivedTag);

                        changeOperationCurrentStatusByName(reduceOperations, numberOfOperations, processedFile, Available);
                        changeOperationLastStatusByName(reduceOperations, numberOfOperations, processedFile, GetWords);

                        struct Operation * nextOperation = getNextOperation(reduceOperations, numberOfOperations);
                        if (!nextOperation) { break; }

                        printf("Sending file to be indexed %s\n", nextOperation->filename);

                        MPI_Send(nextOperation->filename,
                                 strlen(nextOperation->filename) + 1,
                                 MPI_CHAR,
                                 destination,
                                 TASK_INDEX_FILE,
                                 MPI_COMM_WORLD);

                        fileIndex--;

                        break;
                    }
                }
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
                        printf("Process %d could not open file at \"%s\"!\n", CURRENT_RANK, fullPath);
                        free(fullPath);
                        break;
                    }

                    char * tempDirName = buildFilePath(TEMP_DIRNAME, fileName);
                    mkdir(tempDirName, 0777);

                    printf("Worker %d opened file \"%s\"\n", CURRENT_RANK, fullPath);
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

                    printf("Slave %d found %d words in file \"%s\"\n", CURRENT_RANK, numberOfWords, fileName);

                    free(tempDirName);
                    fclose(file);

                    MPI_Send(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_PROCESS_WORDS,
                             MPI_COMM_WORLD);
                    printf("Slave %d sent word to ROOT that he finished %s on tag %d\n", CURRENT_RANK, fileName, TASK_PROCESS_WORDS);
                    break;
                }
                case TASK_INDEX_FILE: {
                    char * directoryPath = buildFilePath(TEMP_DIRNAME, fileName);
                    struct DirectoryFiles df = getFileNamesForDirectory(directoryPath);
                    if (df.numberOfFiles == 0) {
                        printf("No words found in directory %s\n", directoryPath);
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
                        printf("Could not write direct-index file %s\n", directIndexFilePath);

                        MPI_Send(fileName,
                                 strlen(fileName) + 1,
                                 MPI_CHAR,
                                 ROOT,
                                 TASK_INDEX_FILE,
                                 MPI_COMM_WORLD);
                        break;
                    }

                    char * word;
                    char * lastWord = getWordFromFileName(df.filenames[2]->d_name);
                    int wordCount = 1;

                    for(int i = 3; i < df.numberOfFiles; i++) {
                        word = getWordFromFileName(df.filenames[i]->d_name);

                        if (strcmp(lastWord, word) == 0) {
                            wordCount++;
                        } else {
                            printf("%s, %d\n", lastWord, wordCount);

                            fprintf(file, "%s %d\n", lastWord, wordCount);

                            free(lastWord);
                            lastWord = word;
                            wordCount = 1;
                        }
                    }

                    printf("Indexed file! %s\n", fileName);

                    fclose(file);

                    MPI_Send(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_INDEX_FILE,
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