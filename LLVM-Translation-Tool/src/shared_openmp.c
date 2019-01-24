#include <omp.h>
#include <stdio.h>

struct data
{
    int** Mat;
    int* M;
};

void allocate_mat(struct data *d)
{
    d->Mat = malloc(4*sizeof(int*));
    d->M = malloc(4*4*sizeof(int));

    for(int i = 0; i < 4; i++)
    {
        d->Mat[i] = d->M + i*4;
    }


    return;
}

void test2()
{
    int** M2 = malloc(4*sizeof(int*));
    for(int i = 0; i < 4; i++)
    {
        M2[i] = malloc(4*sizeof(int));
    }

    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            M2[i][j] = 0;
        }
    }

#pragma omp parallel for
    {
        for(int i = 0; i < 4; i++)
        {
            M2[i][i] = 42;
            M2[i][i] += 42;
        }
    }

    for(int i = 0; i < 4; i++)
    {
        printf("%d : [%d,%d,%d,%d]\n",omp_get_thread_num(), M2[i][0], M2[i][1], M2[i][2], M2[i][3]);
    }

}


int main()
{
    struct data d;
    allocate_mat(&d);

    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            d.Mat[i][j] = 0;
        }
    }

    int** Mat = d.Mat;

#pragma omp parallel for 
    {
        for(int i = 0; i < 4; i++)
        {
            int rank = omp_get_thread_num();
            Mat[i][i] = rank;
            Mat[i][i] += rank;
        }
    }

    for(int i = 0; i < 4; i++)
    {
        printf("%d : [%d,%d,%d,%d]\n",omp_get_thread_num(), Mat[i][0], Mat[i][1], Mat[i][2], Mat[i][3]);
    }

    test2();

}
