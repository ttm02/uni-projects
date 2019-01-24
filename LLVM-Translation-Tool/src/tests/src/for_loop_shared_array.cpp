#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "tests.h"

int main()
{
    double *a = (double *)malloc(8 * sizeof(double));

    for (int i = 0; i < 8; i++)
    {
        a[i] = 0.0;
    }
#pragma omp parallel
    {
#pragma omp for
        {
            for (int i = 0; i < 8; i++)
            {
                // if (i == omp_get_thread_num())
                //{
                a[i] = i;
                //}
            }
        }
#pragma omp barrier
        orderedPrintf("a: [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f]\n", a[0], a[1],
                      a[2], a[3], a[4], a[5], a[6], a[7]);
    }

    free(a);
}
