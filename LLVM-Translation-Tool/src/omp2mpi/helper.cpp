#include "helper.h"
#include "environment.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
// Debug Macro for easy switching on/off of debug prints
#define DEBUG_HELPER 0

#if DEBUG_HELPER == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

// contains various helper functions

// Returns a vector of all Users of the named function
// Can be used to find all instances where the named function is called in the
// IR Module M
std::vector<User *> get_function_users(Module &M, StringRef name)
{
    std::vector<User *> func_users;
    if (llvm::Function *func = M.getFunction(name))
    {
        for (auto *user : func->users())
        {
            func_users.push_back(user);
        }
    }
    return func_users;
}

// gives the ParallelEntryFunction = Parallel region the instruction resides in (or nullptr if
// sequential)
ForkEntry *get_ForkEntry(Module &M, environment *e, Function *func)
{
    for (auto *parallel_region : e->microtask_functions)
    {
        if (func == parallel_region->get_function())
        {
            return parallel_region;
        }
        // search in called functions
        for (auto *calledF : parallel_region->get_all_functions_called())
        {
            if (calledF->get_function() == func)
                return parallel_region;
        }
    }
    // not found
    return nullptr;
}
ForkEntry *get_ForkEntry(Module &M, environment *e, Instruction *inst)
{
    return get_ForkEntry(M, e, inst->getFunction());
}

// will give the SharedVariable object that is top_level for the Value var
// (use it) in order to create a new "child" of this var
SharedVariable *get_top_level_var(llvm::Module &M, environment *e, llvm::Value *var)
{

    if (isa<Argument>(var))
    {
        // TODO then we have to search one lvl higher
        errs() << "Could not find top level of variable\n";
        var->dump();
        errs() << "This means that any annotations might be lost (nothing more will break so "
                  "far)\n";
        return nullptr;
    }
    else if (isa<GlobalValue>(var))
    {
        // search in globals
        for (auto *canidate : e->global->shared_globals)
        {
            if (canidate->value() == var)
            {
                return canidate;
            }
        }
        errs()
            << "Could not find top level of variable this should never Happen form a global\n";
        var->dump();
        return nullptr;
    }
    else
    {
        // seach in already present top lvl
        for (auto *canidate : e->top_lvl_vars)
        {
            if (canidate->value() == var)
            {
                return canidate;
            }
        }
        // not fount: instantiate a new one
        // therefore here the annotation will be analyzed
        auto *new_one = SharedVariable::Create(M, e, var);
        e->top_lvl_vars.push_back(new_one);
        return new_one;
    }
}

// Determines the length of an dynamically allocated array that has been
// passed through the IR code as a pointer type without the information about
// its length.
int get_array_length(Module &M, Value *array)
{
    StringRef shared_array_name = array->getName();
    auto znam_users = get_function_users(M, StringRef("_Znam"));

    for (auto &znam : znam_users)
    {
        for (auto *user : znam->users())
        {
            if (auto *bitcast = dyn_cast<BitCastInst>(user))
            {
                for (auto *user2 : bitcast->users())
                {
                    if (auto *store = dyn_cast<StoreInst>(user2))
                    {
                        if (store->getPointerOperand()->getName().equals(shared_array_name))
                        {
                            auto *znam_call = dyn_cast<CallInst>(znam);
                            auto *length = dyn_cast<ConstantInt>(znam_call->getArgOperand(0));

                            Type *type = array->getType()
                                             ->getPointerElementType()
                                             ->getPointerElementType();
                            DataLayout DL = M.getDataLayout();

                            return length->getSExtValue() / DL.getTypeAllocSize(type);
                        }
                    }
                }
            }
        }
    }
    errs() << "Error in determining the array size\n";
    return 0;
}
// determines if struct is user defined and not inserted by openmp or our pass
bool is_struct_user_defined(Module &M, environment *e, StructType *ty)
{
    bool result = ty != nullptr &&
                  !ty->getName().startswith("struct.OMP_linked_list<") && // linked_list.h
                  !ty->getName().startswith("struct.MPI_linked_list<") && // linked_list.h
                  !ty->getName().startswith("class.MPI_linked_list") &&   // linked_list.h
                  !ty->getName().startswith("class.OMP_linked_list") &&   // linked_list.h
                  !ty->getName().startswith("class.Linked_list") &&       // linked_list.h
                  !ty->getName().startswith("struct.ident_t") &&          // openmp
                  !ty->getName().startswith("struct.kmp_task_t") &&       // openmp
                  !ty->getName().startswith("struct.anon") &&             // openmp
                  !ty->getName().startswith("struct..kmp_privates") &&    // openmp
                  !ty->getName().startswith("union.kmp_cmplrdata_t") &&   // openmp
                  !ty->getName().startswith("struct.global_tasking_info_struct") && // tasklib
                  !ty->getName().startswith("class.task_info_list") &&              // tasklib
                  !ty->getName().startswith("struct.task_info_list") &&             // tasklib
                  !ty->getName().startswith("struct.task_param_list_struct") &&     // tasklib
                  !ty->getName().startswith("struct.memory_allocation_info");       // rtlib
    if (e->types != nullptr &&
        ty != nullptr) // if environment was not fully initialized yet (this means type from
                       // rtlib where not loaded yet and must be user defined)
    {
        result = result && ty != e->types->mpi_mutex && ty != e->types->mpi_struct_ptr &&
                 ty != e->types->task_info_struct && ty != e->types->array_distribution_info;
    }

    // note: class.std::map is inserded by rtlib but may be used by user so is not included
    // here

    return result;
}

