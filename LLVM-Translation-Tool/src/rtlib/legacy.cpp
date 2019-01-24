#include "rtlib.h"

// suggest to inline this
void mpi_bcast(void *buffer, int count, int mpi_type, int root)
{
    MPI_Bcast(buffer, count, mpi_type, root, MPI_COMM_WORLD);
}

// suggest to  inline this
void mpi_update_shared_var_before(int rank, int size, void *var, int datatype)
{
    if (rank != 0)
    {
        MPI_Recv(var, 1, datatype, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
}

// suggest to inline this
void mpi_update_shared_var_after(int rank, int size, void *var, int datatype)
{
    if (rank == 0)
    {
        MPI_Send(var, 1, datatype, rank + 1, 0, MPI_COMM_WORLD);
        MPI_Recv(var, 1, datatype, size - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    else
    {
        if (rank != size - 1)
        {
            MPI_Send(var, 1, datatype, rank + 1, 0, MPI_COMM_WORLD);
        }
        else
        {
            MPI_Send(var, 1, datatype, 0, 0, MPI_COMM_WORLD);
        }
    }
    MPI_Bcast(var, 1, datatype, 0, MPI_COMM_WORLD);
}

// suggest to inline this
void mpi_create_shared_array_window(void *array, int array_size, MPI_Win *win, int datatype)
{
    int size;
    MPI_Type_size(datatype, &size);
    MPI_Win_create(array, array_size, size, MPI_INFO_NULL, MPI_COMM_WORLD, win);
}

// suggest to inline this
void mpi_store_in_shared_array(void *array, int displacement, int rank, MPI_Win *win,
                               int datatype)
{
    if (rank != 0)
    {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, *win);
        MPI_Put(array, 1, datatype, 0, displacement, 1, datatype, *win);
        MPI_Win_unlock(0, *win);
    }
}

// suggest to inline this
void mpi_load_from_shared_array(void *array, int rank, int displacement, MPI_Win *win,
                                int datatype)
{
    MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, *win);
    MPI_Get(array, 1, datatype, 0, displacement, 1, datatype, *win);
    MPI_Win_unlock(0, *win);
}

// suggest to not inline this ?
void mpi_free_shared_array_window(void *array, int array_size, int rank, MPI_Win *win,
                                  int datatype)
{
    MPI_Win_fence(0, *win);
    if (rank != 0)
    {
        MPI_Get(array, array_size, datatype, 0, 0, array_size, datatype, *win);
    }
    MPI_Win_fence(0, *win);
    MPI_Win_free(win);
}

// suggest to inline this
void add_missing_recv_calls(int rank, int size, void *var, int datatype, int condition)
{
    if (rank >= condition && condition != 0)
    {
        mpi_update_shared_var_before(rank, size, var, datatype);
        mpi_update_shared_var_after(rank, size, var, datatype);
    }
}

// DECREPARTED:
// suggest to  inline this
long get_array_size_from_address(void *base_ptr)
{
    long size = alloc_info->allocs[base_ptr];

    Debug(printf("Array size: %ld\n", size);)

        return size;
}

// suggest to not inline this
void create_2d_array_window(void *base_ptr, int datatype, MPI_Win **window, long *window_count)
{
    int alloc_style = check_2d_array_memory_allocation_style(base_ptr);

    int type_size;
    MPI_Type_size(datatype, &type_size);

    long **p = (long **)base_ptr;

    if (alloc_style == CONTINUOUS)
    {
        long array_size = alloc_info->allocs[p[0]] / type_size;

        *window = (MPI_Win *)malloc(sizeof(MPI_Win));

        MPI_Win_create((void *)p[0], array_size, type_size, MPI_INFO_NULL, MPI_COMM_WORLD,
                       window[0]);
        Debug(printf("Creating MPI Window for 2d-array with array size of %ld\n",
                     array_size);) *window_count = 1;
        return;
    }
    else if (alloc_style == NON_CONTINUOUS)
    {
        *window_count = alloc_info->allocs[base_ptr] / sizeof(long *);
        Debug(printf("Creating %ld MPI windows for the array\n", *window_count);)

            *window = (MPI_Win *)malloc(*window_count * sizeof(MPI_Win));

        for (int i = 0; i < *window_count; i++)
        {
            long array_size = alloc_info->allocs[p[i]] / type_size;

            Debug(printf("Creating MPI Window for 2d-array with array size of %ld\n",
                         array_size);)
                MPI_Win_create((void *)p[i], array_size, type_size, MPI_INFO_NULL,
                               MPI_COMM_WORLD, &(*window)[i]);
        }
        return;
    }
    else
    {
        printf("Can't create MPI window for this array\n");
        return;
    }
}

// suggest to not inline this
void free_2d_array_window(MPI_Win **win, long *window_count, void *base_ptr, int datatype,
                          int rank)
{
    Debug(printf("Freeing MPI Windows, count: %ld\n", *window_count);) if (*window_count == 1)
    {
        Debug(printf("Window: %d\n", (*win)[0]);)

            int type_size;
        MPI_Type_size(datatype, &type_size);

        long **p = (long **)base_ptr;
        long size = alloc_info->allocs[p[0]] / type_size;

        MPI_Win_fence(0, (*win)[0]);
        if (rank != 0)
        {
            MPI_Get(p[0], size, datatype, 0, 0, size, datatype, (*win)[0]);
        }
        MPI_Win_fence(0, (*win)[0]);

        MPI_Win_free(&(*win)[0]);
    }
    else if (*window_count > 1)
    {
        int type_size;
        MPI_Type_size(datatype, &type_size);

        long **p = (long **)base_ptr;

        for (int i = 0; i < *window_count; i++)
        {
            Debug(printf("Window %d : %d\n", i, (*win)[i]);)

                long size = alloc_info->allocs[p[i]] / type_size;

            MPI_Win_fence(0, (*win)[i]);
            if (rank != 0)
            {
                MPI_Get(p[i], size, datatype, 0, 0, size, datatype, (*win)[i]);
            }
            MPI_Win_fence(0, (*win)[i]);

            MPI_Win_free(&(*win)[i]);
        }
        free(*win);
    }
    return;
}

// suggest to inline this ?
void store_in_2d_array_window(void *base_ptr, void *array, long idx1, long idx2, int rank,
                              MPI_Win **win, long *window_count, int datatype)
{
    Debug(printf("Store in 2d array window\n");)

        if (*window_count == 1)
    {
        long **p = (long **)base_ptr;
        long diff = (long)array - (long)p[0];

        int type_size;
        MPI_Type_size(datatype, &type_size);
        long displacement = diff / type_size;

        Debug(printf("displacement: %ld\n", displacement);)

            if (rank != 0)
        {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, (*win)[0]);
            MPI_Put(array, 1, datatype, 0, displacement, 1, datatype, (*win)[0]);
            MPI_Win_unlock(0, (*win)[0]);
        }
    }
    else
    {
        if (rank != 0)
        {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, (*win)[idx1]);
            MPI_Put(array, 1, datatype, 0, idx2, 1, datatype, (*win)[idx1]);
            MPI_Win_unlock(0, (*win)[idx1]);
        }
    }
}

