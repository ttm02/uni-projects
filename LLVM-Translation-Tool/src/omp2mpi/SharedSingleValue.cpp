
#include "SharedSingleValue.h"
#include "SharedVariable.h"
#include "comm_patterns/commPatterns.h"
#include "helper.h"

using namespace llvm;

// constructors that derived should offer:
SharedSingleValueDefault::SharedSingleValueDefault(llvm::Value *val)
{
    this->Type_ID = SharedSingleValueDefaultID;
    this->var = val;
    this->parent_var = nullptr;
}
SharedSingleValueDefault::SharedSingleValueDefault(SharedVariable *original_val,
                                                   llvm::Value *val) // to use when it is
                                                                     // passed to a function
{
    this->Type_ID = SharedSingleValueDefaultID;
    assert(isa<SharedSingleValueDefault>(original_val));
    this->var = val;
    this->parent_var = original_val;
}

// do nothing destructor is already defineddefined in header
//~SharedArrayDistributed(){};

llvm::StructType *SharedSingleValueDefault::get_comm_info_type(llvm::Module &M, environment *e)
{
    return get_default_comm_info_struct_type(M, e, var->getType()->getPointerElementType());
}

// which funcions should this variable use for communication
llvm::Constant *SharedSingleValueDefault::get_comm_function_on_store(llvm::Module &M,
                                                                     environment *e)
{
    return e->functions->mpi_store_single_value_var;
}
llvm::Constant *SharedSingleValueDefault::get_comm_function_on_load(llvm::Module &M,
                                                                    environment *e)
{
    return e->functions->mpi_load_single_value_var;
}
// init and finish a parallel region
llvm::Constant *SharedSingleValueDefault::get_comm_function_on_init(llvm::Module &M,
                                                                    environment *e)
{
    return e->functions->init_single_value_comm_info;
}
llvm::Constant *SharedSingleValueDefault::get_comm_function_on_finish(llvm::Module &M,
                                                                      environment *e)
{
    return e->functions->free_single_value_comm_info;
}
// at sync_point (e.g. barrier or taskwait)
llvm::Constant *SharedSingleValueDefault::get_comm_function_on_sync(llvm::Module &M,
                                                                    environment *e)
{
    // no need to to something special at sync-Point
    return nullptr;
}

// shared reading comm pattern

// constructors that derived should offer:
SharedSingleValueReading::SharedSingleValueReading(llvm::Value *val)
{
    this->Type_ID = SharedSingleValueReadingID;
    this->var = val;
    this->parent_var = nullptr;
}
SharedSingleValueReading::SharedSingleValueReading(SharedVariable *original_val,
                                                   llvm::Value *val) // to use when it is
                                                                     // passed to a function
{
    this->Type_ID = SharedSingleValueReadingID;
    assert(isa<SharedSingleValueReading>(original_val));
    this->var = val;
    this->parent_var = original_val;
}

// do nothing destructor is already defineddefined in header
//~SharedSingleValueReading(){};

llvm::StructType *SharedSingleValueReading::get_comm_info_type(llvm::Module &M, environment *e)
{
    return get_default_comm_info_struct_type(M, e, var->getType()->getPointerElementType());
}

// which funcions should this variable use for communication
llvm::Constant *SharedSingleValueReading::get_comm_function_on_store(llvm::Module &M,
                                                                     environment *e)
{
    return e->functions->mpi_store_single_value_var_reading;
}
llvm::Constant *SharedSingleValueReading::get_comm_function_on_load(llvm::Module &M,
                                                                    environment *e)
{
    return nullptr;
}
// init and finish a parallel region
llvm::Constant *SharedSingleValueReading::get_comm_function_on_init(llvm::Module &M,
                                                                    environment *e)
{
    return e->functions->init_single_value_comm_info_shared_reading;
}
llvm::Constant *SharedSingleValueReading::get_comm_function_on_finish(llvm::Module &M,
                                                                      environment *e)
{
    return e->functions->free_single_value_comm_info;
}
// at sync_point (e.g. barrier or taskwait)
llvm::Constant *SharedSingleValueReading::get_comm_function_on_sync(llvm::Module &M,
                                                                    environment *e)
{
    // no need to to something special at sync-Point
    return nullptr;
}
