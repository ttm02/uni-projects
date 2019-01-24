#include <omp.h>
#include <stdio.h>
#include <unistd.h>

int main()
{

#pragma omp parallel
    {
        int thread = omp_get_thread_num();

#pragma omp sections
        {
#pragma omp section
            {
                printf("Thread %i does section 1\n", thread);
                sleep(1);
            }
#pragma omp section
            {
                printf("Thread %i does section 2\n", thread);
                sleep(1);
            }
        }
        // printf("Hello from Thread %i\n", thread);
    }
}

// TODO Revisit this testcase it may cause different results as standard does not guarantee
// which thread executes which case
