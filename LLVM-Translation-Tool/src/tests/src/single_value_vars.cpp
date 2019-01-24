#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
    double a = 0.0;

#pragma omp parallel
    {
#pragma omp critical
        a += 3.14;
    }

    int b = 0;

#pragma omp parallel
    {
#pragma omp critical
        b += omp_get_thread_num();
#pragma omp barrier
        orderedPrintf("a : %.2f      b : %d\n", a, b);
    }
}
