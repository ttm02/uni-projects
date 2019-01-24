#include "tests.h"
#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    int reduce = INT_MAX;

#pragma omp parallel reduction(min : reduce)
    {
        int thread = omp_get_thread_num();
        reduce = thread + 42;
    }
// output also should be parallel in openmp version
#pragma omp parallel
    {
        orderedPrintf("reduced to value %i\n", reduce);
    }
}
