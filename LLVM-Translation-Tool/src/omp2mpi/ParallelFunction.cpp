#include "ParallelFunction.h"
#include "environment.h"

#include "helper.h"
#include "llvm/IR/Instructions.h"
#include <llvm/Support/raw_ostream.h>

// Debug Macro for easy switching on/off of debug prints
#define DEBUG_PARALLEL_F 0

#if DEBUG_PARALLEL_F == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

// ParallelFunction::ParallelFunction(llvm::Function *function) { func = function; }
ParallelFunction::ParallelFunction(llvm::Module &M, environment *e, llvm::Function *function,
                                   std::vector<SharedVariable *> shared_args)
{
    func = function;
    if (shared_args.size() != func->arg_size())
    {
        errs() << "Error aligning arguments to shared variables";
    }
    int i;
    auto IterA = func->arg_begin();
    auto IterB = shared_args.begin();
    while (IterA < func->arg_end() && IterB < shared_args.end())
    {
        llvm::Value *function_arg = &*IterA;
        SharedVariable *shared_arg = *IterB;
        if (shared_arg != nullptr)
        {
            // TODO maybe keep the information that shared_args[i] of caller refers to
            // TODO we need to find the annotation (if any) at the variable passed to
            // fork_call_inst
            if (function_arg->getType()->isPointerTy())
            {
                shared_vars.push_back(SharedVariable::Create(M, e, shared_arg, function_arg));
            }
            // else the var is firstprivate
        }

        IterA++;
        IterB++;
    }
    add_globals(M, e);
    clean_shared_vars();
    check_for_parallel_for();
}

void ParallelFunction::add_globals(Module &M, environment *e)
{
    for (auto *global : e->global->shared_globals)
    {
        shared_vars.push_back(global);
    }
}

void ParallelFunction::clean_shared_vars()
{
    Debug(for (auto *var
               : shared_vars) {
        errs() << "determine if function " << func->getName() << " uses var "
               << var->value()->getName() << "\n";
    }) auto iterator =
        std::remove_if(shared_vars.begin(), shared_vars.end(),
                       std::bind(StaticIs_var_not_used, std::placeholders::_1, this));
    shared_vars.erase(iterator, shared_vars.end());

    Debug(for (auto *var
               : shared_vars) {
        errs() << "function " << func->getName() << " uses var " << var->value()->getName()
               << "\n";
    })
}

bool ParallelFunction::is_var_not_used(SharedVariable *var)
{
    return StaticIs_var_not_used(var, this);
}

bool ParallelFunction::StaticIs_var_not_used(SharedVariable *var, ParallelFunction *function)
{
    // a variable of type %struct.ident_t* also not used in MPI
    // as it is insertet by openmp
    Module *M = function->func->getParent();
    if (var->getType() == PointerType::get(M->getTypeByName("struct.ident_t"), 0))
    {
        // errs() << "called function " << function->func->getName() << " NOT uses var of type
        // struct.ident_t";
        return true;
    }

    if (isa<Argument>(var->value()))
    {
        // always thread arguments as if the where used
        // even if there is no usage, as the argument should be bcasted in order to make a
        // correct call
        return false;
    }

    for (auto *u : var->value()->users())
    {
        if (llvm::Instruction *inst = dyn_cast<llvm::Instruction>(u))
        {
            if (inst->getParent()->getParent() == function->func && !isa<ReturnInst>(inst))
            {
                // return operation does NOT count as usage of shared Var, as this instruction
                // hase NO effect on the var (it is neither a load nor a store) Debug(errs() <<
                // "called function " << function->func->getName() << " uses var " <<
                // var->getName() << "\n";)
                return false; // usage found
            }
        }
    }
    // errs() << "called function " << function->func->getName() << " NOT uses var " <<
    // var->getName()<< "\n";
    return true;
}

void ParallelFunction::check_for_parallel_for()
{
    for (auto &block : *func)
    {
        for (auto &instr : block)
        {
            if (auto *call_instr = llvm::dyn_cast<llvm::CallInst>(&instr))
            {
                if (call_instr->getCalledFunction()->getName().startswith(
                        "__kmpc_for_static_init_"))
                {
                    kmpc_for_inits.push_back(call_instr);
                }
            }
        }
    }
}

llvm::Function *ParallelFunction::get_function() { return func; }

void ParallelFunction::dump() { func->dump(); }

bool ParallelFunction::has_shared_vars() { return !shared_vars.empty(); }

bool ParallelFunction::uses_parallel_for() { return !kmpc_for_inits.empty(); }

std::vector<SharedVariable *> ParallelFunction::get_shared_vars() { return shared_vars; }

std::vector<llvm::CallInst *> ParallelFunction::get_kmpc_for_inits() { return kmpc_for_inits; }

// DECREPARTED
bool ParallelFunction::isEqual(ParallelFunction *other)
{
    // true if the other instance of Function is used in the same way
    return ((this->func == other->func) && (this->shared_vars == other->shared_vars));
}

bool ParallelFunction::other_usage(ParallelFunction *other)
{
    // check it the two functions are used in a different way
    return ((this->func == other->func) && !isEqual(other));
}

// also works for structs
bool ParallelFunction::is_shared_array_only_written_in_critical(llvm::Value *var)
{
    bool result = true;
    for (auto *u : var->users())
    {
        if (auto *store = dyn_cast<llvm::StoreInst>(u))
        {
            // if it does not meet the criteria for shared reading pattern abort
            if (!(is_in_omp_critical(store)))
            { //|| store->isAtomic() no atomic operations in mpi
                return false;
            }
        }
        else if (auto *load = dyn_cast<llvm::LoadInst>(u))
        {
            if (load->getType()->isPointerTy())
            { // decent into array (or struct)
                result = result && is_shared_array_only_written_in_critical(load);
            }
        }
        else if (auto *gep = dyn_cast<llvm::GetElementPtrInst>(u))
        {
            result = result && is_shared_array_only_written_in_critical(gep);
        }
    }
    return result;
};
