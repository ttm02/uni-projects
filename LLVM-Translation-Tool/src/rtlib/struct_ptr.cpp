#include "rtlib.h"

// fetch the shared struct so that the process can have a local pointer to it
// or use it to update teh local copy
// suggest to inline this
void mpi_load_shared_struct(void *buffer, struct mpi_struct_ptr *struct_ptr, MPI_Win *win)
{
    if (struct_ptr->rank != -1)
    {
        // expect 1: if false the missed branch will not be the big problem (RMA will be slow
        // anyway)
        if (__builtin_expect(
                (my_rank == struct_ptr->rank && (MPI_Aint)buffer == struct_ptr->displacement),
                1))
        {
            // TODO change this if it is allowed to only load/write part of a shared struct so
            // that no load from original copy occures
            Debug(printf("Rank %d is loading from original struct copy\n", my_rank);)
        }
        else
        {
            Debug(
                printf("Rank %d is loading form shared struct: buffer=%p rank=%d disp=%#08lx "
                       "size=%lu\n",
                       my_rank, buffer, struct_ptr->rank, struct_ptr->displacement,
                       struct_ptr->size);)

                int tgt_rank = struct_ptr->rank; // just to be secure that rank will not be
                                                 // overwritten by load from remote
            MPI_Win_lock(MPI_LOCK_SHARED, tgt_rank, 0, *win);
            MPI_Get(buffer, (int)struct_ptr->size, MPI_BYTE, tgt_rank,
                    struct_ptr->displacement, (int)struct_ptr->size, MPI_BYTE, *win);
            MPI_Win_unlock(tgt_rank, *win);
            Debug(printf("loaded content=%d\n", *(int *)buffer);)
        }
    }
    else
    {
        printf("ERROR: loading from nullptr\n");
    }
}

// store the pointer to a shared struct: update the pointer information
// suggest to inline this
void mpi_store_shared_struct_ptr(void *new_ptr, size_t size, struct mpi_struct_ptr *struct_ptr)
{
    if (new_ptr == nullptr)
    {
        struct_ptr->rank = -1;
        // actually not used then:
        MPI_Get_address(new_ptr, &struct_ptr->displacement);
        struct_ptr->size = size;
    }
    else
    {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        struct_ptr->rank = rank;
        MPI_Get_address(new_ptr, &struct_ptr->displacement);
        struct_ptr->size = size;
        Debug(printf("store shared struct ptr: rank=%d disp=%#08lx size%lu \n",
                     struct_ptr->rank, struct_ptr->displacement, struct_ptr->size);)
    }
}

// store the pointer to a shared struct: update the pointer information
// suggest to inline this
void mpi_copy_shared_struct_ptr(struct mpi_struct_ptr *copy_from,
                                struct mpi_struct_ptr *copy_to)
{
    if (copy_from == nullptr)
    {
        copy_to->rank = -1;
        copy_to->displacement = 0;
        copy_to->size = 0;
    }
    else
    {
        // man könnte auch einfach memcpy benutzen...
        // here: not using memcpy as this assignment should also work if somehow from and to
        // are idendical
        copy_to->rank = copy_from->rank;
        copy_to->displacement = copy_from->displacement;
        copy_to->size = copy_from->size;
    }
    Debug(printf("copy shared struct ptr: rank=%d disp=%#08lx size%lu to bufffer:%p \n",
                 copy_to->rank, copy_to->displacement, copy_to->size, copy_to);)
}

// determines if two struct ptr are equal
// suggest to inline this
bool mpi_cmp_EQ_shared_struct_ptr(struct mpi_struct_ptr *a, struct mpi_struct_ptr *b)
{
    if (a == nullptr && b != nullptr)
    {
        return b->rank == -1;
    }
    else if (a != nullptr && b == nullptr)
    {
        return a->rank == -1;
    }
    else if (a == nullptr && b == nullptr)
    {
        return true;
    }
    else if (a != nullptr && b != nullptr)
    {
        return (a->rank == b->rank && a->displacement == b->displacement &&
                a->size == b->size);
    }
    // should never happen:
    printf("Error in comparing struct Pointers. this should never happen\n");
    return false;
}
// determines if two struct ptr are not qual
// suggest to inline this
bool mpi_cmp_NE_shared_struct_ptr(struct mpi_struct_ptr *a, struct mpi_struct_ptr *b)
{
    return !mpi_cmp_EQ_shared_struct_ptr(a, b);
}

// load the pointer to a shared struct: returns the given local buffer or nullptr
// buffer has to be allocated by caller
// suggest to inline this
void *mpi_load_shared_struct_ptr(void *buffer, struct mpi_struct_ptr *struct_ptr)
{
    if (struct_ptr->rank == -1)
    {
        return nullptr;
    }
    else
    {
        return buffer;
    }
}

// update the remote copy of the shared struct
// this will flush the whole local copy therefore it is recomended to first fetch the remote
// content again, then do the actual stroe and finally update remote copy
// suggest to inline this
void mpi_store_to_shared_struct(void *buffer, struct mpi_struct_ptr *struct_ptr, MPI_Win *win)
{
    if (struct_ptr->rank != -1)
    {
        // expect 1: if false the missed branch will not be the big problem (RMA will be slow
        // anyway)
        if (__builtin_expect(
                (my_rank == struct_ptr->rank && (MPI_Aint)buffer == struct_ptr->displacement),
                1))
        {
            // TODO change this if it is allowed to only load/write part of a shared struct so
            // that no store to original copy occures
            Debug(printf("Rank %d is storing to original struct copy\n", my_rank);)
        }
        else
        {
            Debug(printf("Rank %d is storing to shared struct: buffer=%p rank=%d disp=%#08lx "
                         "size=%lu\n",
                         my_rank, buffer, struct_ptr->rank, struct_ptr->displacement,
                         struct_ptr->size);)

                MPI_Win_lock(MPI_LOCK_EXCLUSIVE, struct_ptr->rank, 0, *win);
            MPI_Put(buffer, (int)struct_ptr->size, MPI_BYTE, struct_ptr->rank,
                    struct_ptr->displacement, (int)struct_ptr->size, MPI_BYTE, *win);
            MPI_Win_unlock(struct_ptr->rank, *win);
        }
    }
    else
    {
        printf("ERROR: store to nullptr\n");
    }
}
