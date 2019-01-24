// Functions to Replace openmp functions
// or an openmp Pragma functionality

#include "task_pragma.h"
#include "comm_patterns/commPatterns.h"
#include "environment.h"
#include "external_functions.h"
#include "helper.h"

#include "llvm/IR/InstIterator.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#define DEBUG_TASK_PRAGMA 0

#if DEBUG_TASK_PRAGMA == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

// initialize the task_pragma handeling on a global level
// and add the corresponding wrap up part
void init_task_pragma_handeling(Module &M, environment *e)
{
    IRBuilder<> builder(M.getContext());
    builder.SetInsertPoint(e->global->setup->getTerminator());

    Type *int32ty = Type::getInt32Ty(M.getContext());
    Constant *constant0 = ConstantInt::get(int32ty, 0);
    e->global->task_param_win =
        new GlobalVariable(M, int32ty, false, GlobalValue::LinkageTypes::CommonLinkage,
                           constant0, "task_parameter_win");
    // basis rank =0
    builder.CreateCall(e->functions->mpi_create_dynamic_window, {e->global->task_param_win});
    Value *load_task_param_win = builder.CreateLoad(e->global->task_param_win);
    builder.CreateCall(e->functions->mpi_global_tasking_info_init,
                       {load_task_param_win, constant0});
    builder.SetInsertPoint(e->global->wrapUp->getFirstNonPHI());
    // will also free the task_param_win:
    builder.CreateCall(e->functions->mpi_global_tasking_info_destroy);

    // initialize execute_task function:
    BasicBlock *entryB =
        BasicBlock::Create(e->execute_task->getContext(), "entry", e->execute_task);
    BasicBlock *wrap_up_B =
        BasicBlock::Create(e->execute_task->getContext(), "unknown_task", e->execute_task);
    builder.SetInsertPoint(entryB);
    // create main switch
    Value *arg1 = e->execute_task->arg_begin();
    Value *ptr_to_task_ID =
        builder.CreateGEP(arg1, {builder.getInt32(0), builder.getInt32(0)});
    Value *task_id = builder.CreateLoad(ptr_to_task_ID, "task_ID");
    // prealloc 5 switch inst cases (arbitrary choice)
    // but nothing will break if there where more
    e->execute_task_switch = dyn_cast<SwitchInst>(builder.CreateSwitch(task_id, wrap_up_B, 5));

    builder.SetInsertPoint(wrap_up_B);
    builder.CreateRetVoid();
}

// replaces all calls to taskwait with sync_task function
void replace_taskwait_pragma(Module &M, environment *e)
{
    Value *bool_true = ConstantInt::get(Type::getInt1Ty(M.getContext()), 1);
    auto taskwait_users = get_function_users(M, StringRef("__kmpc_omp_taskwait"));
    for (auto *u : taskwait_users)
    {
        if (auto *invoke = dyn_cast<InvokeInst>(u))
        {
            errs() << "Taskwait is not supposed to be used in invoke\n";
        }
        else if (auto *call = dyn_cast<CallInst>(u))
        {
            // insert new call
            IRBuilder<> builder(M.getContext());
            builder.SetInsertPoint(call);
            builder.CreateCall(e->functions->mpi_sync_task, {bool_true});
            // and delete old
            call->eraseFromParent();
            // necessary because omp taskwait return int and our function void
        }
    }
}

// replaces all calls to barrier with sync_task in specified parallel region
// basically: sync_task will first do taskwait before barrier
void replace_barrier_with_taskwait(Module &M, environment *e,
                                   ParallelEntryFunction *parallel_region)
{
    Value *bool_false = ConstantInt::get(Type::getInt1Ty(M.getContext()), 0);
    auto taskwait_users = get_function_users(M, e->functions->mpi_barrier->getName());
    for (auto *u : taskwait_users)
    {
        if (auto *inst = dyn_cast<Instruction>(u))
            if (get_ForkEntry(M, e, inst) == parallel_region)
            {
                // barrier is not used in invoke with the pass
                if (auto *call = dyn_cast<CallInst>(u))
                {
                    ReplaceInstWithInst(
                        call, CallInst::Create(e->functions->mpi_sync_task, {bool_false}));
                }
            }
    }
}

