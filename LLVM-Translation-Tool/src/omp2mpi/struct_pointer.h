#ifndef OMP2MPI_STRUCT_POINTER_H_
#define OMP2MPI_STRUCT_POINTER_H_

#include "environment.h"
#include "external_functions.h"

// handle the usage of struct_ptr in the entire module
void handle_struct_pointer(llvm::Module &M, environment *e);

// Print out a wanring when a declared type might be a linked list
void warn_if_struct_might_be_linked_list(llvm::Module &M, environment *e);

// initializes the map from old struct types to new one
void build_old_to_new_struct_type_map(llvm::Module &M, environment *e);

// used for reduction of struct types
void build_reduce_wrapper_for_struct_type_reduction(llvm::Module &M, environment *e,
                                                    llvm::Function *omp_combiner,
                                                    llvm::BasicBlock *insertF);

#endif /* OMP2MPI_STRUCT_POINTER_H_ */
