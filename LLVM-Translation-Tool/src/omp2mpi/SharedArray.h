#ifndef OMP2MPI_SHAREDARRAY_H_
#define OMP2MPI_SHAREDARRAY_H_

#include "SharedVariable.h"
#include "environment.h"
#include "llvm/IR/Value.h"

#include <vector>

/*
 * Base class for shared array Typed Variables
 */
class SharedArray : public SharedVariable
{
  public:
    // constructors that derived should offer:
    // SharedVariable(llvm::Value *val);
    // SharedVariable(SharedVariable *original_val,llvm::Value *val); // to use when it is
    // passed to a function

    virtual ~SharedArray(){};

    // casting:
    static inline bool classof(SharedArray const *) { return true; }

    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedArrayID:
        case SharedArrayDistributedID:
        case SharedArrayDistributedMemoryAwareID:
        case SharedArrayBcastedID:
        case SharedArrayMasterBasedID:
            return true;
        default:
            return false;
        }
    }

    // return values needed for init call
    llvm::Value *get_dimension(llvm::Module &M, environment *e);
    llvm::Value *get_base_type_size(llvm::Module &M, environment *e);
};

// Class that represents distributed shared arrays
class SharedArrayDistributed : public SharedArray
{
  public:
    // casting
    static inline bool classof(SharedArrayDistributed const *) { return true; }
    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedArrayDistributedID:
        case SharedArrayDistributedMemoryAwareID:
            return true;
        default:
            return false;
        }
    }

    // constructors that derived should offer:
    SharedArrayDistributed(llvm::Value *val);
    SharedArrayDistributed(SharedVariable *original_val,
                           llvm::Value *val); // to use when it is
    // passed to a function

    ~SharedArrayDistributed(){};

    llvm::StructType *get_comm_info_type(llvm::Module &M, environment *e) override;

    // which funcions should this variable use for communication
    llvm::Constant *get_comm_function_on_store(llvm::Module &M, environment *e) override;
    llvm::Constant *get_comm_function_on_load(llvm::Module &M, environment *e) override;
    // init and finish a parallel region
    llvm::Constant *get_comm_function_on_init(llvm::Module &M, environment *e) override;
    llvm::Constant *get_comm_function_on_finish(llvm::Module &M, environment *e) override;
    // at sync_point (e.g. barrier or taskwait)
    llvm::Constant *get_comm_function_on_sync(llvm::Module &M, environment *e) override;
};

// Class that represents distributed shared arrays
class SharedArrayDistributedMemoryAware : public SharedArrayDistributed
{
  public:
    // casting
    static inline bool classof(SharedArrayDistributed const *) { return true; }
    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedArrayDistributedMemoryAwareID:
            return true;
        default:
            return false;
        }
    }

    // constructors that derived should offer:
    SharedArrayDistributedMemoryAware(llvm::Value *val);
    SharedArrayDistributedMemoryAware(SharedVariable *original_val, llvm::Value *val);

    ~SharedArrayDistributedMemoryAware(){};

    // other comm functions: same as SharedArrayDistributed so no override
    // at sync_point (e.g. barrier or taskwait)
    llvm::Constant *get_comm_function_on_sync(llvm::Module &M, environment *e) override;
};

// Class that represents bcastedshared arrays
class SharedArrayBcasted : public SharedArray
{
  private:
    static std::map<llvm::Type *, llvm::StructType *> already_constructed_types;
    static llvm::StructType *get_comm_info_type(llvm::Module &M, environment *e,
                                                llvm::Type *local_buffer_type);

  public:
    // casting
    static inline bool classof(SharedArrayBcasted const *) { return true; }
    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedArrayBcastedID:
            return true;
        default:
            return false;
        }
    }

    // constructors that derived should offer:
    SharedArrayBcasted(llvm::Value *val);
    SharedArrayBcasted(SharedVariable *original_val,
                       llvm::Value *val); // to use when it is
    // passed to a function

    ~SharedArrayBcasted(){};

    llvm::StructType *get_comm_info_type(llvm::Module &M, environment *e) override;

    // which funcions should this variable use for communication
    llvm::Constant *get_comm_function_on_store(llvm::Module &M, environment *e) override;
    llvm::Constant *get_comm_function_on_load(llvm::Module &M, environment *e) override;
    // init and finish a parallel region
    llvm::Constant *get_comm_function_on_init(llvm::Module &M, environment *e) override;
    llvm::Constant *get_comm_function_on_finish(llvm::Module &M, environment *e) override;
    // at sync_point (e.g. barrier or taskwait)
    llvm::Constant *get_comm_function_on_sync(llvm::Module &M, environment *e) override;
};

// Class that represents master based shared arrays
class SharedArrayMasterBased : public SharedArray
{
  private:
    static std::map<llvm::Type *, llvm::StructType *> already_constructed_types;
    static llvm::StructType *
    get_comm_info_type(llvm::Module &M, environment *e,
                       llvm::Type *local_buffer_type); // TODO this function offers much
                                                       // similarity to other static functions
                                                       // maybe implement it in superclass

  public:
    // casting
    static inline bool classof(SharedArrayMasterBased const *) { return true; }
    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedArrayMasterBasedID:
            return true;
        default:
            return false;
        }
    }

    // constructors that derived should offer:
    SharedArrayMasterBased(llvm::Value *val);
    SharedArrayMasterBased(SharedVariable *original_val,
                           llvm::Value *val); // to use when it is
    // passed to a function

    ~SharedArrayMasterBased(){};

    llvm::StructType *get_comm_info_type(llvm::Module &M, environment *e) override;

    // which funcions should this variable use for communication
    llvm::Constant *get_comm_function_on_store(llvm::Module &M, environment *e) override;
    llvm::Constant *get_comm_function_on_load(llvm::Module &M, environment *e) override;
    // init and finish a parallel region
    llvm::Constant *get_comm_function_on_init(llvm::Module &M, environment *e) override;
    llvm::Constant *get_comm_function_on_finish(llvm::Module &M, environment *e) override;
    // at sync_point (e.g. barrier or taskwait)
    llvm::Constant *get_comm_function_on_sync(llvm::Module &M, environment *e) override;
};

#endif
