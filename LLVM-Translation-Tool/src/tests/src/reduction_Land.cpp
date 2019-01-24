#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

int main()
{
    int reduce = 1;

#pragma omp parallel reduction(&& : reduce)
    {
        int thread = omp_get_thread_num();
        reduce = thread % 2;
    }
// output also should be parallel in openmp version
#pragma omp parallel
    {
        orderedPrintf("reduced to value %i\n", reduce);
    }
}
