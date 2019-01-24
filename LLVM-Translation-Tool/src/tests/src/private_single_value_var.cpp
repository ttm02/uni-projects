#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
    int a = 0;

    int b;
#pragma omp parallel shared(a) private(b)
    {
#pragma omp critical
        a += omp_get_thread_num(); // guard update
#pragma omp barrier
        b = a + omp_get_thread_num();
#pragma omp barrier
        orderedPrintf("a : %.d      b : %d\n", a, b);
    }
}
