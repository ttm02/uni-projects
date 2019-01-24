#include "rtlib.h"

// suggest to inline this
void init_mpi()
{
    int rank;
    int size;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    alloc_info = new struct memory_allocation_info();

    my_rank = rank;
    numprocs = size;
    rank_and_size = rank + size;
}
// suggest to inline this
void finalize_mpi()
{
    delete alloc_info;
    MPI_Finalize();
}

// when user explicitly calls it we will still just use the mpi functions
// suggest to inline this
int mpi_rank()
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return rank;
}

// suggest to inline this
int mpi_size()
{
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    return size;
}

// suggest to inline this
void mpi_barrier() { MPI_Barrier(MPI_COMM_WORLD); }

// TODO rename
// suggest to  inline this
void mpi_allreduce(void *buffer, int count, int mpi_type, int operation)
{
    // MPI_Allreduce(MPI_IN_PLACE, buffer, count, mpi_type, operation, MPI_COMM_WORLD);
    int root = 0;
    if (my_rank == 0)
    {
        MPI_Reduce(MPI_IN_PLACE, buffer, count, mpi_type, operation, root, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Reduce(buffer, nullptr, count, mpi_type, operation, root, MPI_COMM_WORLD);
    }
}

// suggest to inline this
void mpi_create_dynamic_window(MPI_Win *win)
{
    MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, win);
}

// suggest to inline this
int calculate_for_boundaries_int(int *lower, int *upper, int *stride, int mpi_rank,
                                 int mpi_size)
{
    Debug(printf("lower %d, upper %d, stride %d, mpi_rank %d, mpi_size %d\n", *lower, *upper,
                 *stride, mpi_rank, mpi_size);)

        int global_upper_bound = *upper;
    int global_lower_bound = *lower;

    int local_lower_bound, local_upper_bound;

    int trip_count = *upper - *lower + 1;
    int chunk_size = trip_count / mpi_size;
    int remainder = trip_count % mpi_size;

    if (*stride == 1)
    {
        local_lower_bound =
            *lower + (mpi_rank * chunk_size) + (mpi_rank < remainder ? mpi_rank : remainder);
        local_upper_bound = local_lower_bound + chunk_size - (mpi_rank < remainder ? 0 : 1);
        if (trip_count < mpi_size)
        {
            // this means not enough iterations for every process
            if (mpi_rank > trip_count)
            {
                // no iterations for this process (his range has 0 iterations)
                local_lower_bound = *upper + 1;
                local_upper_bound = *upper;
            }
        }
    }
    else
    {
        printf("This loop type is not supported yet.\n");
        printf("lower %d, upper %d, stride %d, mpi_rank %d, mpi_size %d\n", *lower, *upper,
               *stride, mpi_rank, mpi_size);
        local_lower_bound = 0;
        local_upper_bound = 0;
    }

    Debug(printf("new_lower %d, new_upper %d, mpi_rank %d\n", local_lower_bound,
                 local_upper_bound, mpi_rank);)

        *lower = local_lower_bound;
    *upper = local_upper_bound;

    return remainder;
}

// suggest to inline this
long calculate_for_boundaries_long(long *lower, long *upper, long *stride, int mpi_rank,
                                   int mpi_size)
{
    Debug(printf("lower %ld, upper %ld, stride %ld, mpi_rank %d, mpi_size %d\n", *lower,
                 *upper, *stride, mpi_rank, mpi_size);)

        long global_upper_bound = *upper;
    long global_lower_bound = *lower;

    long local_lower_bound, local_upper_bound;

    long trip_count = *upper - *lower + 1;
    long chunk_size = trip_count / mpi_size;
    long remainder = trip_count % mpi_size;

    if (*stride == 1)
    {
        local_lower_bound =
            *lower + (mpi_rank * chunk_size) + (mpi_rank < remainder ? mpi_rank : remainder);
        local_upper_bound = local_lower_bound + chunk_size - (mpi_rank < remainder ? 0 : 1);
        if (trip_count < mpi_size)
        {
            // this means not enough iterations for every process
            if (mpi_rank > trip_count)
            {
                // no iterations for this process (his range has 0 iterations)
                local_lower_bound = *upper + 1;
                local_upper_bound = *upper;
            }
        }
    }
    else
    {
        printf("This loop type is not supported yet.\n");
        printf("lower %ld, upper %ld, stride %ld, mpi_rank %d, mpi_size %d\n", *lower, *upper,
               *stride, mpi_rank, mpi_size);
        local_lower_bound = 0;
        local_upper_bound = 0;
    }

    Debug(printf("new_lower %ld, new_upper %ld, mpi_rank %d\n", local_lower_bound,
                 local_upper_bound, mpi_rank);)

        *lower = local_lower_bound;
    *upper = local_upper_bound;

    return remainder;
}

