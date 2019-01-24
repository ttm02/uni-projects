#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

// Reduktion muss Assoziativ und Kommutativ sein, damit es auch sicher das gleiche wie bei
// OpenMP gibt
int myreduction(int r, int n)
{
    // r is the already reduced value
    // n is the new value
    return r + n + 1;
}
#pragma omp declare reduction(usrDefined:int : omp_out = myreduction(omp_out, omp_in))

int main()
{
    int reduce = 1;

#pragma omp parallel reduction(usrDefined : reduce)
    {
        reduce = omp_get_thread_num();
    }
// output also should be parallel in openmp version
#pragma omp parallel
    {
        orderedPrintf("reduced to value %i\n", reduce);
    }
}
