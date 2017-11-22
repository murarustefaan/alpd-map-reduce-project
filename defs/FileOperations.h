#ifndef MAPREDUCE_V2_FILEOPERATIONS_H
#define MAPREDUCE_V2_FILEOPERATIONS_H

#include "../defs/DirectoryFiles.h"

struct DirectoryFiles getFileNamesForDirectory(char * directoryName);

char * buildFilePath(char * directoryName, char * fileName);

char * readWord(FILE * file, char * filename);

FILE * createFile(char * filename);

#endif
