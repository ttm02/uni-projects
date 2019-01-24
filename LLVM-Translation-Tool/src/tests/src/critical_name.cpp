#include <omp.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    int i;
#pragma omp parallel shared(i)
    {
        int thread = omp_get_thread_num();

        if (thread == 0)
        {
            sleep(0);
        }
        if (thread == 1)
        {
            sleep(1);
        }
        if (thread == 2)
        {
            sleep(4);
        }
        if (thread == 3)
        {
            sleep(5);
        }
        // ordering: 0,1,   2,3
        printf("Thread %i before critical\n", thread);
        if (thread % 2 == 0)
        {
#pragma omp critical(even)
            {
                if (thread == 0)
                {
                    sleep(2);
                }
                if (thread == 2)
                {
                    usleep(10);
                }
            }
        }
        else
        {
#pragma omp critical(odd)
            {
                if (thread == 1)
                {
                    usleep(10);
                }
                if (thread == 3)
                {
                    usleep(2);
                }
            }
        }
        // ordering: 1,0, 2,3
        // if critical does not work 0 will "overtake" 2
        printf("Thread %i after critical\n", thread);
    }
    /*
    // from the standard
    An optional name may be used to identify the critical construct. All
    critical constructs without a name are considered to have the same unspecified name.
    A thread waits at the beginning of a critical region until no thread in the
    contention group is
                 */
}