// suggest to not inline this
void recursive_tree_reduce(char *var, int data_size,
                           void (*func)(char *a, char *b, int *, MPI_Datatype *), int rank,
                           int from, int to)
{
    int tree_members = to - from;
    if (tree_members <= 3)
    {
        if (tree_members == 1)
        {
            // should never happen anyway
            printf("ERROR in Tree-Reduce\n");
            return;
        }
        // first in range will do the recv
        if (rank == from)
        {
            char *buffer = (char *)malloc(data_size);
            // fill buffer
            MPI_Recv(buffer, data_size, MPI_BYTE, from + 1, REDUCTION_MESSAGE_TAG,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // combine
            int len = data_size;
            MPI_Datatype datatype = MPI_BYTE;
            func(buffer, var, &len, &datatype);
            if (tree_members == 3)
            { // again for other rank in tree
                MPI_Recv(buffer, data_size, MPI_BYTE, from + 2, REDUCTION_MESSAGE_TAG,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                func(buffer, var, &len, &datatype);
            }
            free(buffer);
        }
        else
        {
            MPI_Send(var, data_size, MPI_BYTE, from, REDUCTION_MESSAGE_TAG, MPI_COMM_WORLD);
        }
    }
    else
    {
        // split the tree again
        int pivot = from + tree_members / 2;
        // only call the tree you are in
        if (rank >= pivot)
        {
            recursive_tree_reduce(var, data_size, func, rank, pivot, to);
        }
        else
        {
            recursive_tree_reduce(var, data_size, func, rank, from, pivot);
        }
        // reduce the two subtrees
        if (rank == pivot)
        {
            MPI_Send(var, data_size, MPI_BYTE, from, REDUCTION_MESSAGE_TAG, MPI_COMM_WORLD);
        }
        if (rank == from)
        {
            char *buffer = (char *)malloc(data_size);
            // fill buffer
            MPI_Recv(buffer, data_size, MPI_BYTE, pivot, REDUCTION_MESSAGE_TAG, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            // combine
            int len = data_size;
            MPI_Datatype datatype = MPI_BYTE;
            func(buffer, var, &len, &datatype);
            free(buffer);
        }
    }
}

// manuelles reduce
// suggest to not inline this
void mpi_manual_reduce(char *var, int data_size,
                       void (*func)(char *a, char *b, int *, MPI_Datatype *))
{
    const int TAG = 12345;

#ifdef USE_TREE_REDUCTION
    // tree
    recursive_tree_reduce(var, data_size, func, my_rank, 0, numprocs);
#else
    int len = data_size;
    MPI_Datatype datatype = MPI_BYTE;
    // master reduces all:
    if (my_rank == 0)
    {
        char *buffer = (char *)malloc(data_size);
        for (int i = 1; i < numprocs; ++i)
        {
            // fill buffer
            MPI_Recv(buffer, data_size, MPI_BYTE, i, REDUCTION_MESSAGE_TAG, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            // combine
            func(buffer, var, &len, &datatype);
        }
        free(buffer);
    }
    else
    {
        MPI_Send(var, data_size, MPI_BYTE, 0, REDUCTION_MESSAGE_TAG, MPI_COMM_WORLD);
    }
#endif
    // mimic allreduce
    // needed for struct reduction
    MPI_Bcast(var, data_size, MPI_BYTE, 0, MPI_COMM_WORLD);
}