// insert add param that is added with store inst
void insert_add_param(Module &M, environment *e, IRBuilder<> *builder, Value *task_info,
                      StoreInst *store)
{
    Value *param = store->getValueOperand();
    Debug(errs() << "Add task arg "; param->dump();)
        assert(param->getType()->isPointerTy() && "assertion: is_shared");

    Value *cast_to_void = builder->CreateBitCast(param, builder->getInt8PtrTy());
    std::vector<Value *> add_param_args = {task_info, cast_to_void};
    builder->CreateCall(e->functions->add_shared_param, add_param_args);
}

// returns the shared Var that should be used in the task
Value *insert_load_of_shared_param(llvm::Module &M, environment *e, llvm::Value *task_info,
                                   llvm::LoadInst *load)
{
    // this insertion point is in init block of function:
    IRBuilder<> builder(get_init_block(M, e, dyn_cast<Instruction>(task_info)->getFunction())
                            ->getTerminator());

    // note: insertion point is not where the load is but at beginning of function (=init
    // Block) so that order of addParam and Fetch Param calls will match
    Value *var_void = builder.CreateCall(e->functions->fetch_next_shared_param, {task_info});
    // cast it to correct type
    Value *var = builder.CreateBitCast(var_void, load->getType());

    load->replaceAllUsesWith(var);

    return var;
}

bool is_param_shared(std::pair<unsigned long, StoreInst *> pair)
{
    return pair.second->getValueOperand()->getType()->isPointerTy();
}

// only handles the store of PRIVATE params
void handle_store_of_private_params(
    Module &M, environment *e, IRBuilder<> *builder, Value *task_info,
    std::vector<std::pair<unsigned long, StoreInst *>> idx_store_list)
{
    for (auto pair : idx_store_list)
    {
        if (!is_param_shared(pair))
        {
            StoreInst *store = pair.second;
            Value *param = store->getValueOperand();
            Value *param_ptr = nullptr;
            if (auto *load = dyn_cast<LoadInst>(param))
            {
                param_ptr = load->getPointerOperand();
            }
            else
            {
                // use to reset insertion point
                // TODO is there any other way of getting the current insertion pt of builder?
                Instruction *dummy =
                    dyn_cast<Instruction>(builder->CreateLoad(e->global->rank));

                builder->SetInsertPoint(
                    get_init_block(M, e, dummy->getFunction())->getTerminator());
                param_ptr = builder->CreateAlloca(
                    param->getType()); // alloc it at the init of function
                // therefore it will only alloc one buffer even if tasks are created within a
                // loop as private param are copied on add_private_param call this is
                // sufficient

                builder->SetInsertPoint(dummy->getNextNode()); // reset insert point
                // after dummy! else we may not erase dummy instruction
                dummy->eraseFromParent();
                builder->CreateStore(param, param_ptr);
            }
            assert(param_ptr != nullptr);
            Value *size = builder->getInt64(get_size_in_Byte(M, param_ptr));
            Value *cast_to_void = builder->CreateBitCast(param_ptr, builder->getInt8PtrTy());
            std::vector<Value *> add_param_args = {task_info, cast_to_void, size};
            builder->CreateCall(e->functions->add_private_param, add_param_args);
        }
    }
}

