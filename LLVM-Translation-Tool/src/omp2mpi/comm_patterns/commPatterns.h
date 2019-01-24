#ifndef OMP2MPI_COMM_PATTERNS_COMMPATTERNS_H_
#define OMP2MPI_COMM_PATTERNS_COMMPATTERNS_H_

#include <llvm/IR/Module.h>

#include "../SharedArray.h"
#include "../SharedSingleValue.h"
#include "../environment.h"

// Used Function Types:
// a function that distributes the Variables from master
typedef std::vector<llvm::Value *> (*variable_distribution_function)(
    llvm::Module &M, environment *e, ForkEntry *mikrotask,
    llvm::CallInst *insert_before_master, llvm::CallInst *insert_before_worker,
    std::vector<llvm::Value *> *args_to_bcast);

// a function that gives the comm_info struct
typedef llvm::StructType (*get_comm_info_struct_fuction)(llvm::Module &M, environment *e,
                                                         llvm::Type *base_type);

// a function that inserts the Communication for a given Variable
typedef void (*insert_var_comm_function)(llvm::Module &M, environment *e,
                                         ForkEntry *microtask_obj, SharedVariable *shared_var);

// TODO Maintainance rename functions as they should be used in general for Comm-Pattern

// optimizes all loops in given microtask
void optimize_array_loop_acces(llvm::Module &M, environment *e,
                               ParallelFunction *microtask_object);

// functions to call to build each comm_Pattern

// TODO DECREPARTED: restructuring needed
void distribute_array_comm_Pattern(llvm::Module &M, environment *e,
                                   ParallelFunction *microtask_func);

// bcasts all shared variables and all globals from master to have consistent memory view
// before entering a Microtask function
// this means that  worker might have to alloca mem to hold the new variables
// returns args_for_workers
// args to bcast is intended as inout vector to use for the call on master

// same semantics as bcast_shared_vars_from_master for usage with distribute_array_comm_Pattern
std::vector<llvm::Value *>
distribute_shared_vars_from_master(llvm::Module &M, environment *e, ForkEntry *mikrotask,
                                   llvm::CallInst *insert_before_master,
                                   llvm::CallInst *insert_before_worker,
                                   std::vector<llvm::Value *> *args_to_bcast);
// same but also setup all shared vars to use in tasks
std::vector<llvm::Value *>
task_init_shared_vars_from_master(llvm::Module &M, environment *e, ForkEntry *mikrotask,
                                  llvm::CallInst *insert_before_master,
                                  llvm::CallInst *insert_before_worker,
                                  std::vector<llvm::Value *> *args_to_bcast);

// get the struct type that contains the local buffer and all information needed for
// default pattern
llvm::StructType *get_default_comm_info_struct_type(llvm::Module &M, environment *e,
                                                    llvm::Type *base_type);

// functions to only use a part of each comm pattern
void default_single_value_var(llvm::Module &M, environment *e, ParallelFunction *microtask_obj,
                              SharedSingleValue *shared_var);
// TODO DECREPARTED:
void default_static_array_var(llvm::Module &M, environment *e, ParallelFunction *microtask_obj,
                              llvm::Value *shared_array_var);

void distribute_array_array_ptr_var(llvm::Module &M, environment *e,
                                    ParallelFunction *microtask_object, SharedArray *var);

// only use a part of Varialbe distribution pattern
// returns pair: new Master arg, new worker arg
std::pair<llvm::Value *, llvm::Value *> bcast_shared_struct_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, llvm::Value *arg_to_bcast);
std::pair<llvm::Value *, llvm::Value *> bcast_shared_single_value_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, SharedSingleValue *arg_to_bcast);
std::pair<llvm::Value *, llvm::Value *> bcast_firstprivate_single_value_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, llvm::Value *arg_to_bcast);
std::pair<llvm::Value *, llvm::Value *> distribute_shared_dynamic_array_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, SharedArray *array_to_bcast);

void bcast_all_globals(llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
                       llvm::CallInst *insert_before_worker);

#endif /* OMP2MPI_COMM_PATTERNS_COMMPATTERNS_H_ */
