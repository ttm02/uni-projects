#ifndef OMP2MPI_HELPER_H_
#define OMP2MPI_HELPER_H_

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

#include <vector>

#include "environment.h"

// contains various helper functions

// Returns a vector of all Users of the named function
// Can be used to find all instances where the named function is called in the
// IR Module M
std::vector<llvm::User *> get_function_users(llvm::Module &M, llvm::StringRef name);

// gives the ParallelEntryFunction = Parallel region the instruction resides in (or nullptr if
// sequential)
ForkEntry *get_ForkEntry(llvm::Module &M, environment *e, llvm::Instruction *inst);
ForkEntry *get_ForkEntry(llvm::Module &M, environment *e, llvm::Function *func);

// will give the SharedVariable object that is top_level for the Value var
// (use it) in order to create a new "child" of this var
SharedVariable *get_top_level_var(llvm::Module &M, environment *e, llvm::Value *var);

// Determines the length of an dynamically allocated array that has been
// passed through the IR code as a pointer type without the information about
// its length.
int get_array_length(llvm::Module &M, llvm::Value *array);

// determines if struct is user defined and not inserted by openmp or our pass
bool is_struct_user_defined(llvm::Module &M, environment *e, llvm::StructType *ty);

// determines if global variable is defined by our pass
// BEWARE: global constants such as string literals from the pass are not included
bool is_global_defined_by_pass(llvm::Module &M, environment *e, llvm::GlobalVariable *var);
// determine if global is defined by openmp
bool is_global_defined_by_openmp(llvm::GlobalVariable *var);

// Returns the right MPI_Datatype for the Value
int get_mpi_datatype(llvm::Value *value);
int get_mpi_datatype(llvm::Type *type);
// If MPI_BYTE is returned, use this function to get the size of complex datatype in bytes
size_t get_size_in_Byte(llvm::Module &M, llvm::Value *value);
size_t get_size_in_Byte(llvm::Module &M, llvm::Type *type);

// checks if given instruction resides within a omp critical region
bool is_in_omp_critical(llvm::Instruction *instruction);

// checks if someone has declared that no further MPI communicatioin should be used for this
// instruction
bool is_comm_allowed(environment *e, llvm::Instruction *instruction);

// insert a new block at the beginning of func to insert all initialization there
// builds a new block at the end of func to insert all wrap up procedures
// returns pair setupBlock,WrapUpBlock
std::pair<llvm::BasicBlock *, llvm::BasicBlock *>
build_init_and_wrapUp_block(llvm::Module &M, environment *e, llvm::Function *F);

// for convenience:
llvm::BasicBlock *get_init_block(llvm::Module &M, environment *e, llvm::Function *F);
llvm::BasicBlock *get_wrapUp_block(llvm::Module &M, environment *e, llvm::Function *F);

// insert the init and free of an mpi win for a global variable
void init_global_mpi_win(llvm::Module &M, environment *e, llvm::GlobalVariable *var);

// Returns the depth of Pointer types
// E.g.: type** -> depth = 2
//       type* -> depth = 1
int get_pointer_depth(llvm::Type *type);

// adds all operations using the supplied operation to the given vector
// adds all usages uf the usages recursively
// only removes store if usage is vlaue operand
void add_remove_all_uses(std::vector<llvm::Instruction *> *to_remove, llvm::Instruction *inst,
                         bool only_remove_store_val);

// The function scans the content of the Function func for all appearances of
// Instructions of the type T and returns them in a vector
template <class T> std::vector<T *> get_instruction_in_func(llvm::Function *func)
{
    std::vector<T *> instructions;
    for (auto &B : *func)
    {
        for (auto &I : B)
        {
            if (auto *inst = llvm::dyn_cast<T>(&I))
            {
                instructions.push_back(inst);
            }
        }
    }
    return instructions;
}

// utility makro for debugging:
#define ASK_TO_CONTINIUE                                                                      \
    int ask_for = 0;                                                                          \
    printf("Continiue? (0 to abort):");                                                       \
    scanf("%d", &ask_for);                                                                    \
    if (!ask_for)                                                                             \
    {                                                                                         \
        printf("Aborting\n");                                                                 \
        exit(0);                                                                              \
    }

// utility macro:
// usage: char* foo=nullptr;
// PRINTstr(foo,"foo%s ","bar");
// !!!use with caution!!!:
// need int PRINTstr_len! to be defined in order to work properly!!
// PRINTstr_len will be overwritten !!
// tgt will be malloced and therefore overwritten!!
#define PRINTstr(tgt, formatstr, ...)                                                         \
    PRINTstr_len = snprintf(NULL, 0, formatstr, __VA_ARGS__);                                 \
    PRINTstr_len++;                                                                           \
    tgt = (char *)malloc(sizeof(char) * PRINTstr_len);                                        \
    snprintf(tgt, PRINTstr_len, formatstr, __VA_ARGS__)

#endif /* OMP2MPI_HELPER_H_ */
