#include <llvm/IR/IRBuilder.h>
//#include <llvm/Support/raw_ostream.h>

#include "../SharedSingleValue.h"
#include "../helper.h"
#include "commPatterns.h"
#include <llvm/Support/raw_ostream.h>
using namespace llvm;

#define DEBUG_DEFAULT_COMM_PATTERN 0

#if DEBUG_DEFAULT_COMM_PATTERN == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

// TOOD decreparted: (currently not used)
// Adds MPI communication to shared variables of the shared array type.
void default_static_array_var(Module &M, environment *e, ParallelFunction *microtask_obj,
                              Value *shared_array_var)
{
    Function *microtask = microtask_obj->get_function();
    // Holds all load instructions to shared variables in the micro task
    std::vector<Value *> shared_arrays_ptr;
    // Holds a pair of:
    //  first: elementptr instruction
    //  second: the pointer to the corresponding shared array
    std::vector<std::pair<GetElementPtrInst *, Value *>> shared_arrays_geps;
    // as the function is decreparted anyway just a hack that it should work:
    shared_arrays_ptr.push_back(shared_array_var);

    // Find all acceses to shared arrays
    // every GetElementPtr instruction on a shared array leads to an array
    // access
    for (auto &shared_array : shared_arrays_ptr)
    {
        for (auto *user : shared_array->users())
        {
            if (auto *gep = dyn_cast<GetElementPtrInst>(user))
            {
                // only care for usage within the current function
                // other function may use other commiunication patterns
                if (gep->getParent()->getParent() == microtask && is_comm_allowed(e, gep))
                {
                    auto pair = std::make_pair(gep, shared_array);
                    shared_arrays_geps.push_back(pair);
                }
            }
        }
    }

    // mapping the shared array to its mpi window
    std::map<Value *, AllocaInst *> shared_array_windows;

    // Find all return statements in microtask to add freeing of mpi windows
    // infront of them
    std::vector<ReturnInst *> function_exits = get_instruction_in_func<ReturnInst>(microtask);
    // Add calls to creation and freeing of mpi windows for every shared array
    for (auto &shared_array : shared_arrays_ptr)
    {
        IRBuilder<> builder(&*microtask->getEntryBlock().begin());
        LLVMContext &Ctx = M.getContext();

        Value *num_elements = builder.getInt32(
            shared_array->getType()->getPointerElementType()->getArrayNumElements());
        AllocaInst *mpi_win_alloca = builder.CreateAlloca(Type::getInt32Ty(Ctx));
        Value *gep = builder.CreateInBoundsGEP(shared_array,
                                               {builder.getInt32(0), builder.getInt32(0)});
        Value *gep_void_ptr = builder.CreateBitCast(gep, Type::getInt8PtrTy(Ctx));
        Value *mpi_type = builder.getInt32(get_mpi_datatype(shared_array));
        if (get_mpi_datatype(shared_array) == MPI_BYTE)
        {
            num_elements = builder.getInt32(
                shared_array->getType()->getPointerElementType()->getArrayNumElements() *
                get_size_in_Byte(M, shared_array));
        }

        std::vector<Value *> args1 = {gep_void_ptr, num_elements, mpi_win_alloca, mpi_type};

        builder.CreateCall(e->functions->mpi_create_shared_array_window, args1);

        auto pair = std::make_pair(shared_array, mpi_win_alloca);
        shared_array_windows.insert(pair);
        Value *rank = builder.CreateLoad(e->global->rank);

        std::vector<Value *> args2 = {gep_void_ptr, num_elements, rank, mpi_win_alloca,
                                      mpi_type};

        for (auto &ret : function_exits)
        {
            builder.SetInsertPoint(ret);
            builder.CreateCall(e->functions->mpi_free_shared_array_window, args2);
        }
    }

    // Add mpi communication to every access on a shared array
    for (auto &shared : shared_arrays_geps)
    {
        auto *shared_array_gep = shared.first;
        auto *shared_array_ptr = shared.second;

        std::pair<GetElementPtrInst *, StoreInst *> store_access = {nullptr, nullptr};
        std::pair<GetElementPtrInst *, LoadInst *> load_access = {nullptr, nullptr};

        for (auto *user : shared_array_gep->users())
        {
            if (auto *store = dyn_cast<StoreInst>(user))
            {
                auto pair = std::make_pair(shared_array_gep, store);
                store_access = pair;
            }
            else if (auto *load = dyn_cast<LoadInst>(user))
            {
                auto pair = std::make_pair(shared_array_gep, load);
                load_access = pair;
            }
        }

        IRBuilder<> builder(shared_array_gep->getNextNode());
        LLVMContext &Ctx = M.getContext();

        Value *rank = builder.CreateLoad(e->global->rank);
        Value *size = builder.CreateLoad(e->global->size);
        Value *num_elements = builder.getInt32(
            shared_array_ptr->getType()->getPointerElementType()->getArrayNumElements());
        Value *mpi_win_alloca = shared_array_windows.at(shared_array_ptr);
        Value *mpi_type = builder.getInt32(get_mpi_datatype(shared_array_ptr));
        if (get_mpi_datatype(shared_array_ptr) == MPI_BYTE)
        {
            num_elements = builder.getInt32(
                shared_array_ptr->getType()->getPointerElementType()->getArrayNumElements() *
                get_size_in_Byte(M, shared_array_ptr));
        }
        Value *gep_void_ptr = builder.CreateBitCast(shared_array_gep, Type::getInt8PtrTy(Ctx));
        Value *displacement = shared_array_gep->getOperand(2);
        if (displacement->getType() != builder.getInt32Ty())
        {
            displacement = builder.CreateIntCast(displacement, builder.getInt32Ty(), false);
            // displacement should not be signed
        }

        // Add mpi communication for stores to shared array
        if (store_access.second != nullptr)
        {
            std::vector<Value *> args = {gep_void_ptr, displacement, rank, mpi_win_alloca,
                                         mpi_type};

            builder.SetInsertPoint(store_access.second->getNextNode());
            builder.CreateCall(e->functions->mpi_store_in_shared_array, args);
        }
        if (load_access.second != nullptr)
        {
            std::vector<Value *> args = {gep_void_ptr, rank, displacement, mpi_win_alloca,
                                         mpi_type};

            builder.SetInsertPoint(load_access.second);
            builder.CreateCall(e->functions->mpi_load_from_shared_array, args);
        }
    }
}

