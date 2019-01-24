#include "rtlib.h"
#include <limits.h>

// suggest to not inline this printer funcs
void print_int(int a) { printf("rank %d: print: %d\n", my_rank, a); }

void print_long(long a) { printf("rank %d: print: %ld\n", my_rank, a); }

void print_int_ptr(int *a) { printf("rank %d: print: %d\n", my_rank, *a); }

void print_array(int *array) { printf("rank %d: print: %d\n", my_rank, array[0]); }

// a Bcast using one sided communication
// not a collective operation (one sided)
// therefore: recomended to only use within a critical region
// it takes array and write it into win for all processes (including self)
// suggest to inline this
void mpi_one_sided_bcast(void *array, int displacement, MPI_Win *win, int datatype)
{
    MPI_Win_lock_all(0, *win);
    for (int i = 0; i < numprocs; ++i)
    {
        if (i != my_rank)
        {
            MPI_Put(array, 1, datatype, i, displacement, 1, datatype, *win);
        }
    }
    MPI_Win_unlock_all(*win);
}

void mpi_one_sided_bcast(void *array, int displacement, MPI_Win *win, int count, int datatype)
{
    MPI_Win_lock_all(0, *win);
    for (int i = 0; i < numprocs; ++i)
    {
        if (i != my_rank)
        {
            MPI_Put(array, count, datatype, i, displacement, count, datatype, *win);
        }
    }
    MPI_Win_unlock_all(*win);
}

// to bcast more than 2GiB
void mpi_bcast_byte_long(void *buffer, long count,
                         /*MPI_Datatype datatype=MPI_BYTE,*/ int root, MPI_Comm comm)
{
    long left = count;
    long disp = 0;
    char *char_buf = (char *)buffer; // so that base unit is byte
    while (left > 0)
    {
        long sendcount = (left > INT_MAX) ? INT_MAX : left;
        int sendcount_i = sendcount;
        MPI_Bcast(&char_buf[disp], sendcount_i, MPI_BYTE, root, comm);
        disp += sendcount;
        left -= sendcount;
    }
}
