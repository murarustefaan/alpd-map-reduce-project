/**
 * Header library for a structure that holds the names and number of files from a directory
 *
 * @author Stefan Muraru
 * @date 01.12.2017
 */

#ifndef MAPREDUCE_V2_DIRECTORYFILES_H
#define MAPREDUCE_V2_DIRECTORYFILES_H

#include <dirent.h>

struct DirectoryFiles {
    struct dirent ** filenames;
    int numberOfFiles;
};

#endif
