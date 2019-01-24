#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

int main()
{
    int reduce = 0;

#pragma omp parallel reduction(max : reduce)
    {
        int thread = omp_get_thread_num();
        reduce = thread;
    }
// output also should be parallel in openmp version
#pragma omp parallel
    {
        orderedPrintf("reduced to value %i\n", reduce);
    }
}
