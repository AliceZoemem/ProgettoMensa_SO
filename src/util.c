#include <stdlib.h>
#include <unistd.h>
#include "util.h"
#include <time.h>

int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

void nanosleep_ms(int ms) {
    struct timespec t = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000 };
    nanosleep(&t, NULL);
}