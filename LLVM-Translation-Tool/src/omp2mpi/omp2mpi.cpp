#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
// one may want to clean out unneeded includes here as well

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Transforms/Scalar/SCCP.h>

#include "llvm/IR/Verifier.h"

#include <cstring>
#include <mpi.h>
#include <utility>
#include <vector>

#include "ParallelEntryFunction.h"
#include "comm_patterns/commPatterns.h"
#include "environment.h"
#include "helper.h"
#include "replace_pragmas.h"
#include "struct_pointer.h"
#include "task_pragma.h"

using namespace llvm;

// Debug Macro for easy switching on/off of debug prints
#define DEBUG_OMP_2_MPI_PASS 0

#define VERIFY_OUTPUT 1

// switches to toggle Pass behaviour:
//#define INLINING_PASS_DEFINED_FUNCTIONS
// disable inlining the later optimization passes should inline
#define ERASE_UNUSED_PASS_DEFINED_FUNCTIONS

// array optimizations
#define OPTIMIZE_ARRAY_ACCES_IN_LOOP

// this has no conflicts with the included verifier:
#if DEBUG_OMP_2_MPI_PASS == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

#if VERIFY_OUTPUT == 1
#define Verify_output(x) x
#else
#define Verify_output(x)
#endif

namespace
{
struct Omp2mpiPass : public ModulePass
{
    static char ID;
    Omp2mpiPass() : ModulePass(ID) {}

    // all environment variables to pass to functions
    environment *e = nullptr;

    // changes all globals so that it is not a ptr to var but ptr to comm_info for this var

    void change_all_globals(Module &M, environment *e)
    {
        for (auto *global : e->global->shared_globals)
        {
            global->handle_if_global(M, e);
        }
    }

