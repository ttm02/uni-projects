#ifndef RTLIB_STRUCT_PTR_H_
#define RTLIB_STRUCT_PTR_H_

#include "rtlib_typedefs.h"

// fetch the shared struct so that the process can have a local pointer to it
// or use it to update teh local copy
// suggest to inline this
void mpi_load_shared_struct(void *buffer, struct mpi_struct_ptr *struct_ptr, MPI_Win *win);

// store the pointer to a shared struct: update the pointer information
// suggest to inline this
void mpi_store_shared_struct_ptr(void *new_ptr, size_t size,
                                 struct mpi_struct_ptr *struct_ptr);

// store the pointer to a shared struct: update the pointer information
// suggest to inline this
void mpi_copy_shared_struct_ptr(struct mpi_struct_ptr *copy_from,
                                struct mpi_struct_ptr *copy_to);

// determines if two struct ptr are equal
// suggest to inline this
bool mpi_cmp_EQ_shared_struct_ptr(struct mpi_struct_ptr *a, struct mpi_struct_ptr *b);
// determines if two struct ptr are not qual
// suggest to inline this
bool mpi_cmp_NE_shared_struct_ptr(struct mpi_struct_ptr *a, struct mpi_struct_ptr *b);

// load the pointer to a shared struct: returns the given local buffer or nullptr
// buffer has to be allocated by caller
// suggest to inline this
void *mpi_load_shared_struct_ptr(void *buffer, struct mpi_struct_ptr *struct_ptr);

// update the remote copy of the shared struct
// this will flush the whole local copy therefore it is recomended to first fetch the remote
// content again, then do the actual stroe and finally update remote copy
// suggest to inline this
void mpi_store_to_shared_struct(void *buffer, struct mpi_struct_ptr *struct_ptr, MPI_Win *win);

#endif /* RTLIB_STRUCT_PTR_H_ */