void handle_load_of_private_params(Module &M, environment *e, Value *private_param_task_arg,
                                   Function *task_callback_function,
                                   Value *task_info_run_in_ompoutlined,
                                   std::vector<Instruction *> *to_remove)
{
    IRBuilder<> builder_task_run(
        get_init_block(M, e, task_callback_function)->getTerminator());

    for (auto *u : private_param_task_arg->users())
    {
        assert(isa<CallInst>(u));
        CallInst *call = dyn_cast<CallInst>(u);
        // so call will be erased before openmp inserted alloca of ptr to var ptr
        to_remove->push_back(call);
        // the called openmp function copies the private params
        // skip first arg, as it is the ptr to the private param struct
        for (unsigned int i = 1; i < call->getNumArgOperands(); ++i)
        {
            // find load of value as a ptr to var ptr is used in copy_fn
            Value *var_ptr = call->getArgOperand(i);

            for (auto *u2 : var_ptr->users())
            {
                if (u2 != call) // of cause a call is a user itself
                {
                    assert(isa<LoadInst>(u2) && "Only load on private Param ptr is allowed");
                    auto *load = dyn_cast<LoadInst>(u2);
                    assert((!load->getType()->getPointerElementType()->isPointerTy()) &&
                           "ptr variables cannot be private in openmp (as ptr dest is always "
                           "shared)");
                    Value *var = builder_task_run.CreateAlloca(
                        load->getType()->getPointerElementType());
                    Value *to_void =
                        builder_task_run.CreateBitCast(var, builder_task_run.getInt8PtrTy());
                    std::vector<Value *> fetch_param_args = {to_void,
                                                             task_info_run_in_ompoutlined};
                    builder_task_run.CreateCall(e->functions->fetch_next_private_param,
                                                fetch_param_args);
                    load->replaceAllUsesWith(var);
                    to_remove->push_back(load);
                }
            }
            if (auto *ptr_alloc = dyn_cast<AllocaInst>(var_ptr))
            {
                to_remove->push_back(ptr_alloc);
            }
            else
            {
                errs() << "Why is this variable private?";
                call->getArgOperand(i)->dump();
            }
        }
    }
}

std::vector<std::pair<unsigned long, StoreInst *>>
get_task_parameter_init(Module &M, environment *e, CallInst *omp_task_alloc,
                        CallInst *omp_task_call, std::vector<Instruction *> *to_remove)
{
    std::vector<std::pair<unsigned long, StoreInst *>> store_shared_param_idx_pairs;

    Debug(errs() << "Find task args \n";)

        IRBuilder<>
            builder_task_create(omp_task_alloc);

    for (auto *u : omp_task_alloc->users())
    {
        if (auto *cast_to_task_struct = dyn_cast<BitCastInst>(u))
        {
            if (cast_to_task_struct->getDestTy()->getPointerElementType()->isStructTy())
            // cast to kmp_task_t_with_privates
            {
                for (auto *u2 : cast_to_task_struct->users())
                {
                    if (auto *gep_1 = dyn_cast<GetElementPtrInst>(u2))
                    {
                        // is it the right gep?
                        auto idx_iter = gep_1->idx_begin();
                        assert(dyn_cast<Value>(idx_iter) == builder_task_create.getInt32(0));
                        idx_iter++;
                        // this is the position where the args are stored
                        if (dyn_cast<Value>(idx_iter) == builder_task_create.getInt32(0))
                        {
                            for (auto *u3 : gep_1->users())
                            {
                                assert(isa<GetElementPtrInst>(u3));
                                for (auto *u4 : dyn_cast<Instruction>(u3)->users())
                                {
                                    // load of void*
                                    assert(isa<LoadInst>(u4));
                                    assert(dyn_cast<LoadInst>(u4)->getType() ==
                                           builder_task_create.getInt8PtrTy());
                                    for (auto *u5 : dyn_cast<Instruction>(u4)->users())
                                    {
                                        if (auto *cast = dyn_cast<BitCastInst>(u5))
                                        {
                                            if (cast->getDestTy()
                                                    ->getPointerElementType()
                                                    ->isStructTy() &&
                                                dyn_cast<StructType>(
                                                    cast->getDestTy()->getPointerElementType())
                                                    ->getName()
                                                    .startswith("struct.anon"))
                                            {
                                                // we dont need that
                                                // i think this is used to gain some additional
                                                // information for the task
                                                add_remove_all_uses(to_remove, cast, true);
                                            }
                                            else
                                            {
                                                // cast instruction is first arg (as cast
                                                // is aray index 0)
                                                for (auto *u_param : cast->users())
                                                {
                                                    if (auto *store =
                                                            dyn_cast<StoreInst>(u_param))
                                                    {
                                                        assert(store->getPointerOperand() ==
                                                               cast);
                                                        Debug(
                                                            errs() << "Found Task Param\n";
                                                            store->getValueOperand()->dump();)
                                                            // index is 0
                                                            store_shared_param_idx_pairs
                                                                .push_back(
                                                                    std::make_pair(0, store));
                                                        to_remove->push_back(store);
                                                    }
                                                }
                                            }
                                        }
                                        else if (auto *gep = dyn_cast<GetElementPtrInst>(u5))
                                        {
                                            // if one want to order task arguments not
                                            // based on instruction ordering: one need
                                            // to get the index of this gep to order
                                            // task args based on storage location
                                            for (auto *u_gep : gep->users())
                                            {
                                                // cast void* to arg_type**
                                                if (auto *cast = dyn_cast<BitCastInst>(u_gep))
                                                {
                                                    for (auto *u_param : cast->users())
                                                    {
                                                        if (auto *store =
                                                                dyn_cast<StoreInst>(u_param))
                                                        {
                                                            Value *idx = dyn_cast<Value>(
                                                                gep->idx_begin());
                                                            // must be constant
                                                            assert(isa<ConstantInt>(idx));
                                                            unsigned long idx_as_int =
                                                                dyn_cast<ConstantInt>(idx)
                                                                    ->getZExtValue();
                                                            assert(
                                                                store->getPointerOperand() ==
                                                                cast);
                                                            Debug(errs()
                                                                      << "Found task Param\n";
                                                                  store->getValueOperand()
                                                                      ->dump();)

                                                                store_shared_param_idx_pairs
                                                                    .push_back(std::make_pair(
                                                                        idx_as_int, store));
                                                            to_remove->push_back(store);
                                                        }
                                                    }
                                                }
                                                to_remove->push_back(
                                                    dyn_cast<Instruction>(u_gep));
                                            }
                                        }
                                        to_remove->push_back(dyn_cast<Instruction>(u5));
                                    } // end for u5
                                    to_remove->push_back(dyn_cast<Instruction>(u4));
                                } // end for u4
                                to_remove->push_back(dyn_cast<Instruction>(u3));
                            } // end for u3
                        }
                        else
                        {
                            // we do not care about other information given to the task
                            // remove all usages
                            add_remove_all_uses(to_remove, gep_1, true);
                        }
                    }
                    to_remove->push_back(dyn_cast<Instruction>(u2));
                } // end for u2
            }
            to_remove->push_back(cast_to_task_struct);
        }
        else if (u != omp_task_call)
        {
            errs() << "Error in handleing the Task argument: ";
            u->dump();
        }
    } // end for u : omp_task_alloc->users()

    return store_shared_param_idx_pairs;
}

