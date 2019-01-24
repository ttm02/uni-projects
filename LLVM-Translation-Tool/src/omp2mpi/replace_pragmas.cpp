// Functions to Replace openmp functions
// or an openmp Pragma functionality

#include "replace_pragmas.h"
#include "comm_patterns/commPatterns.h"
#include "environment.h"
#include "helper.h"
#include "struct_pointer.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#define DEBUG_REPLACE_PRAGMAS 0

#if DEBUG_REPLACE_PRAGMAS == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

// Replaces calls of basic ompenmp functions like omp_get_num_threads and
// omp_get_thread_num
void replace_omp_functions(Module &M, environment *e)
{
    // one can even replace this with loading the global variable
    auto get_thread_num_users = get_function_users(M, StringRef("omp_get_thread_num"));
    for (auto *instruction : get_thread_num_users)
    {
        if (auto *invoke = dyn_cast<InvokeInst>(instruction))
        {
            invoke->setCalledFunction(e->functions->mpi_rank);
        }
        else if (auto *call = dyn_cast<CallInst>(instruction))
        {
            call->setCalledFunction(e->functions->mpi_rank);
        }
    }

    auto get_num_threads_users = get_function_users(M, StringRef("omp_get_num_threads"));
    for (auto *instruction : get_num_threads_users)
    {
        if (auto *invoke = dyn_cast<InvokeInst>(instruction))
        {
            invoke->setCalledFunction(e->functions->mpi_size);
        }
        else if (auto *call = dyn_cast<CallInst>(instruction))
        {
            call->setCalledFunction(e->functions->mpi_size);
        }
    }

    // this function shuold be threated the same as omp_get_thread_num
    auto get_global_thread_num_users =
        get_function_users(M, StringRef("__kmpc_global_thread_num"));
    for (auto *instruction : get_global_thread_num_users)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            ReplaceInstWithInst(inst, new LoadInst(e->global->rank));
        }
    }

    // omp_get_max_threads() get number of maximun threads available
    // in mpi this is number of mpi procs currently running
    auto get_omp_get_max_threads_users =
        get_function_users(M, StringRef("omp_get_max_threads"));
    for (auto *instruction : get_omp_get_max_threads_users)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            ReplaceInstWithInst(inst, new LoadInst(e->global->size));
        }
    }
}
void replace_barrier_pragma(Module &M, environment *e)
{
    // openmp barrier
    auto get_barrier_users = get_function_users(M, StringRef("__kmpc_barrier"));
    for (auto *instruction : get_barrier_users)
    {
        if (auto *invoke = dyn_cast<InvokeInst>(instruction))
        {
            ReplaceInstWithInst(invoke, CallInst::Create(e->functions->mpi_barrier));
            Debug(
                // TODO is there a different stream for warnings?
                errs() << "Warning: INVOKE Barrier was replaced with CALL Barrier";
                // the MPI_Barrier should not throw any exceptions anyway
            )
        }
        else if (auto *call = dyn_cast<CallInst>(instruction))
        {
            // call->setCalledFunction(mpi_barrier_func);
            ReplaceInstWithInst(call, CallInst::Create(e->functions->mpi_barrier));
        }
    }
}

