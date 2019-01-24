#include <assert.h>
#include <omp.h>
#include <stdio.h>

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

int main()
{
    const int N = 2;
    const int S = 4;
    int **Mat;
    int *dims = (int *)malloc(N * sizeof(int));

    for (int i = 0; i < N; ++i)
    {
        dims[i] = S;
    }
    Mat = (int **)alloc_array(N, dims, sizeof(int));
    free(dims);

    for (int i = 0; i < S; i++)
    {
        for (int j = 0; j < S; j++)
        {
            Mat[i][j] = 0;
        }
    }

#pragma omp parallel for shared(Mat)
    {
        for (int i = 0; i < S; i++)
        {
            int rank = omp_get_thread_num();
            Mat[i][i] = rank;
            Mat[i][0] += rank;
            Mat[i][3] = 42;
        }
    }

    printf("Sequential Part: Matrix:\n");
    for (int i = 0; i < S; i++)
    {
        printf("[%d,%d,%d,%d]\n", Mat[i][0], Mat[i][1], Mat[i][2], Mat[i][3]);
    }

#pragma omp parallel shared(Mat)
    {
#pragma omp for
        for (int i = 0; i < S; i++)
        {
            int rank = omp_get_thread_num();
            Mat[i][i] = rank;
            Mat[i][0] += rank;
            Mat[i][3] = 42;
        } // implicit barrier

        if (omp_get_thread_num() == 1)
        {
            printf("second Part: Rank 1s view of Matrix:\n");
            for (int i = 0; i < S; i++)
            {
                printf("[%d,%d,%d,%d]\n", Mat[i][0], Mat[i][1], Mat[i][2], Mat[i][3]);
            }
        }
    }

    free_array(Mat, N);
}
