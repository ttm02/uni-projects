
#include "SharedArray.h"
#include "SharedVariable.h"
#include "comm_patterns/commPatterns.h"
#include "helper.h"

using namespace llvm;

llvm::Value *SharedArray::get_dimension(llvm::Module &M, environment *e)
{
    int depth = get_pointer_depth(var->getType()->getPointerElementType());
    return ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), depth);
}
llvm::Value *SharedArray::get_base_type_size(llvm::Module &M, environment *e)
{
    Type *elem_type = var->getType();
    for (int i = 0; i < get_pointer_depth(var->getType()); ++i)
    {
        elem_type = elem_type->getPointerElementType();
    }
    assert(!elem_type->isPointerTy());

    size_t size = get_size_in_Byte(M, elem_type);
    return ConstantInt::get(IntegerType::getInt64Ty(M.getContext()), size);
}

// constructors that derived should offer:
SharedArrayDistributed::SharedArrayDistributed(llvm::Value *val)
{
    this->Type_ID = SharedArrayDistributedID;
    this->var = val;
    this->parent_var = nullptr;
}

SharedArrayDistributed::SharedArrayDistributed(SharedVariable *original_val,
                                               llvm::Value *val) // to use when it is
                                                                 // passed to a function
{
    this->Type_ID = SharedArrayDistributedID;
    assert(isa<SharedArrayDistributed>(original_val));
    this->var = val;
    this->parent_var = original_val;
}

// do nothing destructor is already defineddefined in header
//~SharedArrayDistributed(){};

llvm::StructType *SharedArrayDistributed::get_comm_info_type(llvm::Module &M, environment *e)
{

    return get_default_comm_info_struct_type(M, e, var->getType()->getPointerElementType());
}

// which funcions should this variable use for communication
llvm::Constant *SharedArrayDistributed::get_comm_function_on_store(llvm::Module &M,
                                                                   environment *e)
{
    return e->functions->store_to_shared_array_line;
}
llvm::Constant *SharedArrayDistributed::get_comm_function_on_load(llvm::Module &M,
                                                                  environment *e)
{
    return e->functions->cache_shared_array_line;
}
// init and finish a parallel region
llvm::Constant *SharedArrayDistributed::get_comm_function_on_init(llvm::Module &M,
                                                                  environment *e)
{
    return e->functions->distribute_shared_array_from_master;
}
llvm::Constant *SharedArrayDistributed::get_comm_function_on_finish(llvm::Module &M,
                                                                    environment *e)
{
    return e->functions->free_distributed_shared_array_from_master;
}
// at sync_point (e.g. barrier or taskwait)
llvm::Constant *SharedArrayDistributed::get_comm_function_on_sync(llvm::Module &M,
                                                                  environment *e)
{
    return e->functions->invlaidate_shared_array_cache;
}

// mem aware distribution
// constructors that derived should offer:
SharedArrayDistributedMemoryAware::SharedArrayDistributedMemoryAware(llvm::Value *val)
    : SharedArrayDistributed(val)
{
    this->Type_ID = SharedArrayDistributedMemoryAwareID;
    // done in super
    //  this->var = val;
    //    this->parent_var = nullptr;
}

SharedArrayDistributedMemoryAware::SharedArrayDistributedMemoryAware(
    SharedVariable *original_val, llvm::Value *val)
    : SharedArrayDistributed(original_val, val)
{
    this->Type_ID = SharedArrayDistributedMemoryAwareID;
    assert(isa<SharedArrayDistributedMemoryAware>(original_val));

    // note that assertion of super will not break as mem aware is subclass
    // done in super
    // this->var = val;
    // this->parent_var = original_val;
}

llvm::Constant *SharedArrayDistributedMemoryAware::get_comm_function_on_sync(llvm::Module &M,
                                                                             environment *e)
{
    return e->functions->invlaidate_shared_array_cache_release_mem;
}

// BCASTED ARRAY:

// definition of static member
std::map<llvm::Type *, llvm::StructType *> SharedArrayBcasted::already_constructed_types;

llvm::StructType *SharedArrayBcasted::get_comm_info_type(llvm::Module &M, environment *e,
                                                         llvm::Type *local_buffer_type)
{
    if (already_constructed_types.find(local_buffer_type) == already_constructed_types.end())
    {
        // not present: insert new type into map
        // new type is implicitly renamed
        StructType *comm_info_struct = StructType::create(
            M.getContext(), {local_buffer_type, e->types->bcasted_array_info}, "comm_info_");
        already_constructed_types[local_buffer_type] = comm_info_struct;
    }

    return already_constructed_types[local_buffer_type];
}

