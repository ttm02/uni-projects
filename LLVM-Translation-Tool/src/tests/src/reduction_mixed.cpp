#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

int main()
{
    int maximum = 0;
    int sumI = 0;
    double sumD = 0;

#pragma omp parallel reduction(+ : sumI, sumD) reduction(max : maximum)
    {
        int thread = omp_get_thread_num();
        sumI = thread;
        // printf("Thread %i sets value %i\n",thread,reduce);
        sumD = (sumI + 1) * 10.5;
        maximum = (thread + 10) * 12;
    }
// output also should be parallel in openmp version
#pragma omp parallel
    {
        orderedPrintf("reduced to values %i %.2f %i\n", sumI, sumD, maximum);
    }
}
