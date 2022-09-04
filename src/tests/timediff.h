#ifndef __TIMEDIFF_ 
#define __TIMEDIFF_ 

#include <time.h>
#include <stdio.h>

static inline long double time_diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return (long double)temp.tv_sec + (long double)temp.tv_nsec/1000000000;
}

#define TIMEDIFF_INIT() struct timespec __t0, __t1

#define TIMEDIFF_TAKE(fun, str) do {                                            \
                        clock_gettime(CLOCK_MONOTONIC, &__t0);                  \
                        fun;                                                    \
                        clock_gettime(CLOCK_MONOTONIC, &__t1);                  \
                        printf(str"\t%.3Lf\n", time_diff(__t0, __t1));          \
} while (0)

#define TIMEDIFF_TAKE_VAL(fun, val) do {				\
		clock_gettime(CLOCK_MONOTONIC, &__t0);			\
		fun;							\
		clock_gettime(CLOCK_MONOTONIC, &__t1);			\
		val = time_diff(__t0, __t1);				\
} while (0)

#define TIMEDIFF_START() clock_gettime(CLOCK_MONOTONIC, &__t0)

#define TIMEDIFF_STOP(str) do {                                                 \
                        clock_gettime(CLOCK_MONOTONIC, &__t1);                  \
                        printf(str"\t%.3Lf\n", time_diff(__t0, __t1));          \
} while (0)

#endif