bool is_global_defined_by_openmp(llvm::GlobalVariable *var)
{
    bool result = false;

    if (var->getType()->getPointerElementType()->isStructTy())
    {
        result = result || dyn_cast<StructType>(var->getType()->getPointerElementType())
                               ->getName()
                               .startswith("struct.ident_t");
        // inserted every time
    }

    result =
        result || var->getName().startswith(".gomp_critical_user_"); // for critical pragma

    return result;
}
// determines if global variable is defined by our pass
// BEWARE: global constants such as string literals from the pass are not included
bool is_global_defined_by_pass(llvm::Module &M, environment *e, llvm::GlobalVariable *var)
{
    if (var == nullptr)
    {
        return false;
    }
    bool result = false;

    // so that this function may be called when environment was not fully set up:
    if (e->global != nullptr)
    {
        for (auto mpi_win_pair : e->global->mpi_win)
        {
            result = result || var == mpi_win_pair.second;
        }
        result = result || var == e->global->struct_ptr_win;
        result = result || var == e->global->task_param_win;
        result = result || var == e->global->rank;
        result = result || var == e->global->size;
    }
    // if types are not declared, no golbal can be of that type anyway
    if (e->types != nullptr)
    {
        if (var->getType()->getPointerElementType()->isStructTy())
        {
            result = result ||
                     !is_struct_user_defined(
                         M, e, dyn_cast<StructType>(var->getType()->getPointerElementType()));
        }

        // inserted for critical pragmas
        result = result || (var->getName().startswith("critical_mutex") &&
                            var->getType()->getPointerElementType() ==
                                e->types->mpi_mutex->getPointerTo());
    }
    // globals, die von rtlib oder tasklib geladen werden mit inkludieren:
    result = result || var->getName().startswith("alloc_info");    // from rtlib
    result = result || var->getName().startswith("rank_and_size"); // from rtlib
    result = result || var->getName().startswith("tasking_info");  // from tasklib
    result = result || var->getName().startswith(
                           "_ZZ14MPI_Mutex_initPP9MPI_MutexiE3tag"); // from mpi_mutex
    return result;
}

// Returns the right MPI_Datatype for the Value
int get_mpi_datatype(Type *type)
{
    if (type->isIntegerTy(32))
    {
        return MPI_INT;
    }
    else if (type->isIntegerTy(64))
    {
        return MPI_LONG_LONG;
    }
    else if (type->isFloatTy())
    {
        return MPI_FLOAT;
    }
    else if (type->isDoubleTy())
    {
        return MPI_DOUBLE;
    }
    else if (type->isIntegerTy(8))
    {
        return MPI_CHAR;
    }
    Debug(errs() << "Type "; type->dump();
          errs() << " of var " << value->getName() << "not known\n";)
        // if MPI_BYTE Is returned, caller should use get_size_in_Bytes to determine the size
        // of buffer
        return MPI_BYTE;
}

