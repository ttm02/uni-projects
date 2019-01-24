#ifndef OMP2MPI_REPLACE_PRAGMAS_H_
#define OMP2MPI_REPLACE_PRAGMAS_H_

#include "environment.h"
#include "external_functions.h"

// Functions to Replace openmp functions
// or an openmp Pragma functionality
void replace_pragmas(llvm::Module &M, environment *e);

#endif /* OMP2MPI_REPLACE_PRAGMAS_H_ */
