#include "ParallelEntryFunction.h"
#include "environment.h"

#include "llvm/IR/Instructions.h"
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/raw_ostream.h>

#include "helper.h"
// Debug Macro for easy switching on/off of debug prints
#define DEBUG_PARALLEL_ENTRY_F 0

#if DEBUG_PARALLEL_ENTRY_F == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

std::vector<ParallelFunction *> ParallelEntryFunction::get_all_functions_called()
{
    return Functions_called;
}

void ParallelEntryFunction::explore_called_functions(Module &M, environment *e)
{
    // holds the function and the vector of shared values passed to this call as args
    // non shared values should be nullptr in this vector
    typedef std::pair<llvm::Function *, std::vector<SharedVariable *>> call_info;
    std::set<call_info> visited;
    std::set<llvm::Function *> to_visit;
    to_visit.insert(func);
    while (!to_visit.empty())
    {
        llvm::Function *currentF = to_visit.extract(to_visit.begin()).value();
        Debug(errs() << "currently visiting " << currentF->getName() << "\n";)

            for (inst_iterator I = inst_begin(currentF), E = inst_end(currentF); I != E; ++I)
        {
            Instruction *inst = &*I;
            Debug(errs() << "inst:";
                  inst->dump();) if (llvm::CallInst *call = dyn_cast<llvm::CallInst>(inst))
            {
                // TODO ? virtual function calls
                // if function is NULL it is a virtual function and we cannot handle it, as
                // we do not know wich actual function will be invoked
                if (!(call->getCalledFunction() == nullptr))
                {
                    llvm::Function *f = call->getCalledFunction();
                    // if it is defined in this module
                    if (!f->isDeclaration() && !f->getName().startswith(".omp_combiner."))
                    // no need to analyze a reduction function, this
                    // will be done when reduction is analyzed
                    {
                        std::vector<SharedVariable *> call_args;
                        for (llvm::Value *arg : call->arg_operands())
                        {
                            SharedVariable *shared_var = nullptr;
                            for (auto *shared : shared_vars)
                            {
                                if (shared->value() == arg)
                                {
                                    shared_var = shared;
                                    break;
                                }
                            }
                            call_args.push_back(shared_var);
                        }
                        call_info info = {f, call_args};
                        // only if this was not visited yet
                        // (e.g. to avoid endless parsing of recursive calls)
                        if (std::get<bool>(visited.insert(info)))
                        {
                            to_visit.insert(f);
                        }
                    }
                }
            }
            // same code for invoke
            else if (llvm::InvokeInst *invoke = dyn_cast<llvm::InvokeInst>(inst))
            {

                if (!(invoke->getCalledFunction() == nullptr))
                {
                    llvm::Function *f = invoke->getCalledFunction();
                    // if it is defined in this module
                    if (!f->isDeclaration() && !f->getName().startswith(".omp_combiner."))
                    // no need to analyze a reduction function, this
                    // will be done when reduction is analyzed
                    {
                        std::vector<SharedVariable *> call_args;
                        for (llvm::Value *arg : invoke->arg_operands())
                        {
                            SharedVariable *shared_var = nullptr;
                            for (auto *shared : shared_vars)
                            {
                                if (shared->value() == arg)
                                {
                                    shared_var = shared;
                                    break;
                                }
                            }
                            call_args.push_back(shared_var);
                        }
                        call_info info = {f, call_args};
                        // only if this was not visited yet
                        // (e.g. to avoid endless parsing of recursive calls)
                        if (std::get<bool>(visited.insert(info)))
                        {
                            to_visit.insert(f);
                        }
                    }
                }
            }
        }
    }

    for (call_info info : visited)
    {
        Functions_called.push_back(new ParallelFunction(M, e, info.first, info.second));
    }
}

ForkEntry::ForkEntry(llvm::Module &M, environment *e, llvm::CallInst *call_inst)
{
    fork_call_inst = call_inst;

    auto *microtask_arg = call_inst->getArgOperand(2);
    func = dyn_cast<Function>(microtask_arg->stripPointerCastsNoFollowAliases());

    find_shared_vars(M, e);
    add_globals(M, e);
    clean_shared_vars();

    check_for_parallel_for();
    explore_called_functions(M, e);
}

void ForkEntry::find_shared_vars(llvm::Module &M, environment *e)
{
    // minimum: openmp specific ones
    assert(func->getFunctionType()->getNumParams() >= 2);
    int i = 0;
    auto arg_iter = func->arg_begin();
    // first arg
    assert(arg_iter->getName().equals(".global_tid."));
    ++i;
    ++arg_iter;
    // second arg
    assert(arg_iter->getName().equals(".bound_tid."));
    ++i;
    ++arg_iter;
    ++i; // this arg it the ompoutlined function in fork call

    // collect all function arguments that represent an OpenMP shared variable
    for (; arg_iter != func->arg_end(); ++arg_iter, ++i)
    {
        // if arg is not ptr type it was used within the firstprivate clause and is not
        // shared (firstprivate on ptr does not make sense anyway)
        if (arg_iter->getType()->isPointerTy())
        {
            SharedVariable *top_lvl =
                get_top_level_var(M, e, fork_call_inst->getArgOperand(i));
            if (top_lvl != nullptr)
            {
                shared_vars.push_back(SharedVariable::Create(M, e, top_lvl, arg_iter));
            }
            else
            {
                // if not found we will assume default pattern
                shared_vars.push_back(SharedVariable::Create(M, e, arg_iter));
            }
        }
    }
}

llvm::CallInst *ForkEntry::get_fork_call_inst(Module &M)
{
    CallInst *result = fork_call_inst;

    auto kmpc_fork_calls = get_function_users(M, StringRef("__kmpc_fork_call"));

    for (auto &fork_call_user : kmpc_fork_calls)
    {
        if (auto *call_inst = dyn_cast<CallInst>(fork_call_user))
        {
            auto *microtask_arg = call_inst->getArgOperand(2);
            Function *called_microtask =
                dyn_cast<Function>(microtask_arg->stripPointerCastsNoFollowAliases());
            if (called_microtask == func)
            {
                if (call_inst != fork_call_inst)
                {
                    Debug(errs() << "Info: The fork_call_inst was altered\n";)
                }
                return call_inst;
            }
        }
    }

    errs() << "Error: fork_call_inst not found!";
    return fork_call_inst;
}

SharedVariable *ForkEntry::get_sharedVar_for_arg(unsigned int i)
{
    assert(i < this->func->arg_size());

    if (i < 2)
    {
        return nullptr;
        // arg 0 and 1 are OpemMP specific
    }

    auto *iter = this->func->arg_begin();

    for (unsigned int j = 0; j < i; ++j)
    {
        ++iter;
    }

    Argument *arg = iter;

    if (arg->getType()->isPointerTy())
    {
        for (auto *var : this->shared_vars)
        {
            if (var->value() == arg)
            {
                return var;
            }
        }
        errs() << "Error in Finding shared Variable for argument\n";
        arg->dump();
        return nullptr;
    }
    else
    { // var is private
        return nullptr;
    }
}

void ForkEntry::set_tasking_is_used() { uses_tasking = true; }
bool ForkEntry::use_tasking() { return uses_tasking; }
