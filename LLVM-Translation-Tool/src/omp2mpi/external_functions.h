// declaration of functions in external_module or mpi_mutex to use in the pass
#ifndef OMP2MPI_EXTERNAL_FUNCTIONS_H_
#define OMP2MPI_EXTERNAL_FUNCTIONS_H_

#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include "environment.h"

// initialize all externals
void load_external_definitions(llvm::Module &M, environment *e);
void load_external_implementations(llvm::Module &M, environment *e);
// inlines all calls of given function within the module
void inline_this_function(llvm::Module &M, llvm::Function *func);
void inline_external_functions(llvm::Module &M, environment *e);
void erase_not_used_external_functions(llvm::Module &M, environment *e);

// only for debug reasons: let later passes not inline rtlinb
void never_inline_external_functions(llvm::Module &M, environment *e);

bool is_func_defined_by_pass(environment *e, llvm::Function *f);
bool is_func_defined_by_openmp(environment *e, llvm::Function *f);

// utility macro:
// insert case to match external function to specified pointer
// function that is matched is named func
// macro start with else if
#define MATCH_FUNCTION(ptr, name)                                                             \
    else if (func.getName().startswith(name))                                                 \
    {                                                                                         \
        ptr = M.getOrInsertFunction(func.getName(), func.getFunctionType());                  \
        llvm::Function *newF = dyn_cast<llvm::Function>(ptr);                                 \
        e->external_module->exportedF.push_back(newF);                                        \
        e->external_module->val_map[&func] = newF;                                            \
    }

#endif /* OMP2MPI_EXTERNAL_FUNCTIONS_H_ */