    // Creates an initialization of MPi at the beginning of function F and adds a
    // Mpi_Finalize in front of every return instruction in F
    void setup_mpi(Module &M, Function *main_function)
    {
        LLVMContext &Ctx = main_function->getContext();
        IRBuilder<> builder(main_function->getContext());
        Type *int32ty = Type::getInt32Ty(Ctx);
        Constant *constant0 = ConstantInt::get(int32ty, 0);

        // init global variables
        e->global->struct_ptr_win =
            new GlobalVariable(M, int32ty, false, GlobalValue::LinkageTypes::CommonLinkage,
                               constant0, "general_struct_ptr_win");

        std::pair<BasicBlock *, BasicBlock *> extra_blocks =
            build_init_and_wrapUp_block(M, e, main_function);
        e->global->setup = extra_blocks.first;
        builder.SetInsertPoint(e->global->setup->getTerminator());
        builder.CreateCall(e->functions->mpi_init);
        builder.CreateCall(e->functions->mpi_create_dynamic_window,
                           {e->global->struct_ptr_win});

        e->global->wrapUp = extra_blocks.second;

        // then fill the wrap up block
        builder.SetInsertPoint(e->global->wrapUp->getFirstNonPHI());
        builder.CreateCall(e->functions->MPI_Win_free, {e->global->struct_ptr_win});
        builder.CreateCall(e->functions->mpi_finalize);

        // create worker main as internal linked func:
        e->worker_Main = Function::Create(FunctionType::get(Type::getVoidTy(Ctx), false),
                                          Function::InternalLinkage, "worker_main", &M);

        BasicBlock *entryB =
            BasicBlock::Create(e->worker_Main->getContext(), "entry", e->worker_Main);
        // create while condition block:
        // note: the switch instruction is the while condition so no "real" while loop
        BasicBlock *while_cond =
            BasicBlock::Create(e->worker_Main->getContext(), "while.cond", e->worker_Main);
        // create while end block:
        // this block is executed if master will exit main
        BasicBlock *while_end =
            BasicBlock::Create(e->worker_Main->getContext(), "while.end", e->worker_Main);

        builder.SetInsertPoint(entryB);
        // create the function
        // alloc buffer for instruction from master
        Value *recv_next_action = builder.CreateAlloca(int32ty);
        Value *recv_next_action_void =
            builder.CreateBitCast(recv_next_action, Type::getInt8PtrTy(Ctx));
        // start the while
        builder.CreateBr(while_cond);

        builder.SetInsertPoint(while_cond);
        // receive next action from master
        Value *count = ConstantInt::get(int32ty, 1);
        Value *type = ConstantInt::get(int32ty, get_mpi_datatype(int32ty));
        std::vector<Value *> args_bcast = {recv_next_action_void, count, type, constant0};
        builder.CreateCall(e->functions->mpi_bcast, args_bcast);
        // load received value
        Value *master_instruction = builder.CreateLoad(recv_next_action);
        // create switch and reserve a case for each microtask
        // default case is end the loop and return
        e->worker_Main_switch = dyn_cast<SwitchInst>(builder.CreateSwitch(
            master_instruction, while_end, e->microtask_functions.size()));

        builder.SetInsertPoint(while_end);
        builder.CreateRetVoid();

        // now insert the call of this func into main for all workers:
        BasicBlock *worker_b = BasicBlock::Create(Ctx, "worker_only", main_function);

        // fill the worker block with call to worker main func
        builder.SetInsertPoint(worker_b);
        builder.CreateCall(e->worker_Main);
        // worker will just exit with 0, master will have real exit code
        // one could instead insert bcast of return code buffer into wrap_up_block
        Value *return_code_buffer = e->global->setup->getFirstNonPHI();
        assert(isa<AllocaInst>(return_code_buffer));
        builder.CreateStore(constant0, return_code_buffer);

        // we need to split the wrap up part into all and master only
        BasicBlock *new_wrap_up =
            SplitBlock(e->global->wrapUp, e->global->wrapUp->getFirstNonPHI());
        BasicBlock *master_only_wrapUp = e->global->wrapUp;
        e->global->wrapUp = new_wrap_up;

        // builder is still in worker_only block
        builder.CreateBr(e->global->wrapUp);

        // master only wrap_up consist of bcast 0 to everyone as 0 is the signal to exit for
        // the workers
        builder.SetInsertPoint(master_only_wrapUp->getFirstNonPHI());
        // as rank of master is 0 we can use the storage location of rank as sending buffer
        Value *rank_as_void = builder.CreateBitCast(e->global->rank, Type::getInt8PtrTy(Ctx));
        builder.CreateCall(e->functions->mpi_bcast, {rank_as_void, count, type, constant0});

        // now insert that only master proceeds to main and other will call worker_main
        Instruction *old_br = e->global->setup->getTerminator();
        assert(isa<BranchInst>(old_br) &&
               dyn_cast<BranchInst>(old_br)->getNumSuccessors() == 1);
        builder.SetInsertPoint(old_br);
        BasicBlock *main_body = old_br->getSuccessor(0);
        Value *my_rank = builder.CreateLoad(e->global->rank);
        Value *cmp = builder.CreateICmpEQ(my_rank, constant0);
        builder.CreateCondBr(cmp, main_body, worker_b);
        old_br->eraseFromParent();
    }

    // erzeuge statt der Openmp implementation von Linked lists die MPI Variante
    void use_mpi_shared_lists(Module &M, environment *e)
    {
        Function *f = M.getFunction("_Z12get_new_listIiEP11Linked_listIT_Ev");
        if (f != nullptr)
        {
            // get first instruction
            inst_iterator I = inst_begin(f);
            Instruction *inst = &*I;
            if (BranchInst *br = dyn_cast<BranchInst>(inst))
            {
                // swap true and false brnaches
                // = swap omp and mpi lists
                assert(br->isConditional());
                br->swapSuccessors();
                Debug(errs() << "Replaced omp linked list with MPI one\n";)
            }
            else
            {
                errs() << "Error invalid Function get_new_linked_list() found!";
            }
        }
        // else if function is not defined,
        // the list implementation was not used so nothing to do
    }