int get_mpi_datatype(Value *value)
{
    Type *type = nullptr;

    // If it is a shared array
    if (value->getType()->getPointerElementType()->isArrayTy())
    {
        type = value->getType()->getPointerElementType()->getArrayElementType();
    }
    // If it is a pointer with depth > 1
    else if (value->getType()->getPointerElementType()->isPointerTy())
    {
        type = value->getType()->getPointerElementType();
        while (type->isPointerTy())
        {
            type = type->getPointerElementType();
        }
    }
    // If it is a shared single value
    else
    {
        type = value->getType()->getPointerElementType();
    }
    return get_mpi_datatype(type);
}

size_t get_size_in_Byte(llvm::Module &M, llvm::Value *value)
{
    Type *type = nullptr;

    // If it is a shared array
    if (value->getType()->getPointerElementType()->isArrayTy())
    {
        type = value->getType()->getPointerElementType()->getArrayElementType();
    }
    // If it is a pointer with depth > 1
    else if (value->getType()->getPointerElementType()->isPointerTy())
    {
        type = value->getType()->getPointerElementType();
        while (type->isPointerTy())
        {
            type = type->getPointerElementType();
        }
    }
    // If it is a shared single value
    else
    {
        type = value->getType()->getPointerElementType();
    }
    return get_size_in_Byte(M, type);
}

size_t get_size_in_Byte(llvm::Module &M, llvm::Type *type)
{
    DataLayout *TD = new DataLayout(&M);
    return TD->getTypeAllocSize(type);
}

// checks if someone has declared that no further MPI communicatioin should be used for
// this instruction
bool is_comm_allowed(environment *e, llvm::Instruction *instruction)
{
    BasicBlock *block = instruction->getParent();
    return !(e->no_further_comm.find(block) != e->no_further_comm.end());
}

// checks if given instruction resides within a omp critical region
bool is_in_omp_critical(Instruction *instruction)
{
    // traverse all instructions until found a call to
    // __kmpc_end_critical (then true)
    // or return instruction (then false)

    bool in_critical = false;

    Function *func = instruction->getFunction();

    Function *begin_critical = func->getParent()->getFunction("__kmpc_critical");
    Function *end_critical = func->getParent()->getFunction("__kmpc_end_critical");

    if (begin_critical == NULL || end_critical == NULL)
    {
        Debug(errs() << "no critical at all \n";) return false;
    }
    // there are no critical regions used

    for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    {
        Instruction *i = &*I;
        if (i == instruction)
        {
            Debug(instruction->dump();
                  errs() << "in function " << func->getName() << " is IN critical\n");
            // given instruction found
            return in_critical;
        }
        if (CallInst *call = dyn_cast<CallInst>(i))
        {
            // keep track of critical regions
            if (call->getCalledFunction() == begin_critical)
            {
                in_critical = true;
            }
            if (call->getCalledFunction() == end_critical)
            {
                in_critical = false;
            }
        }
    }

    errs() << "WARNING: should never reach this";
    return false; // should never reach this
}

llvm::BasicBlock *get_init_block(llvm::Module &M, environment *e, llvm::Function *F)
{
    return build_init_and_wrapUp_block(M, e, F).first;
}

llvm::BasicBlock *get_wrapUp_block(llvm::Module &M, environment *e, llvm::Function *F)
{
    return build_init_and_wrapUp_block(M, e, F).second;
}

