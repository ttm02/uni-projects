#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

int main()
{
    int reduce = 0;
    unsigned long b = 0;

#pragma omp parallel reduction(^ : reduce, b)
    {
        int thread = omp_get_thread_num();
        reduce = 1 << thread;
        b = 1 << thread;
        b = b | 1 << thread * 3;
    }
// output also should be parallel in openmp version
#pragma omp parallel
    {
        orderedPrintf("reduced to values %i %lu\n", reduce, b);
    }
}