    void handle_lastprivate_pragma(Module &M, environment *e, Value *is_last_flag)
    {
        LLVMContext &Ctx = M.getContext();
        IRBuilder<> builder(Ctx);

        for (auto *u1 : is_last_flag->users())
        {
            if (auto *last_flag_load = dyn_cast<LoadInst>(u1))
            {
                for (auto *u2 : last_flag_load->users())
                {
                    if (auto *last_cmp = dyn_cast<CmpInst>(u2))
                    {
                        for (auto *u3 : last_cmp->users())
                        {
                            if (auto *last_br = dyn_cast<BranchInst>(u3))
                            {
                                assert(last_br->isConditional());
                                // set condition to always true
                                last_br->setCondition(
                                    ConstantInt::get(Type::getInt1Ty(M.getContext()), 1));

                                // insert a bcast from last thread
                                // BasicBlock& BB = ...
                                // for (Instruction &I : BB)
                                for (Instruction &inst : *(last_br->getSuccessor(0)))
                                {
                                    if (auto *store = dyn_cast<StoreInst>(&inst))
                                    {
                                        Value *var = store->getOperand(1);
                                        builder.SetInsertPoint(store->getNextNode());
                                        Value *voidptr = builder.CreateBitCast(
                                            var, Type::getInt8PtrTy(Ctx));
                                        Value *count = builder.getInt32(1);
                                        Value *datatype =
                                            builder.getInt32(get_mpi_datatype(var));
                                        // TODO handle if mpi_datatype is unknown (see Branch
                                        // Tim_other_comm_Patterns)
                                        Value *numprocs = builder.CreateLoad(e->global->size);
                                        Value *numproc1 =
                                            builder.CreateSub(numprocs, builder.getInt32(1));
                                        Value *root = numproc1;
                                        // TODO handle if last process is not numproc -1
                                        // necessary if there might be other for loop
                                        // partitioning in the future
                                        std::vector<Value *> args = {voidptr, count, datatype,
                                                                     root};
                                        builder.CreateCall(e->functions->mpi_bcast, args);
                                        Debug(errs() << "Handled lastprivate clause for var "
                                                     << var->getName() << "\n";)
                                    }
                                }
                            }
                        }
                        // dont need it anymore
                        last_cmp->eraseFromParent();
                    }
                }
                last_flag_load->eraseFromParent();
            }
        }
    }

    // TODO #47
    // TODO is this decreparted?
    void fix_single_value_for_loop(Module &M, environment *e, ParallelFunction *microtask_func,
                                   CallInst *calculate_call)
    {
        std::vector<CallInst *> update_after_calls;
        ReturnInst *return_call;

        for (auto &block : *microtask_func->get_function())
        {
            for (auto &instr : block)
            {
                if (auto *call_inst = dyn_cast<CallInst>(&instr))
                {
                    if (call_inst->getCalledFunction() == e->functions->mpi_update_var_after)
                    {
                        update_after_calls.push_back(call_inst);
                    }
                }
                if (auto *ret_inst = dyn_cast<ReturnInst>(&instr))
                {
                    return_call = ret_inst;
                }
            }
        }

        LLVMContext &Ctx = M.getContext();
        IRBuilder<> builder(return_call);

        for (auto &call : update_after_calls)
        {

            Value *rank = builder.CreateLoad(e->global->rank);
            Value *size = builder.CreateLoad(e->global->size);

            Value *bitcast = call->getArgOperand(2);

            Value *var = nullptr;

            if (auto *bitcast2 = dyn_cast<BitCastInst>(bitcast))
            {
                var = bitcast2->getOperand(0);
            }

            Value *void_ptr = builder.CreateBitCast(var, Type::getInt8PtrTy(Ctx));
            Value *datatype = call->getArgOperand(3);

            std::vector<Value *> args = {rank, size, void_ptr, datatype, calculate_call};

            builder.CreateCall(e->functions->add_missing_recv_calls, args);
        }
    }

