/**
 * The main entry-point of the MapReduce algorithm
 *
 * The scope of this project was to implement the MapReduce algorithm using filesystem storage.
 * Based on some input files, the algorithm was to execute 4 stages of processing, as follows:
 *
 *  - Split the input files into words and write files on a temporary folder
 *      with the following signature {word}_{timestamp}
 *
 *  - Build the direct index of the given files by using the previously generated words.
 *      The direct index phase will write files in the "direct-index" folder containing the words
 *      and their corresponding number of appearances in the original file
 *
 *  - To avoid data race conditions on writing the appearances of the words(in the initial files) there was
 *      implemented another step that creates folders for all words, folders containing the number of appearances
 *      of the word in the initial files word/{fileName}_{appearances}_{timestamp}
 *
 *  - The last step, creating the reverse index, is done after all previous ones are finished. During this phase
 *      files are created for all words, containing the initial file name and the corresponding number of appearances
 *      of the word inside it
 *
 * @author Stefan Muraru
 * @date 01.12.2017
 */

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
#define REVERSE_INDEX_TEMP_LOCATION "/mnt/alpd/reverse-index-temporary"
#define REVERSE_INDEX_LOCATION "/mnt/alpd/reverse-index"

int main(int argc, char ** argv) {
    // SEGMENTATION FAULT HANDLER
    signal(SIGSEGV, handler);

    MPI_Init(&argc, &argv);

    int NUMBER_OF_PROCESSES;
    MPI_Comm_size(MPI_COMM_WORLD, &NUMBER_OF_PROCESSES);

    int CURRENT_RANK;
    MPI_Comm_rank(MPI_COMM_WORLD, &CURRENT_RANK);

    MPI_Status status;

    if (CURRENT_RANK == ROOT) {
        struct DirectoryFiles df = getFileNamesForDirectory(FILES_DIRECTORY);
        int fileIndex;

        // Create the directories for all four stages of processing
        // Get Words, Direct Index, "Pre" Reverse Index and Final Reverse Index
        int tempDirectoryCreated = mkdir(TEMP_DIRNAME, 0777);
        int directIndexDirectoryCreated = mkdir(DIRECT_INDEX_LOCATION, 0777);
        int reverseIndexTempDirectoryCreated = mkdir(REVERSE_INDEX_TEMP_LOCATION, 0777);
        int reverseIndexDirectoryCreated = mkdir(REVERSE_INDEX_LOCATION, 0777);

        // If any directory creation failed, the algorithm will not continue further
        if (tempDirectoryCreated == -1 ||
            directIndexDirectoryCreated == -1 ||
            reverseIndexTempDirectoryCreated == -1 ||
            reverseIndexDirectoryCreated == -1) {
            printf("%s_temp, direct-index, reverse-index temporary or final directory could not be created!%s\n", KRED, KNRM);
            for(int processRank = 1; processRank < NUMBER_OF_PROCESSES; processRank++) {
                printf("%sSENDING KILL TO %d%s\n", KRED, processRank, KNRM);

                MPI_Request kill_req;
                MPI_Isend(NULL, 0, MPI_CHAR, processRank, TASK_KILL, MPI_COMM_WORLD, &kill_req);
            }
            MPI_Finalize();
            return 0;
        }

        // Create a list of the input files that contains the filename, the current operation
        // and the last operation that was executed on that file
        struct Operation * reduceOperations = (struct Operation *) malloc(df.numberOfFiles * sizeof(struct Operation));
        int numberOfOperations = df.numberOfFiles;

        for (fileIndex = 0; fileIndex < numberOfOperations; fileIndex++) {
            reduceOperations[fileIndex].filename = df.filenames[fileIndex]->d_name;
            reduceOperations[fileIndex].currentOperation = reduceOperations[fileIndex].lastOperation = Available;
        }

        MPI_Request req;
        int flag;
        MPI_Status status;

        // The MASTER process will keep listening for messages from workers while not all files are completely processed
        while(doableOperations(reduceOperations, numberOfOperations)) {
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

                MPI_Request task_req;
                MPI_Isend(nextOperation->filename,
                         strlen(nextOperation->filename) + 1,
                         MPI_CHAR,
                         destination,
                         nextTask,
                         MPI_COMM_WORLD,
                         &task_req);
            } else {
                // Cancel and free the listener for the request if no message came trough
                MPI_Cancel(&req);
                MPI_Request_free(&req);
            }

            free(processedFile);
        }

        free(reduceOperations);
        printf("Root -> GetWords, DirectIndexing and the first stage of ReverseIndexing are finished\n");
        for (int i = 0; i < df.numberOfFiles; i++) {
            free(df.filenames[i]);
        }

        /**
         * Start the reverse index phase once all other tasks have been successfully completed
         */
        int numberOfReverseIndexedWords = 0;
        df = getFileNamesForDirectory(REVERSE_INDEX_TEMP_LOCATION);

        bool availableWorkers[NUMBER_OF_PROCESSES];
        for (int i = 1; i < NUMBER_OF_PROCESSES; i++) {
            availableWorkers[i] = true;
        }

        printf("Root -> Found a number of %d words\nRoot -> Beginning reverse-indexing\n", df.numberOfFiles);

        while(numberOfReverseIndexedWords != df.numberOfFiles) {
            char processedFile[FILENAME_MAX];
            MPI_Irecv(processedFile, FILENAME_MAX, MPI_CHAR, MPI_ANY_SOURCE, TASK_REVERSE_INDEX_WORD, MPI_COMM_WORLD, &req);
            MPI_Test(&req, &flag, &status);

            if (flag == true) {
                availableWorkers[status.MPI_SOURCE] = true;
            } else {
                MPI_Cancel(&req);
                MPI_Request_free(&req);
            }

            int availableWorkerId = 0;
            if ((availableWorkerId = getAvailableWorkerId(availableWorkers, NUMBER_OF_PROCESSES)) != 0) {
                MPI_Isend(df.filenames[numberOfReverseIndexedWords]->d_name,
                        strlen(df.filenames[numberOfReverseIndexedWords]->d_name) + 1,
                        MPI_CHAR,
                        availableWorkerId,
                        TASK_REVERSE_INDEX_WORD,
                        MPI_COMM_WORLD,
                        &req);

                availableWorkers[availableWorkerId] = false;
                numberOfReverseIndexedWords++;
            }
        }

        for (int i = 0; i < df.numberOfFiles; i++) {
            free(df.filenames[i]);
        }

        printf("%sROOT -> Finished reverse indexing%s\n", KMAG, KNRM);

        for(int processRank = 1; processRank < NUMBER_OF_PROCESSES; processRank++) {
            printf("SENDING KILL TO %d\n", processRank);

            MPI_Request kill_req;
            MPI_Isend(NULL, 0, MPI_CHAR, processRank, TASK_KILL, MPI_COMM_WORLD, &kill_req);
        }
    }

    if (CURRENT_RANK != ROOT) {
        int tag = 0;
        char * fileName;

        MPI_Request ack_req;
        MPI_Isend(NULL, 0, MPI_CHAR, ROOT, TASK_ACK, MPI_COMM_WORLD, &ack_req);
        MPI_Wait(&ack_req, &status);

        // Workers will listen for task messages while the received message tag is not TASK_KILL
        do {
            int messageReceived;
            MPI_Request taskRequest;

            fileName = (char *)malloc(FILENAME_MAX);
            MPI_Irecv(fileName, FILENAME_MAX, MPI_CHAR, ROOT, MPI_ANY_TAG, MPI_COMM_WORLD, &taskRequest);

            MPI_Test(&taskRequest, &messageReceived, &status);
            if (messageReceived == false) {
                free(fileName);
                MPI_Cancel(&taskRequest);
                MPI_Request_free(&taskRequest);
                continue;
            }

            switch(status.MPI_TAG) {
                case TASK_PROCESS_WORDS: {
                    MPI_Request req;

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

                        // For loop used to try to write the file to the disk.
                        // Creation of the file might fail if the timestamp is the previous one
                        // This is a safety measure, even if the chances of that happening are slim
                        for (int i = 0; i < 5; i++) {
                            char timestamp[42];
                            sprintf(timestamp, "%ld", getCurrentTimestamp());
                            fileToWrite = (char *)malloc(strlen(timestamp) + strlen(word) + 2);
                            sprintf(fileToWrite, "%s_%s", word, timestamp);
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

                    MPI_Isend(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_PROCESS_WORDS,
                             MPI_COMM_WORLD,
                             &req);
                    break;
                }
                case TASK_INDEX_FILE: {
                    MPI_Request req;

                    char * directoryPath = buildFilePath(TEMP_DIRNAME, fileName);
                    struct DirectoryFiles df = getFileNamesForDirectory(directoryPath);
                    if (df.numberOfFiles == 0) {
                        printf("%sWorker %d -> No words found in directory %s%s\n", KRED, CURRENT_RANK, directoryPath, KNRM);
                        free(directoryPath);

                        MPI_Isend(fileName,
                                 strlen(fileName) + 1,
                                 MPI_CHAR,
                                 ROOT,
                                 TASK_INDEX_FILE,
                                 MPI_COMM_WORLD,
                                 &req);
                        break;
                    }

                    char * directIndexFilePath = buildFilePath(DIRECT_INDEX_LOCATION, fileName);
                    fclose(createFile(directIndexFilePath));

                    FILE * file = fopen(directIndexFilePath, "a");
                    if (!file) {
                        printf("%sWorker %d -> Could not write direct-index file %s%s\n", KRED, CURRENT_RANK, directIndexFilePath, KNRM);

                        MPI_Isend(fileName,
                                 strlen(fileName) + 1,
                                 MPI_CHAR,
                                 ROOT,
                                 TASK_INDEX_FILE,
                                 MPI_COMM_WORLD,
                                 &req);
                        break;
                    }

                    char * word;
                    char * lastWord = strtok(df.filenames[0]->d_name, "_");
                    int wordCount = 1;

                    for(int i = 0; i < df.numberOfFiles; i++) {
                        word = strtok(df.filenames[i]->d_name, "_");

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

                    for (int i = 0; i < df.numberOfFiles; i++) {
                        free(df.filenames[i]);
                    }

                    MPI_Isend(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_INDEX_FILE,
                             MPI_COMM_WORLD,
                             &req);

                    break;
                }

                case TASK_REVERSE_INDEX_FILE: {
                    MPI_Request req;

                    printf("%sWorker %d -> Received file %s for reverse-indexing%s\n", KYEL, CURRENT_RANK, fileName, KNRM);

                    char * filePath = buildFilePath(DIRECT_INDEX_LOCATION, fileName);
                    FILE * directIndexFile = fopen(filePath, "r");
                    if (!directIndexFile) {
                        printf("%sWorker %d -> Could not read direct-index file %s%s\n", KRED, CURRENT_RANK, filePath, KNRM);

                        MPI_Isend(fileName,
                                 strlen(fileName) + 1,
                                 MPI_CHAR,
                                 ROOT,
                                 TASK_REVERSE_INDEX_FILE,
                                 MPI_COMM_WORLD,
                                 &req);
                        break;
                    }
                    free(filePath);

                    char * word;
                    char * numberOfApparitions;
                    while ((word = readWord(directIndexFile)) != NULL &&
                        (numberOfApparitions = readWord(directIndexFile)) != NULL) {

                        char * wordPath = buildFilePath(REVERSE_INDEX_TEMP_LOCATION, word);
                        mkdir(wordPath, 0777);

                        char fileNameToWrite[FILENAME_MAX];
                        sprintf(fileNameToWrite, "%s_%s_%ld", fileName, numberOfApparitions, getCurrentTimestamp());
                        filePath = buildFilePath(wordPath, fileNameToWrite);

                        FILE * wordFile = fopen(filePath, "a");
                        fclose(wordFile);

                        free(wordPath);
                        free(word);
                        free(numberOfApparitions);
                    }

                    MPI_Isend(fileName,
                             strlen(fileName) + 1,
                             MPI_CHAR,
                             ROOT,
                             TASK_REVERSE_INDEX_FILE,
                             MPI_COMM_WORLD,
                             &req);

                    fclose(directIndexFile);
                    break;
                }

                case TASK_REVERSE_INDEX_WORD: {
                    MPI_Request req;
                    char * wordPath = buildFilePath(REVERSE_INDEX_TEMP_LOCATION, fileName);
                    struct DirectoryFiles df = getFileNamesForDirectory(wordPath);

                    FILE * wordFile = fopen(buildFilePath(REVERSE_INDEX_LOCATION, fileName), "a");
                    if (!wordFile) {
                        printf("%sWorker %d -> Could not write reverse-index file %s%s\n", KRED, CURRENT_RANK, fileName, KNRM);

                        MPI_Isend(fileName,
                                  strlen(fileName) + 1,
                                  MPI_CHAR,
                                  ROOT,
                                  TASK_REVERSE_INDEX_FILE,
                                  MPI_COMM_WORLD,
                                  &req);
                        break;
                    }

                    for (int i = 0; i < df.numberOfFiles; i++) {
                        char *parentFile = strtok(df.filenames[i]->d_name, "_");
                        char *numberOfApparitions = strtok(NULL, "_");
                        fprintf(wordFile, "%s %s\n", parentFile, numberOfApparitions);
                    }

                    for (int i = 0; i < df.numberOfFiles; i++) {
                        free(df.filenames[i]);
                    }

                    fclose(wordFile);
                    free(wordPath);

                    MPI_Isend(fileName,
                              strlen(fileName) + 1,
                              MPI_CHAR,
                              ROOT,
                              TASK_REVERSE_INDEX_WORD,
                              MPI_COMM_WORLD,
                              &req);

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