std::pair<llvm::BasicBlock *, llvm::BasicBlock *>
build_init_and_wrapUp_block(llvm::Module &M, environment *e, llvm::Function *F)
{
    if (e->init_and_finish_block_map.find(F) != e->init_and_finish_block_map.end())
    {
        return e->init_and_finish_block_map[F];
    }
    // else build it new

    BasicBlock *setup = &F->getEntryBlock();
    // split up at first possible point:
    SplitBlock(setup, setup->getFirstNonPHI());

    IRBuilder<> builder(F->getContext());

    bool isVoid = F->getReturnType()->isVoidTy();
    Value *return_code_buffer = nullptr;

    if (!isVoid)
    {
        builder.SetInsertPoint(setup->getTerminator());
        return_code_buffer = builder.CreateAlloca(F->getReturnType());
    }

    BasicBlock *wrapUp = BasicBlock::Create(F->getContext(), "wrap_up_Mpi", F);
    // enter it before every exit
    auto exits = get_instruction_in_func<ReturnInst>(F);
    for (auto &ret : exits)
    {
        builder.SetInsertPoint(ret);
        if (!isVoid)
        {
            builder.CreateStore(ret->getReturnValue(), return_code_buffer);
        }
        builder.CreateBr(wrapUp);
        ret->eraseFromParent();
    }

    // need to fill the wrap up after all returns are now branches to it
    // otherwise it will enter a branch to itself;-)
    builder.SetInsertPoint(wrapUp);
    if (isVoid)
    {

        builder.CreateRetVoid();
    }
    else
    {
        Value *ret_value = builder.CreateLoad(return_code_buffer);
        builder.CreateRet(ret_value);
    }

    // insert it to the map
    auto result = std::make_pair(setup, wrapUp);
    e->init_and_finish_block_map[F] = result;
    return result;
}

void init_global_mpi_win(llvm::Module &M, environment *e, llvm::GlobalVariable *var)
{
    if (var->getType()->getPointerElementType()->isPointerTy())
    {
        errs() << "Pointers are currently not supported because they can point to different "
                  "storage areas during runtime and then the mpi win has to change\n";
        return;
    }
    else
    { // no pointer type
        auto iter = e->global->mpi_win.find(var);
        // only if not set in map
        if (iter == e->global->mpi_win.end())
        {
            IRBuilder<> builder(e->global->setup->getTerminator());
            LLVMContext &Ctx = M.getContext();
            // void mpi_create_shared_array_window(void *array, int array_size, MPI_Win *win,
            // int datatype)
            Value *num_elements = builder.getInt32(1);
            GlobalVariable *mpi_win = new GlobalVariable(
                M, Type::getInt32Ty(Ctx), false, GlobalValue::LinkageTypes::CommonLinkage,
                ConstantInt::get(Type::getInt32Ty(Ctx), 0), "mpi_win");
            Value *ptr = var; // globals are always used as ptr
            Value *void_ptr = builder.CreateBitCast(ptr, Type::getInt8PtrTy(Ctx));
            Value *mpi_type = builder.getInt32(get_mpi_datatype(var));

            std::vector<Value *> args1 = {void_ptr, num_elements, mpi_win, mpi_type};

            builder.CreateCall(e->functions->mpi_create_shared_array_window, args1);

            Value *rank = builder.CreateLoad(e->global->rank);
            std::vector<Value *> args2 = {void_ptr, num_elements, rank, mpi_win, mpi_type};
            builder.SetInsertPoint(e->global->wrapUp->getFirstNonPHI());
            builder.CreateCall(e->functions->mpi_free_shared_array_window, args2);

            e->global->mpi_win.insert(std::pair<GlobalVariable *, Value *>(var, mpi_win));
        }
    }
}

int get_pointer_depth(Type *type)
{
    int depth = 0;

    while (type->isPointerTy())
    {
        depth++;
        type = type->getPointerElementType();
    }

    return depth;
}

// adds all operations using the supplied operation to the given vector
// adds all usages uf the usages recursively
void add_remove_all_uses(std::vector<llvm::Instruction *> *to_remove, llvm::Instruction *inst,
                         bool only_remove_store_val)
{
    for (auto *u : inst->users())
    {
        if (auto *store = dyn_cast<StoreInst>(u))
        {
            if (!only_remove_store_val ||
                (only_remove_store_val && inst == store->getValueOperand()))
            {
                // insert store instruction at beginning, so that it is ensured to be removed
                // before value and ptr operand
                to_remove->insert(to_remove->begin(), dyn_cast<Instruction>(u));
            }
        }
        else
        {
            add_remove_all_uses(to_remove, dyn_cast<Instruction>(u), only_remove_store_val);
            to_remove->push_back(dyn_cast<Instruction>(u));
        }
    }
}