    // Replaces the OpenMP version of an parallel for loop with an MPI version.
    void convert_omp_for_pragmas(Module &M, environment *e, ParallelFunction *microtask_func)
    {
        auto kmpc_for_inits = microtask_func->get_kmpc_for_inits();

        for (auto &kmpc_for_init_call : kmpc_for_inits)
        {
            Function *parent_func = nullptr;
            Value *lower_bound = nullptr;
            Value *upper_bound = nullptr;
            Value *is_last_flag = nullptr;
            Value *stride = nullptr;
            Value *loop_incr = nullptr;
            Value *kmpc_for_fini = nullptr;

            std::vector<Value *> for_body;

            parent_func = kmpc_for_init_call->getFunction();

            lower_bound = kmpc_for_init_call->getArgOperand(4);
            upper_bound = kmpc_for_init_call->getArgOperand(5);
            stride = kmpc_for_init_call->getArgOperand(6);
            is_last_flag = kmpc_for_init_call->getArgOperand(3);
            loop_incr = kmpc_for_init_call->getArgOperand(7);

            for (auto &block : *parent_func)
            {
                for (auto &instr : block)
                {
                    if (auto *call_instr = dyn_cast<CallInst>(&instr))
                    {
                        if (call_instr->getCalledFunction()->getName().equals(
                                "__kmpc_for_static_fini"))
                        {
                            kmpc_for_fini = call_instr;
                        }
                    }
                }
            }

            Instruction *iter = dyn_cast<Instruction>(kmpc_for_init_call);

            while (iter != nullptr)
            {
                iter = iter->getNextNode();

                if (iter == nullptr)
                {
                    break;
                }

                if (auto *call_instr = dyn_cast<CallInst>(iter))
                {
                    if (call_instr == kmpc_for_fini)
                    {
                        break;
                    }
                }
            }

            assert(kmpc_for_fini != nullptr);

            Debug(errs() << "for_init:\n"; kmpc_for_init_call->dump();
                  errs() << "lower bound:\n"; lower_bound->dump(); errs() << "upper bound:\n";
                  upper_bound->dump(); errs() << "is last flag:\n"; is_last_flag->dump();
                  errs() << "stride:\n"; stride->dump(); errs() << "loop incr:\n";
                  loop_incr->dump(); errs() << "for fini:\n"; kmpc_for_fini->dump();)

                LLVMContext &Ctx = M.getContext();
            IRBuilder<> builder(dyn_cast<Instruction>(kmpc_for_init_call));

            Value *mpi_size = builder.CreateLoad(e->global->size);
            Value *mpi_rank = builder.CreateLoad(e->global->rank);

            std::vector<Value *> args = {lower_bound, upper_bound, stride, mpi_rank, mpi_size};

            Constant *calculate_func = nullptr;
            if (lower_bound->getType()->getPointerElementType() == builder.getInt32Ty())
            {
                calculate_func = e->functions->calculate_for_boundaries_int;
            }
            else if (lower_bound->getType()->getPointerElementType() == builder.getInt64Ty())
            {
                calculate_func = e->functions->calculate_for_boundaries_long;
            }
            else
            {
                errs() << "Unknown Type of for loop index:\n";
                lower_bound->getType()->dump();
            }
            assert(calculate_func != nullptr);

            CallInst *calculate_call = builder.CreateCall(calculate_func, args);

            auto *init_call = dyn_cast<Instruction>(kmpc_for_init_call);
            auto *fini_call = dyn_cast<Instruction>(kmpc_for_fini);

            // handle the lastprivate clause if exists
            handle_lastprivate_pragma(M, e, is_last_flag);

            assert(fini_call != nullptr);

            init_call->eraseFromParent();
            fini_call->eraseFromParent();

            /*
             TODO is this decreparted??
            if (microtask_func->has_single_value_vars() ||
                microtask_func->has_single_value_ptr_vars())
            {
                fix_single_value_for_loop(M, e, microtask_func, calculate_call);
            }
            */
        }
    }

