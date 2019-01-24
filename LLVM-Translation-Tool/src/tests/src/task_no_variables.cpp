#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

int main()
{
#pragma omp parallel
    {
#pragma omp single

        {
#pragma omp task
            {
                printf("hello world\n");
                sleep(1); // to test taskwait
            }

#pragma omp taskwait

#pragma omp task
            printf("hello again!\n");
        }
    }
}
