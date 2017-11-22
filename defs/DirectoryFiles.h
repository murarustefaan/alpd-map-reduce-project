#ifndef MAPREDUCE_V2_DIRECTORYFILES_H
#define MAPREDUCE_V2_DIRECTORYFILES_H

#include <dirent.h>

struct DirectoryFiles {
    struct dirent ** filenames;
    int numberOfFiles;
};

#endif
