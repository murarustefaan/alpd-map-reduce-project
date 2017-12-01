/**
 * Header library used to facilitate the usage of files and directories
 *
 * @author Stefan Muraru
 * @date 01.12.2017
 */

#ifndef MAPREDUCE_V2_FILEOPERATIONS_H
#define MAPREDUCE_V2_FILEOPERATIONS_H

#include "../defs/DirectoryFiles.h"

struct DirectoryFiles getFileNamesForDirectory(char * directoryName);

char * buildFilePath(char * directoryName, char * fileName);

char * readWord(FILE * file);

FILE * createFile(char * filename);

#endif