// Kann alle Kommunikationspattern verarbeiten, die bei load, store und sync (barrier) als
// Parameter den pointer zum comm_info_struct und die größe des Lokanen buffers (erster teil
// des comm_ifo structs) annehmen

// Adds MPI communication to a shared variable of the single value type.
void default_single_value_var(Module &M, environment *e, ParallelFunction *microtask_obj,
                              SharedSingleValue *shared_var_obj)
{
    IRBuilder<> builder(M.getContext());

    Value *shared_var = shared_var_obj->value();
    Value *type_size = shared_var_obj->get_local_buffer_size(M, e);

    assert(!shared_var->getType()->getPointerElementType()->isPointerTy());

    builder.SetInsertPoint(
        get_init_block(M, e, microtask_obj->get_function())->getTerminator());
    Value *comm_info_void = builder.CreateBitCast(shared_var, builder.getInt8PtrTy());

    for (auto *u : shared_var->users())
    {
        auto *inst = dyn_cast<Instruction>(u);
        if (u != comm_info_void && is_comm_allowed(e, inst) &&
            inst->getFunction() == microtask_obj->get_function())
        {

            if (auto *store = dyn_cast<StoreInst>(u))
            {
                if (shared_var_obj->get_comm_function_on_store(M, e))
                {
                    // insert AFTER the store
                    builder.SetInsertPoint(store->getNextNode());
                    builder.CreateCall(shared_var_obj->get_comm_function_on_store(M, e),
                                       {comm_info_void, type_size});
                }
            }
            else if (auto *load = dyn_cast<LoadInst>(u))
            {
                if (shared_var_obj->get_comm_function_on_load(M, e))
                {
                    builder.SetInsertPoint(load);
                    builder.CreateCall(shared_var_obj->get_comm_function_on_load(M, e),
                                       {comm_info_void, type_size});
                }
            }
            else
            {
                errs() << "This operation on a shared single value var is not supported "
                          "yet:\n";
                u->dump();
            }
        }
    }

    // on sync_Point:
    auto barriers = get_function_users(M, e->functions->mpi_barrier->getName());
    for (auto *u : barriers)
    {
        auto *inst = dyn_cast<Instruction>(u);
        if (inst->getFunction() == microtask_obj->get_function())
        {
            if (shared_var_obj->get_comm_function_on_sync(M, e))
            {
                builder.SetInsertPoint(inst);
                builder.CreateCall(shared_var_obj->get_comm_function_on_sync(M, e),
                                   {comm_info_void, type_size});
            }
        }
    }
}

