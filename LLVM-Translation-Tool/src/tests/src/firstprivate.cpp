#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
    double a = 0.0;

#pragma omp parallel firstprivate(a)
    {
        a += 3.14;
    }
    // a should remain 0 on all threads

    int b = 0;

#pragma omp parallel private(b)
    {
        b = omp_get_thread_num();
#pragma omp barrier
        orderedPrintf("a : %.2f      b : %d\n", a, b);
    }
}
