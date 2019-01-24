#ifndef OMP2MPI_ENVIRONMENT_H_
#include "environment.h"
// this will include SharedVariable as well
// this is for convenience so that it is secured, that environment will be included first and
// include ordering will be correct
#endif

#ifndef OMP2MPI_SHAREDVARIABLE_H_
#define OMP2MPI_SHAREDVARIABLE_H_

#include "environment.h"
#include "llvm/IR/Value.h"

#include <vector>

// this definition allowes to have multiple comm modes at once (one hot encoding)
// although this is not useful for the current version of the pass
enum Comm_Type
{
    Default = 0,
    Reading = 1 << 0,
    Distributed = 1 << 1,
    MemAwareDistributed = 1 << 1 | 1 << 3,
    MasterBased = 1 << 2
};

// needed for use of LLVM cast system
// every Class in the hirachy needs an ID here
enum SharedTypeID
{
    SharedVariableID,
    SharedUnhandledTypeID,
    SharedArrayID,
    SharedArrayDistributedID,
    SharedArrayDistributedMemoryAwareID,
    SharedArrayBcastedID,
    SharedArrayMasterBasedID,
    SharedSingleValueID,
    SharedSingleValueDefaultID,
    SharedSingleValueReadingID
};

/*
 * Base class Contains information about a shared variable
 * Derived classes should be used e.g. shared Struct shared array shared single value
 */
class SharedVariable
{
  private:
    // is there any need to expose this to derived classes?
    static void find_comm_type_from_annotation(llvm::Module &M, environment *e,
                                               llvm::Value *var, Comm_Type *comm_type);

  protected:
    // A pointer to the variable that this instance is representing.
    llvm::Value *var;

    // for LLVM casting system
    SharedTypeID Type_ID;

    // parent of this shared Var
    // this means the variable in the caller Function
    SharedVariable *parent_var;
    // if parent is null, Var is either a global or an allocation or cast
    //(e.g. pointer cast of allocation with malloc)

    // create specific types
    static SharedVariable *CreateArray(llvm::Value *var, Comm_Type comm);
    static SharedVariable *CreateSingleValue(llvm::Value *var, Comm_Type comm);
    static SharedVariable *CreateUnhandleledType(llvm::Value *var, Comm_Type comm);

  public:
    // factory that constructs a shared Variable with their specific type
    static SharedVariable *Create(llvm::Module &M, environment *e, SharedVariable *parent,
                                  llvm::Value *var);
    static SharedVariable *Create(llvm::Module &M, environment *e, llvm::Value *var);
    static SharedVariable *Create(llvm::Module &M, environment *e, llvm::Value *var,
                                  Comm_Type comm, bool override_comm_if_annotation = false);

    // needed in order to use LLVM-Casting system as pass is compiled with no RTTI
    // see
    // https://stackoverflow.com/questions/6038330/how-is-llvm-isa-implemented/6068950#6068950
    static inline bool classof(SharedVariable const *) { return true; }
    SharedTypeID getValueID() const { return Type_ID; }

    // constructors that derived should offer:
    // SharedVariable(llvm::Value *val);
    // SharedVariable(SharedVariable *original_val,llvm::Value *val);
    // to use when it is passed to a function (parent!=null)

    virtual ~SharedVariable(){};

    llvm::Value *value();
    // for convenience
    llvm::Type *getType();
    void dump();

    SharedVariable *getParent(); // may return nullptr
    // gives the origin where the variable was allocated
    SharedVariable *getOrigin(); // may not return null. if parent=null should retrun self

    // always used as the second arg for communicating functions
    llvm::Value *get_local_buffer_size(llvm::Module &M, environment *e);
    // just an other name of get_local_buffer_size
    llvm::Value *get_comm_func_arg_2(llvm::Module &M, environment *e)
    {
        return get_local_buffer_size(M, e);
    }

    virtual llvm::StructType *get_comm_info_type(llvm::Module &M, environment *e) = 0;

    // which funcions should this variable use for communication
    virtual llvm::Constant *get_comm_function_on_store(llvm::Module &M, environment *e) = 0;
    virtual llvm::Constant *get_comm_function_on_load(llvm::Module &M, environment *e) = 0;
    // init and finish a parallel region
    virtual llvm::Constant *get_comm_function_on_init(llvm::Module &M, environment *e) = 0;
    virtual llvm::Constant *get_comm_function_on_finish(llvm::Module &M, environment *e) = 0;
    // at sync_point (e.g. barrier or taskwait)
    virtual llvm::Constant *get_comm_function_on_sync(llvm::Module &M, environment *e) = 0;

    // function used to change the allocation of global
    void handle_if_global(llvm::Module &M, environment *e);
};

// class that contain shared Variables that are currently unhandled
// currently static sized arrays
// currently structs are classified here as well, as they have special handeling
class SharedUnhandledType : public SharedVariable
{
  public:
    // casting
    static inline bool classof(SharedUnhandledType const *) { return true; }
    static inline bool classof(SharedVariable const *B)
    {
        switch (B->getValueID())
        {
        case SharedUnhandledTypeID:
            return true;
        default:
            return false;
        }
    }
    // constructors
    SharedUnhandledType(llvm::Value *val)
    {
        this->Type_ID = SharedUnhandledTypeID;
        this->var = val;
        this->parent_var = nullptr;
    }
    SharedUnhandledType(SharedVariable *original_val, llvm::Value *val)
    {
        this->Type_ID = SharedUnhandledTypeID;
        this->var = val;
        this->parent_var = original_val;
    }

    llvm::StructType *get_comm_info_type(llvm::Module &M, environment *e) override
    {
        return nullptr;
    }

    // which funcions should this variable use for communication
    llvm::Constant *get_comm_function_on_store(llvm::Module &M, environment *e) override
    {
        return nullptr;
    }
    llvm::Constant *get_comm_function_on_load(llvm::Module &M, environment *e) override
    {
        return nullptr;
    }
    // init and finish a parallel region
    llvm::Constant *get_comm_function_on_init(llvm::Module &M, environment *e) override
    {
        return nullptr;
    }
    llvm::Constant *get_comm_function_on_finish(llvm::Module &M, environment *e) override
    {
        return nullptr;
    }
    // at sync_point (e.g. barrier or taskwait)
    llvm::Constant *get_comm_function_on_sync(llvm::Module &M, environment *e) override
    {
        return nullptr;
    }
};

#endif
