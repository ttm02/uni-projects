#ifndef RTLIB_SINGLEVALUE_H_
#define RTLIB_SINGLEVALUE_H_

#include "rtlib_typedefs.h"

// creates an MPI_Win exposing the the local buffer
// suggest to inline this
void init_single_value_comm_info(void *comm_info, size_t buffer_type_size);

// suggest to inline this
void free_single_value_comm_info(void *comm_info, size_t buffer_type_size);

// for shared reading communication pattern
// suggest to inline this
void init_single_value_comm_info_shared_reading(void *comm_info, size_t buffer_type_size);

// suggest to inline this
void mpi_store_single_value_var_shared_reading(void *comm_info, size_t buffer_type_size);
// suggest to inline this
void mpi_store_single_value_var(void *comm_info, size_t buffer_type_size);
// suggest to inline this
void mpi_load_single_value_var(void *comm_info, size_t buffer_type_size);

#endif /* RTLIB_SINGLEVALUE_H_ */