// convert openmp single and master sections
void parse_single_section_pragmas(Module &M, environment *e)
{
    auto get_master_start = get_function_users(M, StringRef("__kmpc_master"));
    for (auto *instruction : get_master_start)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            // the instruction asks for the thread num
            // the openmp code following checks if the thread num is 0 and
            // will skip the next section if not; so it does not need to be altered
            Instruction *next = inst->getNextNode();
            ReplaceInstWithInst(inst, new LoadInst(e->global->rank));
            if (auto *cmpinst = dyn_cast<ICmpInst>(next))
            {
                cmpinst->setPredicate(CmpInst::Predicate::ICMP_EQ);
                // if rank ==0 do the master task else skip it
                // (i don't get why this check is different in openmp version)
            }
            else
            {
                Debug(errs() << "Error: no compare instruction in openmp master";)
            }
        }
    }

    auto get_master_end = get_function_users(M, StringRef("__kmpc_end_master"));
    for (auto *instruction : get_master_end)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            inst->eraseFromParent();
            // delete it as it is not needed in mpi
        }
    }
    // implement pragma omp single as pragma omp master as this is also allowed
    // by the standard the barrier at the end of single will be converted to mpi
    // in replace_omp_functions therefore code is nearly the same as above
    auto get_single_start = get_function_users(M, StringRef("__kmpc_single"));
    for (auto *instruction : get_single_start)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            Debug(errs() << "Replace Single Pragma:\n";)
                // the instruction asks for the thread num
                // the openmp code following checks if the thread num is 0 and
                // will skip the next section if not; so it does not need to be altered
                Instruction *next = inst->getNextNode();
            ReplaceInstWithInst(inst, new LoadInst(e->global->rank));
            if (auto *cmpinst = dyn_cast<ICmpInst>(next))
            {
                cmpinst->setPredicate(CmpInst::Predicate::ICMP_EQ);
                // if rank ==0 do the master task else skip it
                // (i don't get why this check is different in openmp version)
            }
            else
            {
                Debug(errs() << "Error: no compare instruction in openmp single";)
            }
        }
    }

    auto get_single_end = get_function_users(M, StringRef("__kmpc_end_single"));
    for (auto *instruction : get_single_end)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            inst->eraseFromParent();
            // delete it as it is not needed in mpi
        }
    }
}

enum Reduce_Operation
{
    user_defined = 0,
    sub = 1,     // NOT SUPPORTED IN MPI
    bit_xor = 2, // NOT SUPPORTED IN MPI
    add = MPI_SUM,
    mul = MPI_PROD,
    max = MPI_MAX,
    min = MPI_MIN,
    bit_and = MPI_BAND,
    logical_and = MPI_LAND,
    bit_or = MPI_BOR,
    logical_or = MPI_LOR,
};

Reduce_Operation get_operation_performed(Module &M, environment *e, Instruction *inst)
{
    Debug(errs() << "Value Used in Reduce Operation:"; inst->dump();)

        switch (inst->getOpcode())
    {
    case Instruction::Add:
        return add;
    case Instruction::FAdd:
        return add;
        // SUB is smae as ADD in OpenMP standard
        // BTW: then this case should never trigger anyway because openmp should already use
        // add operation
    case Instruction::Sub:
        return add;
    case Instruction::FSub:
        return add;
    case Instruction::FMul:
        return mul;
    case Instruction::Mul:
        return mul;

    case Instruction::And:
        return bit_and;
    case Instruction::Or:
        return bit_or;
    case Instruction::Xor:
        return bit_xor;

    case Instruction::ICmp:
        // get which logical is done:
        if (auto *cmp = dyn_cast<CmpInst>(inst))
        {
            switch (cmp->getPredicate())
            {
            case CmpInst::Predicate::ICMP_SLT:
                return min;
            case CmpInst::Predicate::ICMP_SGT:
                return max;

            case CmpInst::Predicate::ICMP_NE:
                // Logical Operation:
                // inst is %tobool
                for (auto *u : inst->users())
                {
                    if (auto *br = dyn_cast<BranchInst>(u))
                    {
                        assert(!br->isUnconditional());

                        BasicBlock *br_true = br->getSuccessor(0);
                        BasicBlock *br_false = br->getSuccessor(1);

                        if (br_true->getNextNode() == br_false)
                        {
                            return logical_and;
                        }
                        else
                        {
                            return logical_or;
                        }
                    }
                }
                __builtin_unreachable(); // will return before

            default:
                errs() << "Error Invalid Reduction Operation found:";
                return user_defined;
            }
        }
        break;

    case Instruction::FCmp:
        // get which logical is done:
        if (auto *cmp = dyn_cast<CmpInst>(inst))
        {
            switch (cmp->getPredicate())
            {
            case CmpInst::Predicate::FCMP_OLT:
                return min;
            case CmpInst::Predicate::FCMP_OGT:
                return max;

            case CmpInst::Predicate::FCMP_UNE:
                // Logical Operation:
                // inst is %tobool
                for (auto *u : inst->users())
                {
                    if (auto *br = dyn_cast<BranchInst>(u))
                    {
                        assert(!br->isUnconditional());

                        BasicBlock *br_true = br->getSuccessor(0);
                        BasicBlock *br_false = br->getSuccessor(1);

                        if (br_true->getNextNode() == br_false)
                        {
                            return logical_and;
                        }
                        else
                        {
                            return logical_or;
                        }
                    }
                }
                __builtin_unreachable(); // will return before

            default:
                errs() << "Error Invalid Reduction Operation found:";
                return user_defined;
            }
        }
        break;

        // should never reach this case anyway!
        // user defined is handled in parent function as param to user defined is not the value
        // but the pointer to value
    case Instruction::Call:
        errs() << "Error Invalid Reduction Operation found:";
        return user_defined;

    default:
        errs() << "Error Invalid Reduction Operation found:";
        inst->dump();
    }
    return user_defined;
}

