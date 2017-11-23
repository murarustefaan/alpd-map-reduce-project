#ifndef MAPREDUCE_V2_MAPREDUCEOPERATION_H
#define MAPREDUCE_V2_MAPREDUCEOPERATION_H

/**
 * The available states of processing for a specific file
 */
enum OperationTag {
    GetWords,
    DirectIndex,
    ReverseIndex
};

/**
 * Struct to hold the name of the file that is processed,
 * The node that did the last processing,
 * And the last operation that was successfully completed
 */
struct Operation {
    char * filename;
    int node;
    enum OperationTag tag;
};

#endif