// returns pair: new Master arg, new worker arg
std::pair<llvm::Value *, llvm::Value *> bcast_shared_struct_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, llvm::Value *arg_to_bcast)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    assert(arg_to_bcast->getType()->isPointerTy() &&
           arg_to_bcast->getType()->getPointerElementType()->isStructTy());

    StructType *comm_info_struct_type = get_default_comm_info_struct_type(
        M, e, arg_to_bcast->getType()->getPointerElementType());

    Value *worker_var_ptr = worker_builder.CreateAlloca(comm_info_struct_type);
    // as struct is already present on master he will not alloc new mem

    Value *master_void = master_builder.CreateBitCast(arg_to_bcast, void_ptr_type);
    Value *worker_void = worker_builder.CreateBitCast(worker_var_ptr, void_ptr_type);
    Value *worker_orig_type =
        worker_builder.CreateBitCast(worker_var_ptr, arg_to_bcast->getType());

    Value *size = master_builder.getInt32(get_size_in_Byte(M, comm_info_struct_type));
    Value *mpi_type = master_builder.getInt32(MPI_BYTE);
    Value *root = master_builder.getInt32(0);
    master_builder.CreateCall(e->functions->mpi_bcast, {master_void, size, mpi_type, root});
    worker_builder.CreateCall(e->functions->mpi_bcast, {worker_void, size, mpi_type, root});

    return std::make_pair(arg_to_bcast, worker_orig_type);
}

// returns pair: new Master arg, new worker arg
std::pair<llvm::Value *, llvm::Value *> bcast_shared_single_value_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, SharedSingleValue *arg_to_bcast)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    assert(arg_to_bcast->getType()->isPointerTy() &&
           !arg_to_bcast->getType()->getPointerElementType()->isPointerTy() &&
           !arg_to_bcast->getType()->getPointerElementType()->isArrayTy() &&
           !arg_to_bcast->getType()->getPointerElementType()->isStructTy());

    StructType *comm_info_struct_type = arg_to_bcast->get_comm_info_type(M, e);

    // no struct type: init comm info
    Value *type_size = arg_to_bcast->get_local_buffer_size(M, e);

    Value *master_var_ptr = master_builder.CreateAlloca(comm_info_struct_type);
    Value *worker_var_ptr = worker_builder.CreateAlloca(comm_info_struct_type);

    Value *master_void = master_builder.CreateBitCast(master_var_ptr, void_ptr_type);
    Value *worker_void = worker_builder.CreateBitCast(worker_var_ptr, void_ptr_type);
    Value *master_orig_type =
        master_builder.CreateBitCast(master_var_ptr, arg_to_bcast->getType());
    Value *worker_orig_type =
        worker_builder.CreateBitCast(worker_var_ptr, arg_to_bcast->getType());

    // master need to load initial value
    // TODO or search for origin of master value and replace its allocation with the
    // comm_info_struct (as this struct may be used as the variable without danger)
    Value *begin_value = master_builder.CreateLoad(arg_to_bcast->value());
    master_builder.CreateStore(begin_value, master_orig_type);

    // the actual "bcast" (init of communication info)
    if (arg_to_bcast->get_comm_function_on_init(M, e) != nullptr)
    {
        master_builder.CreateCall(arg_to_bcast->get_comm_function_on_init(M, e),
                                  {master_void, type_size});
        worker_builder.CreateCall(arg_to_bcast->get_comm_function_on_init(M, e),
                                  {worker_void, type_size});
    }
    // free
    master_builder.SetInsertPoint(insert_before_master->getNextNode());
    worker_builder.SetInsertPoint(insert_before_worker->getNextNode());

    if (arg_to_bcast->get_comm_function_on_finish(M, e) != nullptr)
    {
        master_builder.CreateCall(arg_to_bcast->get_comm_function_on_finish(M, e),
                                  {master_void, type_size});
        worker_builder.CreateCall(arg_to_bcast->get_comm_function_on_finish(M, e),
                                  {worker_void, type_size});
    }
    // master: need to copy the current value to original var
    // the free function should end all communication and result in the correct value
    // present in the local buffer on the master
    Value *final_value = master_builder.CreateLoad(master_orig_type);
    master_builder.CreateStore(final_value, arg_to_bcast->value());

    return std::make_pair(master_orig_type, worker_orig_type);
}

