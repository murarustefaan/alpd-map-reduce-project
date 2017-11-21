#include "../defs/Utils.h"
#include <sys/time.h>
#include <stdlib.h>

int64_t getCurrentTimestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    int64_t timestamp = tv.tv_sec * (int64_t)1000000 + tv.tv_usec;
    return timestamp;
}