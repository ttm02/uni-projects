#include "tests.h"
#include <omp.h>
#include <stdio.h>
#include <unistd.h>

int global = 0;
#pragma omp threadprivate(global)

void foo(int arg)
{
    if (arg != 0)
    {
        global++;
        foo(arg - 1);
    }
}

int main()
{

#pragma omp parallel
    {
        int thread = omp_get_thread_num();
        global = thread;
        foo(2);
#pragma omp barrier
        orderedPrintf("global: %i\n", global);
    }
}
