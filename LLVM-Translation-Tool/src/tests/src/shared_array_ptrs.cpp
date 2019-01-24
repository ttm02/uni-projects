#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
    int *a = new int[8];

    for (int i = 0; i < 8; i++)
    {
        a[i] = 0;
    }

#pragma omp parallel
    {
        int rank = omp_get_thread_num();

        a[rank] += rank;
        a[rank + 4] = 42;

#pragma omp barrier
        orderedPrintf("a: [%d, %d, %d, %d, %d, %d, %d, %d]\n", a[0], a[1], a[2], a[3], a[4],
                      a[5], a[6], a[7]);
    }
}

// TODO This case is currently not supported