// if firstprivate value is a ptr, it is shared anyway as location of ptr is shared, so it will
// be hanled by the other cases!
std::pair<llvm::Value *, llvm::Value *> bcast_firstprivate_single_value_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, llvm::Value *arg_to_bcast)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    assert(!arg_to_bcast->getType()->isPointerTy());

    Value *var_ptr = nullptr;
    if (auto *load = dyn_cast<LoadInst>(arg_to_bcast))
    {
        var_ptr = load->getPointerOperand();
    }
    else
    {
        // result of calculation:
        // master need to alloc a buffer for bcast
        var_ptr = master_builder.CreateAlloca(arg_to_bcast->getType());
        master_builder.CreateStore(arg_to_bcast, var_ptr);
    }

    assert(var_ptr != nullptr);
    // we need to insert a load for worker after bcast
    Value *worker_var_ptr =
        worker_builder.CreateAlloca(var_ptr->getType()->getPointerElementType());
    Value *worker_var = worker_builder.CreateLoad(worker_var_ptr);
    worker_builder.SetInsertPoint(dyn_cast<Instruction>(worker_var));
    // so bcast will be inserted before load

    Value *buffer = master_builder.CreateBitCast(var_ptr, void_ptr_type);
    Value *count = master_builder.getInt32(1);
    Value *mpi_type = master_builder.getInt32(get_mpi_datatype(var_ptr));
    if (get_mpi_datatype(var_ptr) == MPI_BYTE)
    {
        count = master_builder.getInt32(1 * get_size_in_Byte(M, var_ptr));
    }
    Value *root = master_builder.getInt32(0);
    std::vector<Value *> args = {buffer, count, mpi_type, root};
    master_builder.CreateCall(e->functions->mpi_bcast, args);
    // on worker:
    Value *buffer_w_void = worker_builder.CreateBitCast(worker_var_ptr, void_ptr_type);
    std::vector<Value *> args_worker = {buffer_w_void, count, mpi_type, root};
    worker_builder.CreateCall(e->functions->mpi_bcast, args_worker);

    return std::make_pair(arg_to_bcast, worker_var);
}

std::pair<llvm::Value *, llvm::Value *> bcast_shared_static_array_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, llvm::Value *arg_to_bcast)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    assert(arg_to_bcast->getType()->getPointerElementType()->isArrayTy());
    ArrayType *array_type =
        dyn_cast<ArrayType>(arg_to_bcast->getType()->getPointerElementType());

    long size = array_type->getNumElements();
    Value *mpi_type = worker_builder.getInt32(get_mpi_datatype(array_type->getElementType()));
    if (get_mpi_datatype(array_type->getElementType()) == MPI_BYTE)
    {
        size = size * get_size_in_Byte(M, array_type->getElementType());
    }
    Value *count = worker_builder.getInt32(size);
    Value *root = worker_builder.getInt32(0);
    Value *buffer_m_to_void = master_builder.CreateBitCast(arg_to_bcast, void_ptr_type);

    Value *worker_var =
        worker_builder.CreateAlloca(arg_to_bcast->getType()->getPointerElementType());
    Value *buffer_w_to_void = worker_builder.CreateBitCast(worker_var, void_ptr_type);

    std::vector<Value *> master_args = {buffer_m_to_void, count, mpi_type, root};
    std::vector<Value *> worker_args = {buffer_w_to_void, count, mpi_type, root};
    master_builder.CreateCall(e->functions->mpi_bcast, master_args);
    worker_builder.CreateCall(e->functions->mpi_bcast, worker_args);

    return std::make_pair(arg_to_bcast, worker_var);
}

