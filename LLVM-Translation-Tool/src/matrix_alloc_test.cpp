#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

    int DIM = 2;
    printf("allocatig 2D array\n");

    int *ndim = (int *)malloc(DIM * sizeof(int));
    ndim[0] = 4;
    ndim[1] = 4;

    int **array = (int **)alloc_array(DIM, ndim, sizeof(int));

    // fill
    array[0][0] = 1;
    array[0][1] = 2;
    array[0][2] = 3;
    array[0][3] = 4;

    array[1][0] = 5;
    array[1][1] = 6;
    array[1][2] = 7;
    array[1][3] = 8;

    array[2][0] = 9;
    array[2][1] = 10;
    array[2][2] = 11;
    array[2][3] = 12;

    array[3][0] = 13;
    array[3][1] = 14;
    array[3][2] = 15;
    array[3][3] = 16;

    printf("2d: succes!\n");

    // if filling succed, reading will also :-)

    free_array(array, DIM);

    free(ndim);
    printf("allocatig 10D array with wired size\n");

    DIM = 10;
    ndim = (int *)malloc(DIM * sizeof(int));

    for (int i = 0; i < DIM; ++i)
    {
        ndim[i] = 3 + i % 2;
    }

    long **********tenD = (long **********)alloc_array(10, ndim, sizeof(long));

    // first
    tenD[0][0][0][0][0][0][0][0][0][0] = 42;
    // last value
    tenD[2][3][2][3][2][3][2][3][2][3] = 42;
    // arbitrary
    tenD[0][1][0][1][1][1][2][3][0][1] = 42;

    printf("allocatig 10D success!\n");

    free_array(tenD, DIM);
    free(ndim);
}
