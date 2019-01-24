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
    } // implicit "barrier"!

    int b = 0;
    printf("Master Only : %.2f      b : %d\n", a, b);

#pragma omp parallel
    {
#pragma omp critical
        b += omp_get_thread_num();
#pragma omp barrier
        orderedPrintf("a : %.2f      b : %d\n", a, b);
    }
    // implicit "barrier"!

    printf("Master Only : %.2f      b : %d\n", a, b);
}
