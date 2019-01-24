#include "rtlib.h"

// creates an MPI_Win exposing the the local buffer
// suggest to inline this
void init_single_value_comm_info(void *comm_info, size_t buffer_type_size)
{
    single_value_info *info =
        (single_value_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    // if this is more complex one may want to use a different function for that (see arrays)
    MPI_Win_create(comm_info, buffer_type_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &info->win);
}

void empty_test_function(single_value_info *info)
{
    // DO NOT CALL IT!
    // just to enforce that definition of single_value_info struct is not optimized out
    MPI_Win_free(&info->win);
}

// suggest to inline this
void free_single_value_comm_info(void *comm_info, size_t buffer_type_size)
{
    single_value_info *info =
        (single_value_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    MPI_Win_free(&info->win);
}

// fir shared reading communication pattern
// suggest to inline this
void init_single_value_comm_info_shared_reading(void *comm_info, size_t buffer_type_size)
{
    single_value_info *info =
        (single_value_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    MPI_Bcast(comm_info, buffer_type_size, MPI_BYTE, 0, MPI_COMM_WORLD);
    // if this is more complex one may want to use a different function for that (see arrays)
    MPI_Win_create(comm_info, buffer_type_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &info->win);
}

// suggest to inline this
void mpi_store_single_value_var_shared_reading(void *comm_info, size_t buffer_type_size)
{
    single_value_info *info =
        (single_value_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    mpi_one_sided_bcast(comm_info, 0, &info->win, buffer_type_size, MPI_BYTE);
}

// suggest to inline this
void mpi_store_single_value_var(void *comm_info, size_t buffer_type_size)
{
    int tgt_rank = 0;
    if (my_rank != tgt_rank)
    {
        single_value_info *info =
            (single_value_info
                 *)((char *)comm_info +
                    buffer_type_size); // offset to acces the communication info struct

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, tgt_rank, 0, info->win);
        MPI_Put(comm_info, buffer_type_size, MPI_BYTE, tgt_rank, 0, buffer_type_size, MPI_BYTE,
                info->win);
        MPI_Win_unlock(tgt_rank, info->win);
    }
}

// suggest to inline this
void mpi_load_single_value_var(void *comm_info, size_t buffer_type_size)
{
    int tgt_rank = 0;
    if (my_rank != tgt_rank)
    {
        single_value_info *info =
            (single_value_info
                 *)((char *)comm_info +
                    buffer_type_size); // offset to acces the communication info struct

        MPI_Win_lock(MPI_LOCK_SHARED, tgt_rank, 0, info->win);
        MPI_Get(comm_info, buffer_type_size, MPI_BYTE, tgt_rank, 0, buffer_type_size, MPI_BYTE,
                info->win);
        MPI_Win_unlock(tgt_rank, info->win);
    }
}
