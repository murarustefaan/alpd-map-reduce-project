#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../defs/FileOperations.h"

/**
 * Get the number of files in a directory and their corresponding file names
 * @param directoryName The path of the directory for which to get the files
 * @return A struct containing the file names and the number of files
 */
struct DirectoryFiles getFileNamesForDirectory(char * directoryName) {
    struct DirectoryFiles df;
    df.numberOfFiles = scandir(directoryName, &df.filenames, 0, alphasort);

    // The first 2 folders are '.' and '..' and we do not want those
    df.filenames += 2;
    df.numberOfFiles -= 2;

    return df;
}

/**
 * Create a file path by concatenating the 2 given paths into a single one
 * @param directoryName The first part of the path
 * @param fileName The other part of the path
 * @return The joined path
 */
char * buildFilePath(char * directoryName, char * fileName) {
    char * filePath = (char *)malloc(strlen(directoryName) + strlen(fileName) + 2);
    sprintf(filePath, "%s/%s", directoryName, fileName);

    return filePath;
}

/**
 * Check if a given character is either a letter or a number
 * @param c The character to check
 * @return Return true or false, depending whether the character is or isn't a letter or a number
 */
bool isLetterOrNumber (char c) {
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
        return true;
    }

    return false;
}

/**
 * Read and word from an open file stream
 * A word means a group of letters Aa-Zz or numbers 0-9
 * @param file The file stream to read from
 * @return A pointer to the read word
 */
char * readWord(FILE * file) {
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

char * getWordFromFileName(char * fileName) {
    int len = strlen(fileName);
    char * name = (char *)malloc(len * sizeof(char));
    for (int i = 0; i < len; i++) {
        if (fileName[i] == '.') {
            break;
        }

        name[i] = fileName[i];
    }

    return name;
}

/**
 * Creates a file and return a pointer to it
 * @param filename The name of the file to be created
 * @return A pointer to the created file or NULL in case it could not be created
 */
FILE * createFile (char * filename) {
    FILE * f = fopen(filename, "w");

    if (!f) {
        printf("Could not write file with name %s\n", filename);
        return NULL;
    }

    return f;
}
