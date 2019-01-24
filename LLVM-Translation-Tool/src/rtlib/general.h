#ifndef RTLIB_GENERAL_H_
#define RTLIB_GENERAL_H_

#include "rtlib_typedefs.h"

// suggest to not inline this
#define REDUCTION_MESSAGE_TAG 12345

// suggest to inline this
void init_mpi();
// suggest to inline this
void finalize_mpi();

// when user explicitly calls it we will still just use the mpi functions
// suggest to inline this
int mpi_rank();

// suggest to inline this
int mpi_size();

// suggest to inline this
void mpi_barrier();

// suggest to  inline this
void mpi_allreduce(void *buffer, int count, int mpi_type, int operation);

// suggest to inline this
void mpi_create_dynamic_window(MPI_Win *win);

// suggest to inline this
int calculate_for_boundaries_int(int *lower, int *upper, int *stride, int mpi_rank,
                                 int mpi_size);

// suggest to inline this
long calculate_for_boundaries_long(long *lower, long *upper, long *stride, int mpi_rank,
                                   int mpi_size);
// manuelles reduce
// suggest to not inline this
void mpi_manual_reduce(char *var, int data_size,
                       void (*func)(char *a, char *b, int *, MPI_Datatype *));

#endif /* RTLIB_GENERAL_H_ */