std::vector<std::pair<unsigned long, LoadInst *>>
get_shared_task_parameter_load(Module &M, environment *e, Value *shared_param_task_arg,
                               std::vector<Instruction *> *to_remove)
{
    std::vector<std::pair<unsigned long, LoadInst *>> load_shared_param_idx_pairs;

    Value *constant0 = ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0);

    // shared
    for (auto *u : shared_param_task_arg->users())
    {
        if (auto *gep = dyn_cast<GetElementPtrInst>(u))
        {
            auto idx_iter = gep->idx_begin();
            // first idx is 0 (derefeerencing of openmp struct ptr)
            assert(dyn_cast<Value>(idx_iter) == constant0);
            idx_iter++;
            // next idx it the idx of shared var
            Value *idx = dyn_cast<Value>(idx_iter);
            // must be constant
            assert(isa<ConstantInt>(idx));
            unsigned long idx_as_int = dyn_cast<ConstantInt>(idx)->getZExtValue();
            for (auto *u2 : gep->users())
            {
                assert(isa<LoadInst>(u2));
                // load of ptr to shared var
                load_shared_param_idx_pairs.push_back(
                    std::make_pair(idx_as_int, dyn_cast<LoadInst>(u2)));
                // we need to push it in to to removal vector so that no undef will be entered
                // the replacement of load will happen, when the args are loaded (below)
                to_remove->push_back(dyn_cast<Instruction>(u2));
            }

            to_remove->push_back(gep);
        }
    }

    return load_shared_param_idx_pairs;
}

