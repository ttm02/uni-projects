#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
    double a = 0.0;

#pragma omp parallel
    {
#pragma omp sections
        {
#pragma omp section
            {
#pragma omp critical
                a += 3.14;
            }
#pragma omp section
            {
#pragma omp critical
                a += 42;
            }
        }
#pragma omp barrier
        orderedPrintf("a: %.2f\n", a);
    }
}