    // function will insert communication needed for the worker to call given func with given
    // args
    void get_worker_to_call_this(llvm::Module &M, environment *e,
                                 llvm::CallInst *insert_before, ForkEntry *func_to_call,
                                 std::vector<llvm::Value *> *args_to_bcast,
                                 variable_distribution_function distribution_function_to_use)
    {
        IRBuilder<> builder(insert_before);
        ConstantInt *case_value = builder.getInt32(e->worker_Main_switch->getNumCases() + 1);

        Value *buffer_alloc = builder.CreateAlloca(builder.getInt32Ty());
        builder.CreateStore(case_value, buffer_alloc);
        Value *buffer_void = builder.CreateBitCast(buffer_alloc, builder.getInt8PtrTy());
        Value *constant1 = builder.getInt32(1);
        Value *constant0 = builder.getInt32(0);
        Value *datatype = builder.getInt32(get_mpi_datatype(buffer_alloc));

        // bcast the case_value
        std::vector<Value *> args = {buffer_void, constant1, datatype, constant0};
        builder.CreateCall(e->functions->mpi_bcast, args);

        // add the corresponding case
        BasicBlock *caseBlock =
            BasicBlock::Create(e->worker_Main->getContext(),
                               func_to_call->get_function()->getName(), e->worker_Main);
        e->worker_Main_switch->addCase(case_value, caseBlock);
        builder.SetInsertPoint(caseBlock);

        // in mpi the join barrier must be explicit
        CallInst *worker_insert = builder.CreateCall(e->functions->mpi_barrier);
        // branch back to while condition (wait for next instruction from master)
        Instruction *block_term = builder.CreateBr(e->worker_Main_switch->getParent());

        std::vector<Value *> args_for_workers;
        //    bcast_shared_vars_from_master(M, e, insert_before, worker_insert,
        //    args_to_bcast);

        args_for_workers = distribution_function_to_use(M, e, func_to_call, insert_before,
                                                        worker_insert, args_to_bcast);

        // reset insertion point so that it is after new instructions from
        // bcast_shared_vars_from_master but before barrier
        builder.SetInsertPoint(worker_insert);
        Debug(errs() << "insert call for workers:\n master args:\n"; for (auto *arg
                                                                          : *args_to_bcast) {
            arg->dump();
        } errs() << "insert call for workers:\n worker args:\n";
              for (auto *arg
                   : args_for_workers) { arg->dump(); })

            builder.CreateCall(func_to_call->get_function(), args_for_workers);
    }

    static bool not_handle_this_global(SharedVariable *global)
    {
        assert(isa<GlobalVariable>(global->value()));
        auto *as_global = dyn_cast<GlobalVariable>(global->value());

        return as_global->isExternallyInitialized() || isa<SharedUnhandledType>(global) ||
               is_global_defined_by_openmp(as_global);
    }

    void analyze_globals(Module &M, environment *e)
    {
        for (auto &global : M.globals())
        {
            e->global->shared_globals.push_back(SharedVariable::Create(M, e, &global));
        }
        // currently there are no globals inserted by pass as we are in analyze

        // we need to remove globals that cannot be handled
        // and the one defined by OpenMP
        e->global->shared_globals.erase(std::remove_if(e->global->shared_globals.begin(),
                                                       e->global->shared_globals.end(),
                                                       not_handle_this_global),
                                        e->global->shared_globals.end());

        // TODO ? warn the user if there where unhandled globals not defined by openmp
        Debug(errs() << "Globals that will be handled:\n";
              for (auto *g
                   : e->global->shared_globals) { g->dump(); })
    }

    // analyze which replacements should be done
    // no operations/calls in this function should change the module
    // so that the result of the analysis do not interfere with changes already made
    void analyze(Module &M, environment *e)
    {
        analyze_globals(M, e);
        // find kmpc_fork_calls
        auto kmpc_fork_calls = get_function_users(M, StringRef("__kmpc_fork_call"));
        for (auto &fork_call_user : kmpc_fork_calls)
        {
            if (auto *fork_call_inst = dyn_cast<CallInst>(fork_call_user))
            {
                ForkEntry *microtask_func = new ForkEntry(M, e, fork_call_inst);
                e->microtask_functions.push_back(microtask_func);
            }
        }
    }

