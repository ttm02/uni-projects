#ifndef OMP2MPI_ENVIRONMENT_H_
#define OMP2MPI_ENVIRONMENT_H_

// contains the envoronment-struct
// this should be included first

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

// forward declaration of ParallelFunction, ParallelEntryFunction and ParallelTaskEntry to
// resolve circular dependencies:
class ParallelFunction;
class ParallelEntryFunction;
class ForkEntry;
class TaskEntry;
// same for shared Variable
class SharedVariable;

#include <map>
#include <set>
#include <tuple>
#include <vector>

// stores global variables to use in the pass
struct global_variables
{
    llvm::GlobalVariable *rank;
    llvm::GlobalVariable *size;
    llvm::GlobalVariable *struct_ptr_win;
    llvm::GlobalVariable *task_param_win;

    std::vector<SharedVariable *> shared_globals;

    // The blocks that should contain all init and wrap-up
    // part if it should be done globally
    llvm::BasicBlock *setup = nullptr;
    // insert setup before the block terminator
    llvm::BasicBlock *wrapUp = nullptr;
    // insert wrapUp before first instruction

    // map containing mpi windows for each shared global variable
    // TODO DECREPARTED: REMOVE
    std::map<llvm::GlobalVariable *, llvm::Value *> mpi_win;
};

// number of functions defined below:
#define OMP2MPI_NUMBER_OF_EXTERNAL_FUNCTIONS 84
#define OMP2MPI_MAXIMUM_RTLIB_CALL_DEPTH 3

// holds functions to use in te pass
struct external_functions
{
    // External functions from external_module
    llvm::Constant *mpi_init;
    llvm::Constant *mpi_finalize;
    llvm::Constant *mpi_bcast;
    llvm::Constant *mpi_allreduce;
    llvm::Constant *mpi_update_var_before;
    llvm::Constant *mpi_update_var_after;
    llvm::Constant *mpi_rank;
    llvm::Constant *mpi_size;
    llvm::Constant *mpi_create_dynamic_window;
    llvm::Constant *mpi_create_shared_array_window;
    llvm::Constant *mpi_store_in_shared_array;
    llvm::Constant *mpi_free_shared_array_window;
    llvm::Constant *mpi_one_sided_bcast;
    llvm::Constant *mpi_load_from_shared_array;
    llvm::Constant *calculate_for_boundaries_int;
    llvm::Constant *calculate_for_boundaries_long;
    llvm::Constant *add_missing_recv_calls;
    llvm::Constant *mpi_barrier;
    llvm::Constant *mpi_manual_reduce;
    llvm::Constant *mpi_load_shared_struct;
    llvm::Constant *mpi_store_shared_struct_ptr;
    llvm::Constant *mpi_copy_shared_struct_ptr;
    llvm::Constant *mpi_load_shared_struct_ptr;
    llvm::Constant *mpi_cmp_EQ_shared_struct_ptr;
    llvm::Constant *mpi_cmp_NE_shared_struct_ptr;
    llvm::Constant *mpi_store_to_shared_struct;
    llvm::Constant *log_memory_allocation;
    llvm::Constant *get_array_size_from_address;
    llvm::Constant *check_2d_array_memory_allocation_style;
    llvm::Constant *create_2d_array_window;
    llvm::Constant *free_2d_array_window;
    llvm::Constant *load_from_2d_array_window;
    llvm::Constant *store_in_2d_array_window;
    llvm::Constant *create_1d_array_window;
    llvm::Constant *free_1d_array_window;
    llvm::Constant *bcast_shared_array_from_master;
    llvm::Constant *distribute_shared_array_from_master;
    llvm::Constant *free_distributed_shared_array_from_master;
    llvm::Constant *cache_shared_array_line;
    llvm::Constant *invlaidate_shared_array_cache;
    llvm::Constant *invlaidate_shared_array_cache_simple_signature;
    llvm::Constant *store_to_shared_array_line;
    llvm::Constant *get_own_upper_array_line;
    llvm::Constant *get_own_lower_array_line;
    llvm::Constant *init_single_value_comm_info;
    llvm::Constant *free_single_value_comm_info;
    llvm::Constant *mpi_store_single_value_var;
    llvm::Constant *mpi_load_single_value_var;
    llvm::Constant *is_array_row_own;
    llvm::Constant *mpi_store_single_value_var_reading;
    llvm::Constant *init_single_value_comm_info_shared_reading;
    llvm::Constant *bcast_array_from_master;
    llvm::Constant *free_bcasted_array_from_master;
    llvm::Constant *store_to_bcasted_array_line;
    llvm::Constant *init_master_based_array_info;
    llvm::Constant *free_master_based_array;
    llvm::Constant *store_to_master_based_array_line;
    llvm::Constant *load_from_master_based_array_line;
    llvm::Constant *sync_master_based_array;
    llvm::Constant *invlaidate_shared_array_cache_release_mem;

