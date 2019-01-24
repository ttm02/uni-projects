#ifndef RTLIB_UTILITY_H_
#define RTLIB_UTILITY_H_

#include "rtlib_typedefs.h"

// for debug purpouses
void print_int(int a);
void print_long(long a);
void print_int_ptr(int *a);
void print_array(int *array);

void mpi_one_sided_bcast(void *array, int displacement, MPI_Win *win, int count, int datatype);

// to bcast more than 2GiB
void mpi_bcast_byte_long(void *buffer, long count,
                         /*MPI_Datatype datatype=MPI_BYTE,*/ int root, MPI_Comm comm);

#endif /* RTLIB_UTILITY_H_ */