// suggest to inline this ?
void load_from_2d_array_window(void *base_ptr, void *array, long idx1, long idx2, int rank,
                               MPI_Win **win, long *window_count, int datatype)
{
    Debug(printf("load from 2d array window\n");)

        if (*window_count == 1)
    {
        long **p = (long **)base_ptr;
        long diff = (long)array - (long)p[0];

        int type_size;
        MPI_Type_size(datatype, &type_size);
        long displacement = diff / type_size;

        Debug(printf("displacement: %ld\n", displacement);)

            MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, (*win)[0]);
        MPI_Get(array, 1, datatype, 0, displacement, 1, datatype, (*win)[0]);
        MPI_Win_unlock(0, (*win)[0]);
    }
    else
    {
        MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, (*win)[idx1]);
        MPI_Get(array, 1, datatype, 0, idx2, 1, datatype, (*win)[idx1]);
        MPI_Win_unlock(0, (*win)[idx1]);
    }
}

// suggest to inline this
void create_1d_array_window(void *base_ptr, MPI_Win *win, int datatype)
{
    Debug(printf("Create Window for 1d-array at address: %p\n", base_ptr);) int type_size;
    MPI_Type_size(datatype, &type_size);

    long array_size = alloc_info->allocs[base_ptr] / type_size;

    Debug(printf("array size: %ld\n", array_size);)

        MPI_Win_create(base_ptr, array_size, type_size, MPI_INFO_NULL, MPI_COMM_WORLD, win);
}

// suggest to inline this
void free_1d_array_window(void *base_ptr, int rank, MPI_Win *win, int datatype)
{
    Debug(printf("Freeing MPI window: %d\n", *win);)

        int type_size;
    MPI_Type_size(datatype, &type_size);

    long array_size = alloc_info->allocs[base_ptr] / type_size;

    Debug(printf("array size: %ld\n", array_size);)

        MPI_Win_fence(0, *win);
    if (rank != 0)
    {
        MPI_Get(base_ptr, array_size, datatype, 0, 0, array_size, datatype, *win);
    }
    MPI_Win_fence(0, *win);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Win_free(win);
}

// collective call: bcasts a shared array from the master
// void* array is array base ptr (null on worker)
// DIM is dimension of array
// NDIM is array of size DIM that indicate size of each array dim
// elem_size is the size for each element
// retruns base ptr for all threads (void* array input on master)
void *bcast_shared_array_from_master(void *array, int DIM, int *NDIM, size_t elem_size)
{
    void *result = array;

    int array_dim = DIM;
    // first: need to get DIM from master
    MPI_Bcast(&array_dim, 1, MPI_INT, 0, MPI_COMM_WORLD);
    // than NDIM
    int *size_of_dim = NDIM;
    if (my_rank != 0)
    {
        size_of_dim = (int *)malloc(array_dim * sizeof(int));
    }
    MPI_Bcast(size_of_dim, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (my_rank != 0)
    {
        // get a local buffer
        result = alloc_array(array_dim, size_of_dim, elem_size);
    }

    // TODO do the actual bcast
    // depending on master layout
    return nullptr;
}
