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
    const int N = 2;
    const int S = 10;
    double **Mat_in;
    double **Mat_out;
    int *dims = (int *)malloc(N * sizeof(int));

    for (int i = 0; i < N; ++i)
    {
        dims[i] = S;
    }
    Mat_in = (double **)alloc_array(N, dims, sizeof(double));
    Mat_out = (double **)alloc_array(N, dims, sizeof(double));

    free(dims);

    for (int i = 0; i < S; i++)
    {
        for (int j = 0; j < S; j++)
        {
            Mat_in[i][j] = 0;
            Mat_out[i][j] = 0;
        }
    }

#pragma omp parallel shared(Mat_in, Mat_out)
    {
#pragma omp for
        for (int i = 1; i < S - 1; i++)
        {
            for (int j = 1; j < S - 1; j++)
            {
                double star = 0.25 * (Mat_in[i - 1][j] + Mat_in[i][j - 1] + Mat_in[i][j + 1] +
                                      Mat_in[i + 1][j]);

                // interference function
                star += 0.5;
                Mat_out[i][j] = star;
            }
        }
        // iteration 2: swap in and out
#pragma omp for
        for (int i = 1; i < S - 1; i++)
        {
            for (int j = 1; j < S - 1; j++)
            {
                double star = 0.25 * (Mat_out[i - 1][j] + Mat_out[i][j - 1] +
                                      Mat_out[i][j + 1] + Mat_out[i + 1][j]);

                // interference function
                star += 0.5;
                Mat_in[i][j] = star;
            }
        }
        // iteration 3: swap in and out
#pragma omp for
        for (int i = 1; i < S - 1; i++)
        {
            for (int j = 1; j < S - 1; j++)
            {
                double star = 0.25 * (Mat_in[i - 1][j] + Mat_in[i][j - 1] + Mat_in[i][j + 1] +
                                      Mat_in[i + 1][j]);

                // interference function
                star += 0.5;
                Mat_out[i][j] = star;
            }
        }
    }

    printf("Matrix_out:\n");
    for (int i = 0; i < S; i++)
    {
        printf("[");

        for (int j = 0; j < S; j++)
        {
            printf("%f,", Mat_out[i][j]);
        }
        printf("]\n");
    }

    free_array(Mat_in, N);
    free_array(Mat_out, N);
}
