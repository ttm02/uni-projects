#include <omp.h>
#include <stdio.h>

int main()
{

    int a = 0;

#pragma omp parallel for shared(a)
    for (int i = 0; i < 10; i++)
    {
        a += 1;
    }

    printf("a: %d\n", a);
}
