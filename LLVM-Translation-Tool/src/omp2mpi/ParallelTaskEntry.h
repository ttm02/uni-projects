#ifndef OMP2MPI_ENVIRONMENT_H_
#include "environment.h"
// this will include ParallelTask_entry as well
// this is for convenience so that it is secured, that environment will be included first and
// include ordering will be correct
#endif

#ifndef OMP2MPI_PARALLELTASK_ENTRY_H_
#define OMP2MPI_PARALLELTASK_ENTRY_H_

#include "ParallelEntryFunction.h"
#include "ParallelFunction.h"
#include "environment.h"
#include "llvm/IR/Function.h"

#include <set>
#include <vector>

/*
 * This class holds information about a microtask function that has been created
 * by clang during the translation of an OpenMP parallel pragma.
 *
 * It analyzes the functions arguments and figures out the types of shared
 * variables that are used in the parallel region.
 *
 * The class also categorizes the microtask function according to the specific
 * OpenMP features that are used (e.g. parallel-for, uses a reduce, has omp
 * critical sections...)
 *
 * It is the same as ParallelEntryFunction but used for task
 * (different constructor)
 */
class TaskEntry : public ParallelEntryFunction
{
  public:
    // The constructor expects a pointer to a __kmpc_fork_call
    TaskEntry(llvm::Module &M, environment *e, std::vector<llvm::Value *> shared_vars,
              llvm::Function *task_callback_function);
};
#endif
