#include <omp.h>
#include <stdio.h>

#include "tests.h"

// currently : arrays may only be syncronized correctly with barriers, criticals are not enough
// as cached view may not be consistent then
// *a is considered an array with one element.
int main()
{
    double *a = new double;
    *a = 0.0;

#pragma omp parallel
    {
        for (int i = 0; i < omp_get_num_threads(); ++i)
        {
            if (omp_get_thread_num() == i)
            {
                *a += 3.14;
            }
#pragma omp barrier
        }
    }

    int *b = new int;
    *b = 0;

#pragma omp parallel
    {
        for (int i = 0; i < omp_get_num_threads(); ++i)
        {
            if (omp_get_thread_num() == i)
            {
                *b += i;
            }
#pragma omp barrier
        }
#pragma omp barrier
        orderedPrintf("a : %.2f      b : %d\n", *a, *b);
    }
}
