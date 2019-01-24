#include <omp.h>
#include <stdio.h>

#include "tests.h"

int main()
{
    double a, b;
    a = 0;
    b = 0;

#pragma omp parallel for lastprivate(a) shared(b)
    {
        for (int i = 0; i < 12; i++)
        {
#pragma omp critical
            b += 42.42;          // update to b is guarded
            a = (i + 1) * 42.42; // a a is private: no need to guard it
        }
    }

    // a should equal b

#pragma omp parallel
    {
#pragma omp barrier
        orderedPrintf("a : %.2f      b : %.2f\n", a, b);
    }
}
