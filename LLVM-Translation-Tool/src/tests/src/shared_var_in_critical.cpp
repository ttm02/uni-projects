#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

double global = 42;
int main()
{
    int local = 0;
#pragma omp parallel shared(local)
    {
        int thread = omp_get_thread_num();

        // any order
#pragma omp critical
        {
            if (thread % 2 == 0)
            {
                local += 42; // only 2 updates here
            }
        }
#pragma omp critical
        {
            if (thread % 2 != 0)
            {
                global = global / 2; // only 2 updates here
            }
        }
#pragma omp barrier
        orderedPrintf("local:%i      global:%.2f\n", local, global);
    }
}
