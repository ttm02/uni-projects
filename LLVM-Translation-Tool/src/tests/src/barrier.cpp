#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

int main()
{

#pragma omp parallel
    {
        int thread = omp_get_thread_num();

        if (thread == 0)
        {
            sleep(3);
        }
        if (thread == 1)
        {
            sleep(2);
        }
        if (thread == 2)
        {
            sleep(1);
        }
        if (thread == 3)
        {
            sleep(0);
        }
        // ordering: 3,2,1,0
        printf("Thread %i: before Barrier\n", thread);
#pragma omp barrier
        // ordering 0,1,2,3
        orderedPrintf("after Barrier\n", NULL);
    }
}
