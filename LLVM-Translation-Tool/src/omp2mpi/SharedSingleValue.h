#ifndef OMP2MPI_SHAREDSINGLEVALUE_H_
#define OMP2MPI_SHAREDSINGLEVALUE_H_

#include "SharedVariable.h"
#include "environment.h"
#include "llvm/IR/Value.h"

#include <vector>

/*
 * Base class for shared array Typed Variables
 */
class SharedSingleValue : public SharedVariable
{
  public:
    // constructors that derived should offer:
    // SharedVariable(llvm::Value *val);
    // SharedVariable(SharedVariable *original_val,llvm::Value *val); // to use when it is
    // passed to a function

    virtual ~SharedSingleValue(){};

    // casting:
    static inline bool classof(SharedSingleValue const *) { return true; }

    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedSingleValueID:
        case SharedSingleValueDefaultID:
        case SharedSingleValueReadingID:
            return true;
        default:
            return false;
        }
    }

    // Currently contains nothing special to arrays
    //
};

class SharedSingleValueDefault : public SharedSingleValue
{
  public:
    // casting
    static inline bool classof(SharedSingleValueDefault const *) { return true; }
    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedSingleValueDefaultID:
            return true;
        default:
            return false;
        }
    }

    // constructors that derived should offer:
    SharedSingleValueDefault(llvm::Value *val);
    SharedSingleValueDefault(SharedVariable *original_val,
                             llvm::Value *val); // to use when it is
    // passed to a function

    ~SharedSingleValueDefault(){};

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

// shared reading comm pattern
class SharedSingleValueReading : public SharedSingleValue
{
  public:
    // casting
    static inline bool classof(SharedSingleValueReading const *) { return true; }
    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedSingleValueReadingID:
            return true;
        default:
            return false;
        }
    }

    // constructors that derived should offer:
    SharedSingleValueReading(llvm::Value *val);
    SharedSingleValueReading(SharedVariable *original_val,
                             llvm::Value *val); // to use when it is
    // passed to a function

    ~SharedSingleValueReading(){};

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
