#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "../environment.h"
#include "../helper.h"
#include "commPatterns.h"

#include <map>

#define DEBUG_TASK_COMM_PATTERN 0

#if DEBUG_TASK_COMM_PATTERN == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

void init_shared_vars_for_tasking(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker,
    std::vector<std::pair<Value *, Value *>> shared_arg_master_worker_pairs)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    int num_of_vars = shared_arg_master_worker_pairs.size();

    Value *num = master_builder.getInt32(num_of_vars);

    master_builder.CreateCall(e->functions->init_all_shared_vars_for_tasking, {num});
    worker_builder.CreateCall(e->functions->init_all_shared_vars_for_tasking, {num});

    for (int i = 0; i < num_of_vars; ++i)
    {
        auto p = shared_arg_master_worker_pairs[i];
        Value *master_var = p.first;
        Value *worker_var = p.second;
        Value *current_num = master_builder.getInt32(i);

        Value *master_void = master_builder.CreateBitCast(master_var, void_ptr_type);
        Value *worker_void = worker_builder.CreateBitCast(worker_var, void_ptr_type);

        Value *sync_point_callback_func =
            ConstantPointerNull::get(e->types->sync_point_callback_func_type->getPointerTo());

        if (master_var->getType()->getPointerElementType()->isPointerTy())
        {
            // array
            assert(worker_var->getType()->getPointerElementType()->isPointerTy());
            sync_point_callback_func =
                e->functions->invlaidate_shared_array_cache_simple_signature;
        }

        master_builder.CreateCall(e->functions->setup_this_shared_var_for_tasking,
                                  {master_void, sync_point_callback_func, current_num});
        worker_builder.CreateCall(e->functions->setup_this_shared_var_for_tasking,
                                  {worker_void, sync_point_callback_func, current_num});
    }

    // insert wrapup after call of microtask
    master_builder.SetInsertPoint(insert_before_master->getNextNode());
    worker_builder.SetInsertPoint(insert_before_worker->getNextNode());

    master_builder.CreateCall(e->functions->wrap_up_all_shared_vars_for_tasking);
    worker_builder.CreateCall(e->functions->wrap_up_all_shared_vars_for_tasking);
}

// Use this function whan microtask has tasks!
// same as distribute_shared_vars_from_master but setup all shared vars for tasking as well
std::vector<llvm::Value *>
task_init_shared_vars_from_master(llvm::Module &M, environment *e, ForkEntry *mikrotask,
                                  llvm::CallInst *insert_before_master,
                                  llvm::CallInst *insert_before_worker,
                                  std::vector<llvm::Value *> *args_to_bcast)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    // contains all shared args (NOT the private ones)
    std::vector<std::pair<Value *, Value *>> shared_arg_master_worker_pairs;

    std::vector<llvm::Value *> args_for_workers;
    // bcast all arguments given
    for (unsigned int i = 0; i < args_to_bcast->size(); ++i)
    {
        Value *var = (*args_to_bcast)[i];

        SharedVariable *microtask_parent_var = mikrotask->get_sharedVar_for_arg(i);
        if (microtask_parent_var != nullptr)
        {
            microtask_parent_var = microtask_parent_var->getOrigin();
        }

        if (isa<GlobalVariable>(var))
        { // all globals are bcasted below
            if (microtask_parent_var !=
                nullptr) // if for whatever reason something like a string literal is passed
            {
                assert(microtask_parent_var->value() == var &&
                       "Error in shared Variable matching");
            }
            args_for_workers.push_back(var);
        }
        else if (isa<Constant>(var) && !isa<GlobalVariable>(var))
        { // global is derived from constant
            // no need to bcast a constant
            args_for_workers.push_back(var);
        }
        else
        {
            std::pair<Value *, Value *> new_master_worker_pair;

            if (var->getType()->isPointerTy())
            { // if not ptr: it is a private var

                if (var->getType()->getPointerElementType()->isStructTy())
                {
                    // structs currently have special handeling
                    new_master_worker_pair = bcast_shared_struct_var_from_master(
                        M, e, insert_before_master, insert_before_worker, var);
                }
                else
                {
                    assert(microtask_parent_var->value() == var &&
                           "Error in shared Variable matching");
                }

                if (auto *array = dyn_cast<SharedArray>(microtask_parent_var))
                {
                    new_master_worker_pair = distribute_shared_dynamic_array_var_from_master(
                        M, e, insert_before_master, insert_before_worker, array);
                }

                else if (auto *single = dyn_cast<SharedSingleValue>(microtask_parent_var))
                {
                    {
                        new_master_worker_pair = bcast_shared_single_value_var_from_master(
                            M, e, insert_before_master, insert_before_worker, single);
                    }
                }
                shared_arg_master_worker_pairs.push_back(new_master_worker_pair);
            }
            else // no pointer type
            {
                new_master_worker_pair = bcast_firstprivate_single_value_var_from_master(
                    M, e, insert_before_master, insert_before_worker, var);
            }

            assert(new_master_worker_pair.first != nullptr);
            assert(new_master_worker_pair.second != nullptr);
            (*args_to_bcast)[i] = new_master_worker_pair.first;
            args_for_workers.push_back(new_master_worker_pair.second);
        }
    }

    // currently using default bcast pattern:
    bcast_all_globals(M, e, insert_before_master, insert_before_worker);

    init_shared_vars_for_tasking(M, e, insert_before_master, insert_before_worker,
                                 shared_arg_master_worker_pairs);

    return args_for_workers;
}
