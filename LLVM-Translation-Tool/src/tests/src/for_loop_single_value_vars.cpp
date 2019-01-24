#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
    double a = 0.0;

#pragma omp parallel
    {
#pragma omp for
        {
            for (int i = 0; i < 12; i++)
            {
#pragma omp critical
                a += i; // need to guard this update
                // atomic is currently not supported by the pass
            }
        }
#pragma omp barrier
        orderedPrintf("a: %.2f\n", a);
    }
}
