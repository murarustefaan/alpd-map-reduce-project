#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../defs/FileOperations.h"

struct DirectoryFiles getFileNamesForDirectory(char * directoryName) {
    struct DirectoryFiles df;
    df.numberOfFiles = scandir(directoryName, &df.filenames, 0, alphasort);

    // The first 2 folders are '.' and '..' and we do not want those
    df.filenames += 2;
    df.numberOfFiles -= 2;

    return df;
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

FILE * createFile (char * filename) {
    FILE * f = fopen(filename, "w");

    if (!f) {
        printf("Could not write file with name %s\n", filename);
        return NULL;
    }

    return f;
}
