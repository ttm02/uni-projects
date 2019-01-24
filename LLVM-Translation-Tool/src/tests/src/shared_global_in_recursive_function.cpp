#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

int global = 0;

void foo(int arg)
{
    if (arg != 0)
    {
#pragma omp critical
        global++;
        foo(arg - 1);
    }
}

int main()
{

#pragma omp parallel shared(global)
    {
        foo(4);
#pragma omp barrier
        orderedPrintf("global: %i\n", global);
    }
}