void handle_create_task(Module &M, environment *e, CallInst *omp_task_call)
{
    // holds the instructions to be removed:
    std::vector<Instruction *> to_remove;

    CallInst *omp_task_alloc = dyn_cast<CallInst>(omp_task_call->getArgOperand(2));
    assert(omp_task_alloc != nullptr);

    Value *task_entry = omp_task_alloc->getArgOperand(5);

    Function *task_callback_function = nullptr;
    if (isa<Function>(task_entry))
    {
        task_callback_function = dyn_cast<Function>(task_entry);
    }
    else
    {
        // it is a constant function cast
        Instruction *as_inst = dyn_cast<ConstantExpr>(task_entry)->getAsInstruction();
        if (as_inst->isCast())
        {
            Value *casted_v = as_inst->getOperand(0);
            // get the casted Function
            task_callback_function = dyn_cast<Function>(casted_v);
        }
        as_inst->deleteValue();
    }
    assert(task_callback_function != nullptr);

    Debug(errs() << "Creating call to task: " << task_callback_function->getName() << "\n";)

        // builder that inserts task creation:
        IRBuilder<>
            builder_task_create(M.getContext());
    // builder that inserts the calling of task:
    IRBuilder<> builder_task_run(M.getContext());

    // use NumCases() + 1 as task id so that 0 is an unknown default ID = no task
    ConstantInt *task_id = ConstantInt::get(Type::getInt32Ty(M.getContext()),
                                            e->execute_task_switch->getNumCases() + 1);
    // build a new switch case
    BasicBlock *run_task_B = BasicBlock::Create(
        e->execute_task->getContext(), task_callback_function->getName(), e->execute_task);
    e->execute_task_switch->addCase(task_id, run_task_B);

    builder_task_run.SetInsertPoint(run_task_B);
    Value *task_info_run = e->execute_task->arg_begin();

    // in task_callback_function find call of ompoutlined function
    for (inst_iterator I = inst_begin(task_callback_function),
                       E = inst_end(task_callback_function);
         I != E; ++I)
    {
        if (auto *call = dyn_cast<CallInst>(&*I))
        {
            assert(call->getCalledFunction()->getName().startswith(".omp_outlined"));
            // directly use the omp_outlined func
            task_callback_function = call->getCalledFunction();
        }
    }

    std::vector<Value *> args_for_task_call;
    assert(task_callback_function->arg_size() == 6);
    auto iter = task_callback_function->arg_begin();
    args_for_task_call.push_back(task_id); // not used anyway
    iter++;
    args_for_task_call.push_back(
        ConstantPointerNull::get(dyn_cast<PointerType>(iter->getType())));
    // use third arg to pass task info:
    iter++;
    Type *arg_ty = iter->getType();
    Value *casted_task_info = builder_task_run.CreateBitCast(task_info_run, arg_ty);
    Value *task_info_run_arg = *&iter;
    args_for_task_call.push_back(casted_task_info);
    // use constant null for other 3 args
    iter++;
    args_for_task_call.push_back(
        ConstantPointerNull::get(dyn_cast<PointerType>(iter->getType())));
    Value *private_param_task_arg = *&iter; // callee will use the arg to get private params
    iter++;
    args_for_task_call.push_back(
        ConstantPointerNull::get(dyn_cast<PointerType>(iter->getType())));
    iter++;
    args_for_task_call.push_back(
        ConstantPointerNull::get(dyn_cast<PointerType>(iter->getType())));
    Value *shared_param_task_arg = *&iter; // callee will use the arg to get shared params

    builder_task_run.CreateCall(task_callback_function, args_for_task_call);
    // return if task is completed
    builder_task_run.CreateRetVoid();

    // paring of the args is done in the callback function
    builder_task_run.SetInsertPoint(
        get_init_block(M, e, task_callback_function)->getTerminator());
    Value *task_info_run_in_ompoutlined = builder_task_run.CreateBitCast(
        task_info_run_arg, e->types->task_info_struct->getPointerTo());

    // get store of task args:
    // add them to store
    // first is the index of argument
    // second is the store inst
    auto store_param_idx_pairs =
        get_task_parameter_init(M, e, omp_task_alloc, omp_task_call, &to_remove);
    // load at callee side (when task is executed)
    auto load_shared_param_idx_pairs =
        get_shared_task_parameter_load(M, e, shared_param_task_arg, &to_remove);

    // sorting the args
    // as index is first pair elem, it will sort according to idx
    // and we do not need to supply a sort perdicate
    std::sort(store_param_idx_pairs.begin(), store_param_idx_pairs.end());
    std::sort(load_shared_param_idx_pairs.begin(), load_shared_param_idx_pairs.end());

    // only the shared params
    std::vector<std::pair<unsigned long, StoreInst *>> shared_param_idx_pairs;

    std::copy_if(store_param_idx_pairs.begin(), store_param_idx_pairs.end(),
                 std::back_inserter(shared_param_idx_pairs), is_param_shared);

    int num_shared_int = shared_param_idx_pairs.size();
    Value *num_shared = builder_task_create.getInt32(num_shared_int);

    builder_task_create.SetInsertPoint(omp_task_alloc);
    Value *task_info_create = builder_task_create.CreateCall(
        e->functions->create_new_task_info, {task_id, num_shared});

    // private
    handle_store_of_private_params(
        M, e, &builder_task_create, task_info_create,
        store_param_idx_pairs); // only handles the private part of the whole vector
    handle_load_of_private_params(M, e, private_param_task_arg, task_callback_function,
                                  task_info_run_in_ompoutlined, &to_remove);

    std::vector<Value *> shared_vars_in_task;
    // shared
    for (auto pair : load_shared_param_idx_pairs)
    {
        auto *v = insert_load_of_shared_param(M, e, task_info_run_in_ompoutlined, pair.second);
        shared_vars_in_task.push_back(v);
    }

    for (auto pair : shared_param_idx_pairs)
    {
        insert_add_param(M, e, &builder_task_create, task_info_create, pair.second);
    }

    // finally add the task
    builder_task_create.CreateCall(e->functions->mpi_add_task, {task_info_create});

    to_remove.push_back(omp_task_call);
    to_remove.push_back(omp_task_alloc);
    for (auto *inst : to_remove)
    {
        if (!inst->use_empty())
        {
            // than the order of the instructions removal does not matter:
            inst->replaceAllUsesWith(UndefValue::get(inst->getType()));
            errs() << "Nevertheless insertion of undef should not hapen\n";
        }
        inst->eraseFromParent();
    }
    // if task is cleaned: analyte it as a microtask:

    TaskEntry *task_function =
        new TaskEntry(M, e, shared_vars_in_task, task_callback_function);
    e->task_functions.push_back(task_function);

    //  ASK_TO_CONTINIUE
}

