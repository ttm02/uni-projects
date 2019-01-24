#ifndef OMP2MPI_ENVIRONMENT_H_
#include "environment.h"
// this will include ParallelEntryFunction as well
// this is for convenience so that it is secured, that environment will be included first and
// include ordering will be correct
#endif

#ifndef MICROTASK_H
#define MICROTASK_H

#include "ParallelFunction.h"
#include "environment.h"
#include "llvm/IR/Function.h"

#include <set>
#include <vector>

// TODO describe abstract class
class ParallelEntryFunction : public ParallelFunction
{
  protected:
    // holds information for all called functions
    // also to functions called by functions called (2nd level) and so on
    std::vector<ParallelFunction *> Functions_called;

    // helper function that will initialize the functions_called vector
    void explore_called_functions(llvm::Module &M, environment *e);

  public:
    std::vector<ParallelFunction *> get_all_functions_called();
};

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
 */

class ForkEntry : public ParallelEntryFunction
{
  protected:
    // the fork call where this microtask is invoked
    llvm::CallInst *fork_call_inst = nullptr;

    bool uses_tasking = false;
    // Internal function, which scans the microtasks arguments and figures out
    // their types
    void find_shared_vars(llvm::Module &M, environment *e);

  public:
    // The constructor expects a pointer to a __kmpc_fork_call
    ForkEntry(llvm::Module &M, environment *e, llvm::CallInst *call_inst);

    llvm::CallInst *get_fork_call_inst(llvm::Module &M);
    // returns the shared Variable object passed as argument i
    // or null if arg i is firstprivate
    SharedVariable *get_sharedVar_for_arg(unsigned int i);

    void set_tasking_is_used();
    bool use_tasking();
};
#endif
