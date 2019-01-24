#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"
typedef struct
{
    double sum;
    double avg;
    int count;

} testtype;

// Reduktion muss Assoziativ und Kommutativ sein, damit es auch sicher das gleiche wie bei
// OpenMP gibt
testtype myreduction(testtype r, testtype n)
{
    // r is the already reduced value
    // n is the new value

    testtype result;
    result.sum = r.sum + n.sum;
    result.count = r.count + n.count;
    result.avg = result.sum / result.count;

    return result;
}
#pragma omp declare reduction(usrDefined:testtype : omp_out = myreduction(omp_out, omp_in))

int main()
{
    testtype reduce;
    reduce.sum = 0;
    reduce.count = 0;
    reduce.avg = 0;

#pragma omp parallel reduction(usrDefined : reduce)
    {
        int thread = omp_get_thread_num();
        reduce.sum = thread + 1;
        reduce.count = 1;
        reduce.avg = thread + 1;
    }
// output also should be parallel in openmp version
#pragma omp parallel
    {
        orderedPrintf("reduced to values %.2f %i %f\n", reduce.sum, reduce.count, reduce.avg);
    }
}