void handle_task_pragma(llvm::Module &M, environment *e)
{
    std::vector<User *> tasking_users = get_function_users(M, "__kmpc_omp_task");

    if (!tasking_users.empty())
    {
        init_task_pragma_handeling(M, e);
        // find all parallel regions that use tasking
        std::set<ForkEntry *> parallel_regions_with_tasking;
        for (auto *u : tasking_users)
        {
            if (auto *inst = dyn_cast<Instruction>(u))
            {
                parallel_regions_with_tasking.insert(get_ForkEntry(M, e, inst));
            }
        }

        for (auto *parallel_region : parallel_regions_with_tasking)
        {
            parallel_region->set_tasking_is_used();
            // within the regions with tasking a tasksync need to be performed before
            // each barrier
            replace_barrier_with_taskwait(M, e, parallel_region);
            // add another task-syncronization at the end of function for the following join
            // barrier:
            Value *bool_false = ConstantInt::get(Type::getInt1Ty(M.getContext()), 0);
            IRBuilder<> builder(
                get_wrapUp_block(M, e, parallel_region->get_function())->getFirstNonPHI());
            builder.CreateCall(e->functions->mpi_sync_task, {bool_false});
        }

        // handle creation of tasks:
        // will fill the ParallelTaskEntry list in the environment
        auto task_users = get_function_users(M, "__kmpc_omp_task");
        for (auto *u : task_users)
        {
            if (auto *call = dyn_cast<CallInst>(u))
            {
                handle_create_task(M, e, call);
            }
            else
            {
                errs() << "ERROR: not allowed use of task pargma:\n";
                u->dump();
            }
        }
        replace_taskwait_pragma(M, e);
    }
}
