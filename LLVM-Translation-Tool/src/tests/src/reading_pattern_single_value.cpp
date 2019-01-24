#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
#pragma omp2mpi comm reading
    double a = 0.0;

#pragma omp parallel
    {
#pragma omp critical
        a += 3.14;
        // small amount of writes, large amount of reads
#pragma omp barrier

        double sum = 0;
        for (int i = 0; i < 100; ++i)
        {
            sum += a;
        }
#pragma omp barrier
        orderedPrintf("a : %.2f   sum : %.2f\n", a, sum);
    }
}
