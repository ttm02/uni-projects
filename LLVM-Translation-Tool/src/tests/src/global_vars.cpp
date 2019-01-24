#include <omp.h>
#include <stdio.h>

#include "tests.h"

double a = 0.0;
int *b = nullptr;

int main()
{
    a = 0.0;
    b = new int;
    *b = 0;

#pragma omp parallel
    {
#pragma omp critical
        a += 3.14;
    }

#pragma omp parallel
    {
#pragma omp critical
        *b += omp_get_thread_num();

#pragma omp barrier
        orderedPrintf("a : %.2f      b : %d\n", a, *b);
    }
    delete b;
}
