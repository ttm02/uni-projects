#ifndef OMP2MPI_TASK_PRAGMA_H_
#define OMP2MPI_TASK_PRAGMA_H_

#include "environment.h"
#include "external_functions.h"

// handles task_pragma
// only call it after replace_pragmas
void handle_task_pragma(llvm::Module &M, environment *e);

#endif /* OMP2MPI_TASK_PRAGMA_H_ */