void bcast_all_globals(llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
                       llvm::CallInst *insert_before_worker)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    // bcast all globals
    for (auto global : e->global->shared_globals)
    {
        if (auto *single = dyn_cast<SharedSingleValue>(global))
        {
            Value *global_val = single->value();
            Value *buffer_size = single->get_local_buffer_size(M, e);

            master_builder.SetInsertPoint(insert_before_master);
            worker_builder.SetInsertPoint(insert_before_worker);

            Value *master_void = master_builder.CreateBitCast(global_val, void_ptr_type);
            Value *worker_void = worker_builder.CreateBitCast(global_val, void_ptr_type);

            master_builder.CreateCall(global->get_comm_function_on_init(M, e),
                                      {master_void, buffer_size});
            worker_builder.CreateCall(global->get_comm_function_on_init(M, e),
                                      {worker_void, buffer_size});

            master_builder.SetInsertPoint(insert_before_master->getNextNode());
            worker_builder.SetInsertPoint(insert_before_worker->getNextNode());

            master_builder.CreateCall(global->get_comm_function_on_finish(M, e),
                                      {master_void, buffer_size});
            worker_builder.CreateCall(global->get_comm_function_on_finish(M, e),
                                      {worker_void, buffer_size});
        }
        else if (auto *array = dyn_cast<SharedArray>(global))
        {
            master_builder.SetInsertPoint(insert_before_master);
            worker_builder.SetInsertPoint(insert_before_worker);

            Value *global_val = array->value();
            Value *buffer_size = array->get_local_buffer_size(M, e);
            Value *dim = array->get_dimension(M, e);
            Value *elem_size = array->get_base_type_size(M, e);

            Value *master_void = master_builder.CreateBitCast(global_val, void_ptr_type);
            Value *worker_void = worker_builder.CreateBitCast(global_val, void_ptr_type);

            master_builder.CreateCall(array->get_comm_function_on_init(M, e),
                                      {master_void, buffer_size, dim, elem_size});
            worker_builder.CreateCall(array->get_comm_function_on_init(M, e),
                                      {worker_void, buffer_size, dim, elem_size});

            master_builder.SetInsertPoint(insert_before_master->getNextNode());
            worker_builder.SetInsertPoint(insert_before_worker->getNextNode());

            master_builder.CreateCall(array->get_comm_function_on_finish(M, e),
                                      {master_void, buffer_size});
            worker_builder.CreateCall(array->get_comm_function_on_finish(M, e),
                                      {worker_void, buffer_size});
        }
    }
}

llvm::StructType *get_default_comm_info_struct_type(llvm::Module &M, environment *e,
                                                    llvm::Type *base_type)
{
    if (e->default_comm_info_map.find(base_type) == e->default_comm_info_map.end())
    {
        // not present: insert new type into map
        StructType *comm_info_part = nullptr;
        // get the right comm_info_part
        if (base_type->isPointerTy())
        {
            comm_info_part = e->types->array_distribution_info;
        }
        else if (base_type->isArrayTy())
        {
            errs() << "Static array not supported yet\n";
        }
        else if (base_type->isStructTy())
        {
            StructType *changed_struct =
                e->types->struct_old_new_map[dyn_cast<StructType>(base_type)];
            assert(changed_struct != nullptr);
            // struct types:
            // first is the struct itself
            // second the mpi struct ptr
            // third the changed struct that contains mpi struct ptr for member pointers
            StructType *comm_info_struct = StructType::create(
                M.getContext(), {base_type, e->types->mpi_struct_ptr, changed_struct},
                "comm_info_");
            e->default_comm_info_map[base_type] = comm_info_struct;
            return comm_info_struct;
        }
        else
        {
            comm_info_part = e->types->single_value_info;
        }
        assert(comm_info_part != nullptr);
        // new type is implicitly renamed
        StructType *comm_info_struct =
            StructType::create(M.getContext(), {base_type, comm_info_part}, "comm_info_");
        e->default_comm_info_map[base_type] = comm_info_struct;
    }
    // return type from map
    return e->default_comm_info_map[base_type];
}