// i is the index of the var to be reduced (openmp stores all pointer to the variables to be
// reduced in an array)
// reduce_func is the openmp added Function that performs the reduce
// user_defined_call_func is output-param set if user_defined Func is found
Reduce_Operation get_reduce_operation(Module &M, environment *e, Function *reduce_func,
                                      Value *i, Function **user_defined_call_func)
{
    // Instruction *first_inst = reduce_func->getEntryBlock().getFirstNonPHI();

    Value *arg0 = reduce_func->arg_begin();

    unsigned int num_uses_first_arg = 0;
    for (auto *first_inst : arg0->users())
    {
        // else we have an invalid reduction function:
        assert(isa<BitCastInst>(first_inst));
        assert(num_uses_first_arg == 0);
        ++num_uses_first_arg;

        for (auto *u : first_inst->users())
        {
            if (auto *array_ptr = dyn_cast<GetElementPtrInst>(u))
            {
                assert(array_ptr->getNumOperands() == 3);
                // if Pointer to var i is loaded
                if (array_ptr->getOperand(2) == i)
                {
                    // traverse to the load of actual value
                    for (auto *ptr_u : array_ptr->users())
                    {
                        if (auto *ptr_load = dyn_cast<LoadInst>(ptr_u))
                        {
                            for (auto *ptr_load_u : ptr_load->users())
                            {
                                if (auto *ptr_cast = dyn_cast<BitCastInst>(ptr_load_u))
                                {
                                    for (auto *ptr_cast_u : ptr_cast->users())
                                    {
                                        if (auto *val_load = dyn_cast<LoadInst>(ptr_cast_u))
                                        {
                                            // actual value loaded
                                            for (auto *val_u : val_load->users())
                                            {
                                                if (auto *red_inst =
                                                        dyn_cast<Instruction>(val_u))
                                                {
                                                    // for min and max:
                                                    // use the cmp instruction nd not the phi
                                                    // where the result of cmp is gatehred
                                                    if (!isa<PHINode>(red_inst))
                                                    {
                                                        // determine type of instruction the
                                                        // value is used in
                                                        return get_operation_performed(
                                                            M, e, red_inst);
                                                    }
                                                }
                                            }
                                        }
                                        // if user_ defined: no load of value
                                        // the ptr is passed to function
                                        if (auto *call_op = dyn_cast<CallInst>(ptr_cast_u))
                                        {
                                            {
                                                Debug(errs()
                                                          << "Found user_defined reduction\n";)
                                                    *user_defined_call_func =
                                                        call_op->getCalledFunction();
                                                return user_defined;
                                            }
                                        }
                                        // due to the handeling of struct ptr this also might
                                        // be a bitcast (cast to old struct type) then we have
                                        // to follow this cast in order to get the call inst
                                        if (auto *pass_inserted_cast =
                                                dyn_cast<BitCastInst>(ptr_cast_u))
                                        {
                                            for (auto *pass_cast_u :
                                                 pass_inserted_cast->users())
                                            {
                                                if (auto *call_op =
                                                        dyn_cast<CallInst>(pass_cast_u))
                                                {
                                                    Debug(errs() << "Found user_defined "
                                                                    "reduction\n";)
                                                        *user_defined_call_func =
                                                            call_op->getCalledFunction();
                                                    return user_defined;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // if anything above failes:
    errs() << "Error Invalid Reduction function found:\n";
    reduce_func->dump();
    return user_defined;
}

void handle_user_defined_reduce(Module &M, environment *e, Value *var_ptr,
                                Function *user_defined_func, Instruction *insert)
{
    Debug(errs() << "Handle User defined reduce\n";) LLVMContext &Ctx = M.getContext();
    IRBuilder<> builder(Ctx);

    Type *var_type = nullptr;
    Type *var_ptr_type = var_ptr->getType();

    // If it is a shared array
    if (var_ptr->getType()->getPointerElementType()->isArrayTy())
    {
        var_type = var_ptr->getType()->getPointerElementType()->getArrayElementType();
    }
    // If it is a shared single value
    else
    {
        var_type = var_ptr->getType()->getPointerElementType();
    }

    // if it is a struct
    if (var_type->isStructTy())
    {
        // remind that we need to communicate the whole struct (with communication info)
        var_type = get_default_comm_info_struct_type(M, e, var_type);
    }

    // build wrapper function
    // wrapper function will cast types from void* back to original pointer type

    // function is implicitly renamed if there are conflicts
    // external function will call this one (mpi_manual_reduce will get a pointer to it)
    Function *wrapper_F = Function::Create(e->types->MPI_User_function,
                                           Function::ExternalLinkage, "reduce_wrapper", &M);

    BasicBlock *BB = BasicBlock::Create(wrapper_F->getContext(), "entry", wrapper_F);
    builder.SetInsertPoint(BB);

    if (user_defined_func->getFunctionType()
            ->getFunctionParamType(0)
            ->getPointerElementType()
            ->isStructTy())
    {
        build_reduce_wrapper_for_struct_type_reduction(M, e, user_defined_func, BB);

        // build call to mpi_manual_reduce
        // as within the reduction of structs there is communication it is NOT allowed to use
        // the wrapperF as an MPI_Op because communication within an user defined MPI operation
        // is not allowed
        builder.SetInsertPoint(insert);

        Value *buf = builder.CreateBitCast(var_ptr, Type::getInt8PtrTy(Ctx));
        // get size of var in bytes
        size_t bytes = get_size_in_Byte(M, var_type);

        Value *data_size = ConstantInt::get(Type::getInt32Ty(Ctx), bytes);
        std::vector<Value *> args = {buf, data_size, wrapper_F};
        builder.CreateCall(e->functions->mpi_manual_reduce, args);
    }
    else
    {
        // für nicht structs kann ompcombiner weiter verwendet werden
        std::vector<Value *> casted_args;
        unsigned int i = 0;

        for (auto &arg : wrapper_F->args())
        {
            if (i < 2) // else we ignore the args as ompcombiner do not use them
            {
                Type *cast_to = user_defined_func->getFunctionType()->getFunctionParamType(i);
                Value *cast = builder.CreateBitCast(&arg, cast_to);
                casted_args.insert(casted_args.begin(), cast);
                // we need to swap the position of the args, as MPI user defined expects the
                // reduction result to be store in second arg but omp use first arg for that
            }
            ++i;
        }
        builder.CreateCall(user_defined_func, casted_args);
        builder.CreateRetVoid();

        // insert global that hold the MPI op handle
        Type *int32ty = Type::getInt32Ty(Ctx);
        Constant *constant0 = ConstantInt::get(int32ty, 0);
        Value *mpi_op = new GlobalVariable(
            M, int32ty, false, GlobalValue::LinkageTypes::CommonLinkage, constant0, "mpi_op");
        // init the operation handle
        builder.SetInsertPoint(e->global->setup->getTerminator());
        builder.CreateCall(e->functions->MPI_Op_create,
                           {wrapper_F, ConstantInt::get(int32ty, 1), mpi_op});
        // free it
        builder.SetInsertPoint(e->global->wrapUp->getFirstNonPHI());
        builder.CreateCall(e->functions->MPI_Op_free, {mpi_op});

        // Insert a call to allreduce with the user defined operation
        builder.SetInsertPoint(insert);
        Value *buf = builder.CreateBitCast(var_ptr, Type::getInt8PtrTy(Ctx));
        size_t bytes = get_size_in_Byte(M, var_type);
        Value *data_size = ConstantInt::get(Type::getInt32Ty(Ctx), bytes);
        Value *datatype = ConstantInt::get(Type::getInt32Ty(Ctx), MPI_BYTE);
        // TODO use get_mpi_datatype
        Value *mpi_operation = builder.CreateLoad(mpi_op);
        std::vector<Value *> args = {buf, data_size, datatype, mpi_operation};
        builder.CreateCall(e->functions->mpi_allreduce, args);
    }
    // wrapper_F->dump();

    // no communication within the user defined Reduction:
    for (auto &b : user_defined_func->getBasicBlockList())
    {
        e->no_further_comm.insert(&b);
    }
}

void build_single_value_reduction(Module &M, environment *e, Value *var_ptr,
                                  Reduce_Operation op, Function *user_defined_func,
                                  Instruction *insert)
{
    LLVMContext &Ctx = M.getContext();
    IRBuilder<> builder(Ctx);
    builder.SetInsertPoint(insert);

    if (op == user_defined)
    {
        handle_user_defined_reduce(M, e, var_ptr, user_defined_func, insert);
        return;
    }
    if (op == sub)
    {
        // Do nothing
        errs() << "Subtraction is not supported. It is not Associative nor Commutative and "
                  "therefore the result will be Indeterministic in MPI\n";
        return;
    }
    if (op == bit_xor)
    {
        // build a new "user defined" function to xor the given data
        Type *var_type = nullptr;
        Type *var_ptr_type = var_ptr->getType();

        // If it is a shared array
        if (var_ptr->getType()->getPointerElementType()->isArrayTy())
        {
            var_type = var_ptr->getType()->getPointerElementType()->getArrayElementType();
        }
        // If it is a shared single value
        else
        {
            var_type = var_ptr->getType()->getPointerElementType();
        }
        FunctionType *func_ty =
            FunctionType::get(Type::getVoidTy(Ctx), {var_ptr_type, var_ptr_type}, false);
        // function is implicitly renamed if there are conflicts
        // internal function no one else shuold use it
        // TODO set always inline flag
        Function *xor_F =
            Function::Create(func_ty, Function::InternalLinkage, "xor_reduction", &M);

        BasicBlock *BB = BasicBlock::Create(xor_F->getContext(), "entry", xor_F);
        builder.SetInsertPoint(BB);
        std::vector<Value *> xor_args;
        for (auto &arg : xor_F->args())
        {
            Value *load = builder.CreateLoad(&arg);
            xor_args.push_back(load);
        }
        Value *xor_result = builder.CreateXor(xor_args[0], xor_args[1], "xor");
        builder.CreateStore(xor_result, xor_F->arg_begin()); // store it to first location
        builder.CreateRetVoid();

        Debug(errs() << "Created Xor for type "; var_type->dump();)
            // handle it as it where user_defined:
            handle_user_defined_reduce(M, e, var_ptr, xor_F, insert);
        return;
    }
    // if non above: one can directly insert call to mpi allreduce:

    // int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
    //                 MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
    Value *buf = builder.CreateBitCast(var_ptr, Type::getInt8PtrTy(Ctx));
    Value *count = ConstantInt::get(Type::getInt32Ty(Ctx), 1);
    if (get_mpi_datatype(var_ptr) == MPI_BYTE)
    {
        count = ConstantInt::get(Type::getInt32Ty(Ctx), 1 * get_size_in_Byte(M, var_ptr));
    }
    Value *datatype = ConstantInt::get(Type::getInt32Ty(Ctx), get_mpi_datatype(var_ptr));
    Value *mpi_operation = ConstantInt::get(Type::getInt32Ty(Ctx), op);
    std::vector<Value *> args = {buf, count, datatype, mpi_operation};

    builder.CreateCall(e->functions->mpi_allreduce, args);
    Debug(errs() << "Inserted Call to MPI_allreduce\n";)
}

// return true if at least one variabnle uses min or max reduction
bool insert_reduction(Module &M, environment *e, Value *data_ptr, Function *reduce_func,
                      Instruction *insert)
{
    bool result = false;
    if (auto *cast = dyn_cast<BitCastInst>(data_ptr))
    {
        // make shure it is the right cast:
        assert(cast->getDestTy() == Type::getInt8PtrTy(M.getContext()));
        assert(cast->getNumOperands() == 1);

        Value *array = cast->getOperand(0);

        // for each store to the array:
        for (auto *array_u : array->users())
        {
            if (auto *elem_ptr = dyn_cast<GetElementPtrInst>(array_u))
            {
                assert(elem_ptr->getNumOperands() == 3);
                Value *array_index = elem_ptr->getOperand(2);

                // get Ptr to Variable that is reduced
                for (auto *elem_ptr_u : elem_ptr->users())
                {
                    if (auto *store_to_array = dyn_cast<StoreInst>(elem_ptr_u))
                    {
                        assert(store_to_array->getNumOperands() == 2);

                        if (auto *var_ptr_cast =
                                dyn_cast<BitCastInst>(store_to_array->getOperand(0)))
                        {
                            // make shure it is the right cast:
                            assert(var_ptr_cast->getDestTy() ==
                                   Type::getInt8PtrTy(M.getContext()));
                            assert(var_ptr_cast->getNumOperands() == 1);
                            auto *var_ptr = var_ptr_cast->getOperand(0);

                            // get operation performed
                            Function *user_defined_call;
                            Reduce_Operation op = get_reduce_operation(
                                M, e, reduce_func, array_index, &user_defined_call);
                            // insert one reduction
                            build_single_value_reduction(M, e, var_ptr, op, user_defined_call,
                                                         insert);
                            result = result || (op == min || op == max);
                        }
                    }
                }
            }
        }
    }
    return result;
}

void handle_reduce_call(Module &M, environment *e, CallInst *reduce_start_call)
{

    if (reduce_start_call->getNumArgOperands() != 7)
    {
        Debug(errs() << "Error: invalid reduction call found\n";
              errs() << "Warning: MPI Reduce always has implicit Barrier\n";)
    }
    else
    {
        Value *num_vars = reduce_start_call->getArgOperand(2);
        Value *size = reduce_start_call->getArgOperand(3);
        Value *data_ptr = reduce_start_call->getArgOperand(4);
        Value *reduce_func = reduce_start_call->getArgOperand(5);
        std::vector<Value *> args = {num_vars, size, data_ptr, reduce_func};

        if (auto *func = dyn_cast<Function>(reduce_func))
        {

            bool comparision_used = insert_reduction(M, e, data_ptr, func, reduce_start_call);
            // reduction result should be only present on master
            // if result var is shared, master will just wirte it to the shared var
            // therefore only master continiues into the block where result is written from
            // local var to shared var (just the same as in openMP)
            IRBuilder<> builder(reduce_start_call);
            Value *rank = builder.CreateLoad(e->global->rank);
            Value *cmp_result =
                builder.CreateICmp(CmpInst::Predicate::ICMP_EQ, rank,
                                   ConstantInt::get(Type::getInt32Ty(M.getContext()), 0));

            ReplaceInstWithInst(reduce_start_call,
                                BitCastInst::CreateIntegerCast(
                                    cmp_result, Type::getInt32Ty(M.getContext()), false));
        }
    }
}

void parse_reduction(Module &M, environment *e)
{
    Function *user_defined_call;
    auto get_reduce_nowait_start = get_function_users(M, StringRef("__kmpc_reduce_nowait"));
    for (auto *instruction : get_reduce_nowait_start)
    {
        handle_reduce_call(M, e, dyn_cast<CallInst>(instruction));
    }

    auto get_reduce_start = get_function_users(M, StringRef("__kmpc_reduce"));
    for (auto *instruction : get_reduce_start)
    {
        handle_reduce_call(M, e, dyn_cast<CallInst>(instruction));
    }

    auto get_reduce_nowait_end = get_function_users(M, StringRef("__kmpc_end_reduce_nowait"));
    for (auto *instruction : get_reduce_nowait_end)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            inst->eraseFromParent();
            // delete it as it is not needed in mpi
        }
    }
    auto get_reduce_end = get_function_users(M, StringRef("__kmpc_end_reduce"));
    for (auto *instruction : get_reduce_end)
    {
        if (auto *inst = dyn_cast<Instruction>(instruction))
        {
            inst->eraseFromParent();
            // delete it as it is not needed in mpi
        }
    }
}

// convert critical pragmas
void parse_critical_pragma(Module &M, environment *e)
{
    IRBuilder<> builder(M.getContext());
    Constant *constant0 = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);

    auto get_critical_start = get_function_users(M, StringRef("__kmpc_critical"));

    std::set<Value *> critical_ident;

    for (auto *u : get_critical_start)
    {
        if (CallInst *begin_crit = dyn_cast<CallInst>(u))
        {
            critical_ident.insert(begin_crit->getArgOperand(2));
        }
    }

    for (auto *critical : critical_ident)
    {
        assert(isa<GlobalVariable>(critical) &&
               critical->getName().startswith(".gomp_critical_user_"));

        std::vector<Instruction *> to_remove;

        GlobalVariable *mutex = new GlobalVariable(
            M, e->types->mpi_mutex->getPointerTo(), false,
            GlobalVariable::LinkageTypes::CommonLinkage,
            ConstantPointerNull::get(e->types->mpi_mutex->getPointerTo()), "critical_mutex");
        // init mutex
        builder.SetInsertPoint(e->global->setup->getTerminator());
        builder.CreateCall(e->functions->MPI_Mutex_init, {mutex, constant0});
        // destroy
        builder.SetInsertPoint(e->global->wrapUp->getFirstNonPHI());
        auto *dereferenced = builder.CreateLoad(mutex);
        builder.CreateCall(e->functions->MPI_Mutex_destroy, {dereferenced});
        for (auto *u : critical->users())
        {
            if (auto *call = dyn_cast<CallInst>(u))
            {
                if (call->getCalledFunction()->getName().startswith("__kmpc_critical"))
                {
                    builder.SetInsertPoint(call);
                    auto *dereferenced = builder.CreateLoad(mutex);
                    builder.CreateCall(e->functions->MPI_Mutex_lock, {dereferenced});
                    to_remove.push_back(call);
                }
                else if (call->getCalledFunction()->getName().startswith(
                             "__kmpc_end_critical"))
                {
                    builder.SetInsertPoint(call);
                    auto *dereferenced = builder.CreateLoad(mutex);
                    builder.CreateCall(e->functions->MPI_Mutex_unlock, {dereferenced});
                    to_remove.push_back(call);
                }
                else
                {
                    errs() << "Unknown user of critical\n";
                    call->dump();
                }
            }
            else
            {
                errs() << "Unknown user of critical\n";
                u->dump();
            }
        }

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
        // we dont need the OpenMP global anymore
        dyn_cast<GlobalVariable>(critical)->eraseFromParent();
    } // end for each critical

    // assert that no more criticlas left
    assert(get_function_users(M, StringRef("__kmpc_critical")).empty());
    assert(get_function_users(M, StringRef("__kmpc_end_critical")).empty());
}

void replace_pragmas(Module &M, environment *e)
{
    replace_omp_functions(M, e);
    replace_barrier_pragma(M, e);
    parse_single_section_pragmas(M, e);
    parse_reduction(M, e);
    parse_critical_pragma(M, e);
}
