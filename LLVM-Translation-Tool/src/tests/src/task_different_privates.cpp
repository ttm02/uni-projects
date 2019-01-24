#include <omp.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    int a = 0;
    double b = 10;
    int c = 0;
    double d = 10;

#pragma omp parallel shared(a, b, c, d)
    {

#pragma omp single
        {
#pragma omp task shared(a, b) firstprivate(c, d)
            {
#pragma omp critical
                {
                    ++a;
                    b = b * 2.5;
                    c = c + 12;
                    d = d * 4.5;
                }
            }

#pragma omp taskwait

            // now the other Vars are shared:
#pragma omp task firstprivate(a, b) shared(c, d)
            {
#pragma omp critical
                {
                    ++a;
                    b = b * 2.5;
                    c = c + 12;
                    d = d * 4.5;
                }
            }
        }
    }

    printf("a:%d b:%f c:%d d:%f\n", a, b, c, d);
    // printf("a:%d b:%f\n", a, b);
}