    // tasklib
    llvm::Constant *mpi_global_tasking_info_init;
    llvm::Constant *mpi_global_tasking_info_destroy;
    llvm::Constant *init_all_shared_vars_for_tasking;
    llvm::Constant *setup_this_shared_var_for_tasking;
    llvm::Constant *wrap_up_all_shared_vars_for_tasking;
    llvm::Constant *mpi_add_task;
    llvm::Constant *mpi_sync_task;
    llvm::Constant *create_new_task_info;
    llvm::Constant *add_shared_param;
    llvm::Constant *add_private_param;
    llvm::Constant *fetch_next_shared_param;
    llvm::Constant *fetch_next_private_param;

    // from mpi mutex
    llvm::Constant *MPI_Mutex_init;
    llvm::Constant *MPI_Mutex_destroy;
    llvm::Constant *MPI_Mutex_lock;
    llvm::Constant *MPI_Mutex_unlock;
    llvm::Constant *MPI_Mutex_trylock;

    // direct MPI
    llvm::Constant *MPI_Win_attach;
    llvm::Constant *MPI_Win_detach;
    llvm::Constant *MPI_Win_free;
    llvm::Constant *MPI_Op_create;
    llvm::Constant *MPI_Op_free;

    // for debugging:
    llvm::Constant *print_int;
    llvm::Constant *print_long;
};

// holds types defined in the pass
struct types
{
    // mpi mutex struct
    llvm::StructType *mpi_mutex;
    // struct that contains all information about a struct ptr
    llvm::StructType *mpi_struct_ptr;
    // TODO? reorganisieren, sodass es ein extra struct für die Struct-Typen der Communikation
    // gibt?
    // informations about distributed array
    llvm::StructType *array_distribution_info;
    llvm::StructType *bcasted_array_info;
    llvm::StructType *master_based_array_info;
    // communication info for single value vars
    llvm::StructType *single_value_info;

    // map old struct types to new one
    std::map<llvm::StructType *, llvm::StructType *> struct_old_new_map;

    // function type for MPI user defined functions
    llvm::FunctionType *MPI_User_function;
    llvm::FunctionType *sync_point_callback_func_type;

    // for Tasking:
    llvm::StructType *task_info_struct;
};

// holds information on other loaded modules
typedef struct
{
    // the actual module
    std::unique_ptr<llvm::Module> M;

    // Function that are exported by this module
    std::vector<llvm::Function *> exportedF;
    // auxiliaryF include all other functions defined in module as well as declaration of MPI
    // functions
    std::vector<llvm::Function *> auxiliaryF;

    // mapping of functions/global Values in this module to functions in the passed module
    std::map<llvm::Value *, llvm::Value *> val_map;

} module_info;

// defines the environment struct that contain all information needed for replacement functions
typedef struct
{
    module_info *external_module;

    struct types *types = nullptr;
    struct external_functions *functions = nullptr;

    struct global_variables *global = nullptr;

    std::vector<ForkEntry *> microtask_functions;
    std::vector<TaskEntry *> task_functions;

    // contain the top lvl of every shared Variable (except globals, as they are held in global
    // struct) this means it contains the original allocation of this Var (to possibly find an
    // annotation)
    std::vector<SharedVariable *> top_lvl_vars;

    // function that will be executed by all non Master Processes instead of main body
    llvm::Function *worker_Main = nullptr;
    llvm::SwitchInst *worker_Main_switch = nullptr;

    // function that will be executed by any process when he start to work on a task
    llvm::Function *execute_task = nullptr;
    llvm::SwitchInst *execute_task_switch = nullptr;

    // set of all blocks where no further communication should be entered
    std::set<llvm::BasicBlock *> no_further_comm;

    // map containing the wrap up and finish block for each function
    std::map<llvm::Function *, std::pair<llvm::BasicBlock *, llvm::BasicBlock *>>
        init_and_finish_block_map;

    // contains the map for each used variable type to the struct type that contains the
    // default communication info
    std::map<llvm::Type *, llvm::StructType *> default_comm_info_map;

    // needed for SCCP
    llvm::TargetLibraryInfo *TLI;

} environment;

// these includes needs the environment to be defined.
// They contain complete declaration of classes forwardly declared above
#include "ParallelEntryFunction.h"
#include "ParallelFunction.h"
#include "ParallelTaskEntry.h"

#include "SharedVariable.h"

#endif /* OMP2MPI_ENVIRONMENT_H_ */
