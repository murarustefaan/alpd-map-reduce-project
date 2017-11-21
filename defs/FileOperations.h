#ifndef MAPREDUCE_V2_FILEOPERATIONS_H
#define MAPREDUCE_V2_FILEOPERATIONS_H

char ** getFileNamesForDirectory(char * directoryName);

int getNumberOfFilesInDirectory(char * directoryName);

char * buildFilePath(char * directoryName, char * fileName);

char * readWord(FILE * file, char * filename);

#endif