// constructors that derived should offer:
SharedArrayBcasted::SharedArrayBcasted(llvm::Value *val)
{
    this->Type_ID = SharedArrayBcastedID;
    this->var = val;
    this->parent_var = nullptr;
}

SharedArrayBcasted::SharedArrayBcasted(SharedVariable *original_val,
                                       llvm::Value *val) // to use when it is
                                                         // passed to a function
{
    this->Type_ID = SharedArrayBcastedID;
    assert(isa<SharedArrayBcasted>(original_val));
    this->var = val;
    this->parent_var = original_val;
}

// do nothing destructor is already defineddefined in header
//~SharedArrayBcasted(){};

llvm::StructType *SharedArrayBcasted::get_comm_info_type(llvm::Module &M, environment *e)
{
    return get_comm_info_type(M, e, var->getType()->getPointerElementType());
}

// which funcions should this variable use for communication
llvm::Constant *SharedArrayBcasted::get_comm_function_on_store(llvm::Module &M, environment *e)
{
    return e->functions->store_to_bcasted_array_line;
}
llvm::Constant *SharedArrayBcasted::get_comm_function_on_load(llvm::Module &M, environment *e)
{
    return nullptr;
}
// init and finish a parallel region
llvm::Constant *SharedArrayBcasted::get_comm_function_on_init(llvm::Module &M, environment *e)
{
    return e->functions->bcast_array_from_master;
}
llvm::Constant *SharedArrayBcasted::get_comm_function_on_finish(llvm::Module &M,
                                                                environment *e)
{
    return e->functions->free_bcasted_array_from_master;
}
// at sync_point (e.g. barrier or taskwait)
llvm::Constant *SharedArrayBcasted::get_comm_function_on_sync(llvm::Module &M, environment *e)
{
    return nullptr;
}

// MASTER BASED ARRAY

// BCASTED ARRAY:

// definition of static member
std::map<llvm::Type *, llvm::StructType *> SharedArrayMasterBased::already_constructed_types;

llvm::StructType *SharedArrayMasterBased::get_comm_info_type(llvm::Module &M, environment *e,
                                                             llvm::Type *local_buffer_type)
{
    if (already_constructed_types.find(local_buffer_type) == already_constructed_types.end())
    {
        // not present: insert new type into map
        // new type is implicitly renamed
        StructType *comm_info_struct = StructType::create(
            M.getContext(), {local_buffer_type, e->types->master_based_array_info},
            "comm_info_");
        already_constructed_types[local_buffer_type] = comm_info_struct;
    }

    return already_constructed_types[local_buffer_type];
}

// constructors that derived should offer:
SharedArrayMasterBased::SharedArrayMasterBased(llvm::Value *val)
{
    this->Type_ID = SharedArrayMasterBasedID;
    this->var = val;
    this->parent_var = nullptr;
}

SharedArrayMasterBased::SharedArrayMasterBased(SharedVariable *original_val,
                                               llvm::Value *val) // to use when it is
                                                                 // passed to a function
{
    this->Type_ID = SharedArrayMasterBasedID;
    assert(isa<SharedArrayMasterBased>(original_val));
    this->var = val;
    this->parent_var = original_val;
}

// do nothing destructor is already defineddefined in header
//~SharedArrayBcasted(){};

llvm::StructType *SharedArrayMasterBased::get_comm_info_type(llvm::Module &M, environment *e)
{
    return get_comm_info_type(M, e, var->getType()->getPointerElementType());
}

// which funcions should this variable use for communication
llvm::Constant *SharedArrayMasterBased::get_comm_function_on_store(llvm::Module &M,
                                                                   environment *e)
{
    return e->functions->store_to_master_based_array_line;
}
llvm::Constant *SharedArrayMasterBased::get_comm_function_on_load(llvm::Module &M,
                                                                  environment *e)
{
    return e->functions->load_from_master_based_array_line;
}
// init and finish a parallel region
llvm::Constant *SharedArrayMasterBased::get_comm_function_on_init(llvm::Module &M,
                                                                  environment *e)
{
    return e->functions->init_master_based_array_info;
}
llvm::Constant *SharedArrayMasterBased::get_comm_function_on_finish(llvm::Module &M,
                                                                    environment *e)
{
    return e->functions->free_master_based_array;
}
// at sync_point (e.g. barrier or taskwait)
llvm::Constant *SharedArrayMasterBased::get_comm_function_on_sync(llvm::Module &M,
                                                                  environment *e)
{
    return e->functions->sync_master_based_array;
}
