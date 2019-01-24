#ifndef OMP2MPI_ENVIRONMENT_H_
#include "environment.h"
// this will include ParallelFunction as well
// this is for convenience so that it is secured, that environment will be included first and
// include ordering will be correct
#endif

#ifndef OMP2MPI_PARALLELFUNCTION_H_
#define OMP2MPI_PARALLELFUNCTION_H_

#include "SharedVariable.h"
#include "environment.h"
#include "llvm/IR/Function.h"
#include <llvm/IR/Module.h>

#include <vector>

/*
 * TODO describe class here
 */
class ParallelFunction
{
  protected:
    // A pointer to the function that this instance of the
    // ParallelFunction class is representing.
    llvm::Function *func = nullptr;

    // Containers for the different types of possible shared variables that are used in this
    // ParallelFunction

    std::vector<SharedVariable *> shared_vars;

    std::vector<llvm::CallInst *> kmpc_for_inits;

    // constructor that does nothing to use as super-constructor in subclasses
    ParallelFunction() {}

    // helper function that deletes all variables not used from the vectors
    void clean_shared_vars();
    static bool StaticIs_var_not_used(SharedVariable *var, ParallelFunction *function);

    // add all global variables to shared var list
    // globals are always shared if they are private openmp should copy them to new var
    void add_globals(llvm::Module &M, environment *e);

    void check_for_parallel_for();

    // Decreparted
    bool is_shared_array_only_written_in_critical(llvm::Value *var);

  public:
    // The constructor expects a pointer to a __kmpc_fork_call
    // ParallelFunction(llvm::Function *function);
    // constructor that is used by ParallelEntry function
    // it requires a vector of all args that are shared in caller non shared args should be
    // nullptr
    ParallelFunction(llvm::Module &M, environment *e, llvm::Function *function,
                     std::vector<SharedVariable *> shared_args);

    // helper function that determines if variable is used in THIS function (not including
    // called functions)
    bool is_var_not_used(SharedVariable *var); // currently not used

    // Returns the underlying pointer to the actual function.
    llvm::Function *get_function();

    // A simple wrapper for the dump method that can be called on all LLVM
    // classes.
    void dump();

    void refresh_global_var_analysis(
        std::map<llvm::GlobalVariable *, llvm::Constant *> old_new_map);

    // Check and getter methods for the shared variables container.
    bool has_shared_vars();

    bool uses_parallel_for();
    bool is_shared_Reading_pattern();

    std::vector<SharedVariable *> get_shared_vars();

    std::vector<llvm::CallInst *> get_kmpc_for_inits();

    // Decreparted?
    // currently unused anyway?
    // check if the two objects represent the same call
    bool isEqual(ParallelFunction *other);
    // check it the two functions are used in a different way but points to the same function
    bool other_usage(ParallelFunction *other);
};
#endif
