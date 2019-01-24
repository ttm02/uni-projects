#ifndef TESTS_SRC_TESTS_H_
#define TESTS_SRC_TESTS_H_

#include <omp.h>
#include <stdio.h>
#include <unistd.h>

// utility macro:
// usage: like printf
// ensures that the printf occur in correct ordering through usage of barriers
#define orderedPrintf(str, ...)                                                               \
    for (int orderedPrintf_loop_index = 0; orderedPrintf_loop_index < omp_get_num_threads();  \
         ++orderedPrintf_loop_index)                                                          \
    {                                                                                         \
        if (omp_get_thread_num() == orderedPrintf_loop_index)                                 \
        {                                                                                     \
            printf("Thread %i: ", omp_get_thread_num());                                      \
            printf(str, __VA_ARGS__);                                                         \
        }                                                                                     \
        _Pragma("omp barrier")                                                                \
    }

#endif /* TESTS_SRC_TESTS_H_ */