    // convert one function to mpi
    void replace_Function(Module &M, ParallelFunction *microtask_func)
    {
        // TODO refactoring of comm pattern
        {
            Debug(errs() << "Using default pattern for function "
                         << microtask_func->get_function()->getName() << "\n";)
                // deafult_comm_Pattern(M, e, microtask_func);
                // should work fine as default, as it is better for array and use default for
                // others
                distribute_array_comm_Pattern(M, e, microtask_func);
        }

        if (microtask_func->uses_parallel_for())
        {
            convert_omp_for_pragmas(M, e, microtask_func);
        }

        // do it after parallel for conversion:
        // TODO later only do it when using default pattern
#ifdef OPTIMIZE_ARRAY_ACCES_IN_LOOP
        optimize_array_loop_acces(M, e, microtask_func);
#endif
    }

    // do the actual conversion to mpi
    void replace(Module &M)
    {
        change_all_globals(M, e);
        // set up mpi
        auto *main_func = M.getFunction(StringRef("main"));
        setup_mpi(M, main_func);

        // handle structs before reduction because the sizes of new struct types might be used
        // in reduction communication
        handle_struct_pointer(M, e);

        use_mpi_shared_lists(M, e);
        replace_pragmas(M, e);
        handle_task_pragma(M, e); // do it after barrier Pragma was replaced

        // Modify all ParallelEntryFunction functions
        for (auto *microtask_func : e->microtask_functions)
        {
            auto *fork_call_inst = microtask_func->get_fork_call_inst(M);
            // Replace the fork call with a direct call to the microtask
            auto *shared_vars_arg = fork_call_inst->getArgOperand(1);
            if (auto *num_shared_vars = dyn_cast<ConstantInt>(shared_vars_arg))
            {
                LLVMContext &Ctx = M.getContext();
                IRBuilder<> builder(fork_call_inst);

                // Construct arguments for micotask function call
                std::vector<Value *> args;

                Value *null_ptr = ConstantPointerNull::get(
                    PointerType::PointerType::getUnqual(Type::getInt32Ty(Ctx)));

                args.push_back(null_ptr);
                args.push_back(null_ptr);

                for (auto i = 0; i < num_shared_vars->getSExtValue(); i++)
                {
                    args.push_back(fork_call_inst->getArgOperand(3 + i));
                }

                variable_distribution_function distribution_func_to_use = nullptr;
                if (microtask_func->use_tasking())
                {
                    distribution_func_to_use = &task_init_shared_vars_from_master;
                }
                else
                {
                    distribution_func_to_use = &distribute_shared_vars_from_master;
                }
                assert(distribution_func_to_use != nullptr);

                get_worker_to_call_this(M, e, fork_call_inst, microtask_func, &args,
                                        distribution_func_to_use);
                // Replace fork call with direct call to microtask
                builder.CreateCall(microtask_func->get_function(), args);
                // in mpi the join barrier must be explicit
                builder.CreateCall(e->functions->mpi_barrier);
                fork_call_inst->eraseFromParent();
            }

            replace_Function(M, microtask_func);
            // TODO ?? check that function was not already converted to MPI
            //(therefore are the isEqual and other_usage functions of ParallelFunction class)

            for (auto *called_function : microtask_func->get_all_functions_called())
            {
                replace_Function(M, called_function);
            }
        }

        for (auto *task_entry : e->task_functions)
        {
            replace_Function(M, task_entry);
            // TODO ?? check that function was not already converted to MPI
            //(therefore are the isEqual and other_usage functions of ParallelFunction class)

            for (auto *called_function : task_entry->get_all_functions_called())
            {
                replace_Function(M, called_function);
            }
        }
    }

    void analyse_memory_allocation(Module &M)
    {
        auto malloc_users = get_function_users(M, "malloc");

        for (auto &x : malloc_users)
        {
            Debug(errs() << "Malloc call: "; x->dump();) for (auto *u : x->users())
            {
                Debug(errs() << "  -> "; u->dump();) for (auto *u2 : u->users())
                {
                    Debug(errs() << "    -> "; u2->dump();)
                }
            }

            if (auto *call = dyn_cast<CallInst>(x))
            {
                Debug(call->dump(); call->getArgOperand(0)->dump();) IRBuilder<> builder(
                    call->getNextNode());
                LLVMContext &Ctx = M.getContext();

                std::vector<Value *> args = {call, call->getArgOperand(0)};

                builder.CreateCall(e->functions->log_memory_allocation, args);
            }
        }

        auto znam_users = get_function_users(M, "_Znam");

        for (auto &x : znam_users)
        {
            Debug(errs() << "Znam call: "; x->dump();)

                if (auto *call = dyn_cast<CallInst>(x))
            {
                Debug(call->dump(); call->getArgOperand(0)->dump();) IRBuilder<> builder(
                    call->getNextNode());
                LLVMContext &Ctx = M.getContext();

                std::vector<Value *> args = {call, call->getArgOperand(0)};

                builder.CreateCall(e->functions->log_memory_allocation, args);
            }
        }
    }

