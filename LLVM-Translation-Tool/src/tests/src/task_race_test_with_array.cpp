#include <assert.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// from rtlib:
// helper function that allocates a N dimensional contigous array with malloc
// void* array is array base ptr (null on worker)
// array_dim is dimension of array
// size_of_dim is array of size DIM that indicate size of each array dim
// elem_size is the size for each element
// retruns base ptr
void *alloc_array(int array_dim, int *size_of_dim, size_t elem_size)
{
    int i, j;
    int layer_elem_count;
    assert(array_dim > 0);
    assert(*size_of_dim > 0);
    assert(elem_size > 0);

    if (array_dim == 1) // trivial:
    {
        return malloc(*size_of_dim * elem_size);
    }
    else // FUN:
    {
        // pointer to each array layer:
        void ***dim_ptr = (void ***)malloc(array_dim * sizeof(void *));

        // allocate each "middle" layer
        for (i = 0; i < array_dim - 1; ++i)
        {
            assert(size_of_dim[i] > 0);
            layer_elem_count = size_of_dim[i];
            for (j = 0; j < i; ++j)
            {
                layer_elem_count = layer_elem_count * size_of_dim[j];
            }
            dim_ptr[i] = (void **)malloc(layer_elem_count * sizeof(void *));
            // printf("layer:%d count %d\n", i, layer_elem_count);
        }

        // allocate data layer
        i = array_dim - 1;
        assert(size_of_dim[i] > 0);
        layer_elem_count = size_of_dim[i];
        for (j = 0; j < i; ++j)
        {
            layer_elem_count = layer_elem_count * size_of_dim[j];
        }
        dim_ptr[i] = (void **)malloc(layer_elem_count * elem_size);
        // printf("final layer:%d count %d\n", i, layer_elem_count);

        // initialize middle layers:
        for (i = 0; i < array_dim - 2; ++i)
        {
            layer_elem_count = size_of_dim[i];
            for (j = 0; j < i; ++j)
            {
                layer_elem_count = layer_elem_count * size_of_dim[j];
            }
            // for each elem in this layer
            int offset = size_of_dim[i + 1];
            int current_pos = 0;
            // offset for next layer
            for (j = 0; j < layer_elem_count; ++j)
            {
                // total pointer FUN:
                dim_ptr[i][j] = (void *)&dim_ptr[i + 1][current_pos];
                current_pos += offset;
            }
        }
        // initialize second lowest layer with ptr to data
        i = array_dim - 1 - 1;
        layer_elem_count = size_of_dim[i];
        for (j = 0; j < i; ++j)
        {
            layer_elem_count = layer_elem_count * size_of_dim[j];
        }
        // for each elem in this layer
        int offset = size_of_dim[i + 1] * elem_size;
        int current_pos = 0;
        // offset for next layer
        for (j = 0; j < layer_elem_count; ++j)
        {
            // even more FUN than above:
            dim_ptr[i][j] = (void *)&((char *)(dim_ptr[i + 1]))[current_pos];
            current_pos += offset;
        }

        // return first layer
        void *result = (void *)dim_ptr[0];
        // free temp mem:
        free(dim_ptr);

        return result;
    }
}

// also from rtlib
void free_array(void *array, int array_dim)
{
    void **next_layer = (void **)array;
    void **this_layer = (void **)array;
    for (int i = 0; i < array_dim - 1; ++i)
    {
        this_layer = next_layer;
        next_layer = (void **)this_layer[0];
        free(this_layer);
    }
    // final layer
    free(next_layer);
}

#define MESS_UP_TIMING
// #define LONG_TEST
// so that executing all testcases will not take so long

int main()
{
    // there might be conflicts when some processes want to dequeue tasks at the same time
    // ore when some processes arrive at task_synk with no more task available
    // this might lead to deadlock in sync_task or a task executed twice or get skipped

    // Therefore, this test should be run multiple times to ensure that the race-conditions are
    // at least very unlikely, as this should test that the implementation will prevent them

    // TODO ?!!? when running with more then 2 procs Valgrind reports lots of uninitialized
    // vals. But all uninitialized Vals originated from MPI_Init

    srand(time(NULL));
    const int DIM = 2;
#ifdef LONG_TEST
    const int N = 5000;
#else
    const int N = 500; // this means way more array lines then threads
#endif
    const int S = 4;
#ifdef LONG_TEST
    // const int RUNS = 10000;
    const int RUNS = 1000;
#else
    const int RUNS = 100;
#endif

    int *dims = (int *)malloc(DIM * sizeof(int));

    dims[0] = N;
    for (int i = 1; i < DIM; ++i)
    {
        dims[i] = S;
    }
    int **Mat = (int **)alloc_array(DIM, dims, sizeof(int));
    free(dims);

    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < S; j++)
        {
            Mat[i][j] = 0;
        }
    }

#pragma omp parallel shared(Mat)
    {
#pragma omp single
        {
            for (int r = 0; r < RUNS; ++r)
            {
                for (int i = 0; i < N; ++i)
                {
                    // create one task for each row:
#pragma omp task shared(Mat) firstprivate(i)
                    {
                        for (int j = 0; j < S; ++j)
                        {
                            Mat[i][j] += i * 10 + j;
                        }
#ifdef MESS_UP_TIMING
                        int random_time = rand() % 3;
                        usleep(random_time);
#endif
                    }
                }
                // sync at the end of each run (so that no race conditions inside the array may
                // occur)
#pragma omp taskwait
#ifdef LONG_TEST
                printf("run %d\n", r);
#endif
            }
        } // implicit barrier
    }

    printf("begin of Matrix:\n");
    for (int i = 0; i < S; i++)
    {
        printf("[%d,%d,%d,%d]\n", Mat[i][0], Mat[i][1], Mat[i][2], Mat[i][3]);
    }

    bool integrity = true;
    // check integrity of matrix:
    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < S; ++j)
        {
            integrity = integrity && Mat[i][j] == RUNS * (i * 10 + j);
        }
    }

    if (integrity)
    {
        printf("Matrix has the expected Values\n");
    }
    else
    {
        printf("ERROR: some tasks wehre executed not the correct amount of times!!\n");
        for (int i = 0; i < N; i++)
        {
            printf("[%d,%d,%d,%d]\n", Mat[i][0], Mat[i][1], Mat[i][2], Mat[i][3]);
        }
    }

    free_array(Mat, DIM);
}
