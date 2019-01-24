#include <assert.h>
#include <omp.h>
#include <stdio.h>

// from rtlib
// helper function that allocates a N dimensional contigous array with malloc
// void* array is array base ptr (null on worker)
// array_dim is dimension of array
// size_of_dim is array of size DIM that indicate size of each array dim
// elem_size is the size for each element
// retruns base ptr
void *alloc_array(int array_dim, int *size_of_dim, size_t elem_size)
{
    int i, j;
    unsigned long layer_elem_count;
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
            unsigned long offset = size_of_dim[i + 1];
            unsigned long current_pos = 0;
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
        unsigned long offset = size_of_dim[i + 1] * elem_size;
        unsigned long current_pos = 0;
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

#pragma omp2mpi comm reading
long **Mat;

int main()
{
    const int N = 2;
    const int S = 20000; // array is larger than 3GB

    int *dims = (int *)malloc(N * sizeof(int));

    for (int i = 0; i < N; ++i)
    {
        dims[i] = S;
    }
    Mat = (long **)alloc_array(N, dims, sizeof(long));

    free(dims);

    for (int i = 0; i < S; i++)
    {
        for (int j = 0; j < S; j++)
        {
            Mat[i][j] = i * S + j;
        }
    }

    long sum = 0;

#pragma omp parallel shared(Mat) reduction(+ : sum)
    {
#pragma omp for
        for (int i = 0; i < S; i++)
        {
            for (int j = 0; j < S; j++)
            {
                sum = sum + Mat[i][j];
            }
        }
    }

    long elements = S * S - 1; // first summand is 0
    // gaussche summenformel (n*(n+1))/2 :
    long expected = (elements * elements + elements) / 2;
    double data_size = (sizeof(long) * elements / (double)1000000000);
    printf("Delta=%ld (with %f GB data)\n", (sum - expected), data_size);

    free_array(Mat, N);
}
