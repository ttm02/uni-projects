#include "ParallelTaskEntry.h"
#include "environment.h"

#include "llvm/IR/Instructions.h"
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/raw_ostream.h>

#include "helper.h"
// Debug Macro for easy switching on/off of debug prints
#define DEBUG_TASK_ENTRY 0

#if DEBUG_TASK_ENTRY == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

TaskEntry::TaskEntry(llvm::Module &M, environment *e, std::vector<llvm::Value *> shared_vars,
                     llvm::Function *task_callback_function)
{
    this->func = task_callback_function;
    for (auto *v : shared_vars)
    {
        this->shared_vars.push_back(SharedVariable::Create(M, e, v));
    }
    add_globals(M, e);
    clean_shared_vars();

    check_for_parallel_for();
    assert(kmpc_for_inits.empty() && "Parallel For in task is forbidden\n");
    explore_called_functions(M, e);
}