    // Pass starts here
    virtual bool runOnModule(Module &M)
    {
        e = new environment();
        e->global = new struct global_variables();
        Debug(errs() << "BEGIN ANALYSIS:\n\n";) analyze(M, e);

        // Don't change anything if there is no fork call
        if (e->microtask_functions.empty())
        {
            errs() << "No OpenMP fork call found. It will compile just as a normal non MPI "
                      "Application";
            return false;
        }

        Debug(errs() << "\n\nCODE BEFORE PASS:\n\n"; M.dump();)

            // for debugging:
            // ASK_TO_CONTINIUE

            // init the environment to start tsanslaion
            load_external_definitions(M, e);

        e->TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

        // this will insert the new struct types but not changes more so far:
        build_old_to_new_struct_type_map(M, e);

        Debug(errs() << "\n\nMEMORY ALLOCATION ANALYSIS:\n\n";) analyse_memory_allocation(M);

        Debug(errs() << "\n\nPERFORM CHANGES:\n\n";) replace(M);

        Debug(errs() << "\n\nCODE AFTER PASS:\n\n"; M.dump();)
            Debug(Verify_output(errs() << "\nVerification:\n";))
                Verify_output(bool broken_debug; if (verifyModule(M, &errs(), &broken_debug)) {
                    errs() << "Verification FAIL\n";
                    return true; // abort
                } else { Debug(errs() << "Verification Done\n";) } if (broken_debug) {
                    errs() << "Debug-Info is Broken\n";
                })

                    load_external_implementations(M, e);

        // so that e. g. valgrind reports are more useful
        Debug(never_inline_external_functions(M, e);)
#ifdef INLINING_PASS_DEFINED_FUNCTIONS
            inline_external_functions(M, e);
#endif

        // fill execute_task so that linker will find a definition
        if (e->execute_task->isDeclaration())
        {
            BasicBlock *entryB =
                BasicBlock::Create(e->execute_task->getContext(), "entry", e->execute_task);
            IRBuilder<> builder(entryB);
            builder.CreateRetVoid();
        }

#ifdef ERASE_UNUSED_PASS_DEFINED_FUNCTIONS
        erase_not_used_external_functions(M, e);
#endif

        // verify again after inlining and erasing of unused functions:
        Debug(Verify_output(errs() << "\nVerification:\n";))
            Verify_output(if (verifyModule(M, &errs(), &broken_debug)) {
                errs() << "Verification FAIL\n";
            } else { Debug(errs() << "Verification Done\n";) } if (broken_debug) {
                errs() << "Debug-Info is Broken\n";
            })

            // wrap_up
            delete e->external_module;
        // deletion of libs will close the files
        delete e->types;
        delete e->functions;
        delete e->global;
        delete e;

        return true;
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.addRequired<TargetLibraryInfoWrapperPass>();
        // AU.addRequired<SCCPPass>();
    }

}; // namespace
} // namespace

char Omp2mpiPass::ID = 0;

// register pass to use with opt
// false:  Only looks at CFG
// false : Analysis Pass

static RegisterPass<Omp2mpiPass> X("omp2mpi", "OpenMP to MPI Pass", false, false);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerOmp2mpiPass(const PassManagerBuilder &, legacy::PassManagerBase &PM)
{
    PM.add(new Omp2mpiPass());
}

static RegisterStandardPasses RegisterMyPass(PassManagerBuilder::EP_ModuleOptimizerEarly,
                                             registerOmp2mpiPass);

static RegisterStandardPasses RegisterMyPass0(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                              registerOmp2mpiPass);
