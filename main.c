#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mpi.h>

#include "defs/ErrorHandling.h"
#include "defs/FileOperations.h"
#include "defs/Utils.h"

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
        int availableWorkers = NUMBER_OF_PROCESSES - 1;

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



        for(fileIndex = 0; fileIndex < df.numberOfFiles; fileIndex++) {
            char * processedFile = (char *)malloc(FILENAME_MAX);
            MPI_Recv(processedFile, FILENAME_MAX, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            availableWorkers++;
            int destination = status.MPI_SOURCE;
            int receivedTag = status.MPI_TAG;

            printf("ROOT -> Received filename %s from %d on tag %d\n", processedFile, destination, receivedTag);

            switch (receivedTag) {
                default:
                case TASK_ACK:
                case TASK_INDEX_FILE: {
                    printf("Sending file to be word-processed \"%s\"\n", df.filenames[fileIndex]->d_name);

                    MPI_Send(df.filenames[fileIndex]->d_name,
                             strlen(df.filenames[fileIndex]->d_name) + 1,
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

                    printf("Sending file to be indexed %s\n", processedFile);

                    MPI_Send(processedFile,
                             strlen(processedFile) + 1,
                             MPI_CHAR,
                             destination,
                             TASK_INDEX_FILE,
                             MPI_COMM_WORLD);

                    fileIndex--;

                    break;
                }
            }

            availableWorkers--;
        }

        while (availableWorkers != NUMBER_OF_PROCESSES - 1) {
            char processedFile[FILENAME_MAX];
            MPI_Recv(processedFile, FILENAME_MAX, MPI_CHAR, MPI_ANY_SOURCE, TASK_ACK, MPI_COMM_WORLD, &status);
            availableWorkers++;
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
                        break;
                    }

                    char * tempDirName = buildFilePath(TEMP_DIRNAME, fileName);
                    mkdir(tempDirName, 0777);

                    printf("Worker %d opened file \"%s\"\n", CURRENT_RANK, fullPath);
                    char * word;
                    int numberOfWords = 0;
                    while ((word = readWord(file, fileName)) != NULL) {
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

                            if (written != NULL) {
                                break;
                            }
                        }

                        numberOfWords++;

                        free(word);
                        free(fileToWrite);
                        free(pathToWrite);

                        if (written != NULL) {
                            fclose(written);
                        }
                    }

                    printf("Slave %d found %d words in file \"%s\"\n", CURRENT_RANK, numberOfWords, fileName);

                    free(tempDirName);
                    free(fullPath);
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
                    char * lastWord = strtok(df.filenames[0]->d_name, ".");
                    int wordCount = 1;

                    for(int i = 1; i < df.numberOfFiles; i++) {
                        word = strtok(df.filenames[i]->d_name, ".");

                        if (strcmp(lastWord, word) == 0) {
                            wordCount++;
                        } else {
                            printf("%s, %d\n", lastWord, wordCount);

                            fprintf(file, "%s %d\n", lastWord, wordCount);

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