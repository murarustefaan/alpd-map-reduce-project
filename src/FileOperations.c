#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "../defs/FileOperations.h"

char ** getFileNamesForDirectory(char * directoryName) {
    char ** filenames = (char **) malloc(getNumberOfFilesInDirectory(directoryName) * sizeof(char*));

    printf("%s\n", directoryName);

    DIR * directory = opendir(directoryName);
    struct dirent * temp;

    int fileCount = 0;

    if (directory != NULL) {
        while ((temp = readdir(directory)) != NULL) {
            // Exclude the . and .. fislepaths
            if (strcmp(temp->d_name, ".") == 0 ||
                strcmp(temp->d_name, "..") == 0) {
                temp = readdir(directory);
                continue;
            }

            filenames[fileCount] = (char*)malloc(strlen(temp->d_name) + 1);
            strcpy(filenames[fileCount++], temp->d_name);
            printf("%s\n", temp->d_name);
        }
    }

    closedir(directory);
    return filenames;
}

int getNumberOfFilesInDirectory(char * directoryName) {
    DIR * directory = opendir(directoryName);
    struct dirent * temp;

    int fileCount = 0;
    if (directory != NULL) {
        while ((temp = readdir(directory)) != NULL) {
            if (strcmp(temp->d_name, ".") == 0 ||
                strcmp(temp->d_name, "..") == 0) {
                temp = readdir(directory);
                continue;
            }

            fileCount++;
        }
    }

    closedir(directory);
    return fileCount;
}

char * buildFilePath(char * directoryName, char * fileName) {
    char * filePath = (char *)malloc(strlen(directoryName) + strlen(fileName) + 2);
    sprintf(filePath, "%s/%s", directoryName, fileName);

    return filePath;
}

bool isLetterOrNumber (char c) {
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
        return true;
    }

    return false;
}

char * readWord(FILE * file, char * filename) {
    char temp[255] = { '\0' };
    int i = 0;
    char c;

    while ((c = fgetc(file)) != EOF) {
        if (isLetterOrNumber(c)) {
            temp[i++] = c;
            temp[i] = '\0';
        } else {
            if (strlen(temp) == 0) { continue; }
            else { break; }
        }
    }

    // EOF reached
    if (strlen(temp) == 0) {
        return NULL;
    }

    char * word = (char *)malloc(strlen(temp));
    strcpy(word, temp);

    return word;
}
