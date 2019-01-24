#include "external_functions.h"
#include "environment.h"
#include "helper.h"
#include <assert.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

using namespace llvm;

#define DEBUG_EXTERNAL_FUNCTIONS 0

#if DEBUG_EXTERNAL_FUNCTIONS == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

void load_external_globals(llvm::Module &M, environment *e)
{
    // load globals e.g. strings for debug printing
    for (auto global = e->external_module->M->global_begin();
         global != e->external_module->M->global_end(); global++)
    {
        Constant *init = nullptr;
        if (global->hasInitializer())
        {
            init = global->getInitializer();
        }
        else
        {
            errs() << "WARNING: loaded global has no initializer\n";
            global->dump();
        }
        GlobalVariable *new_global = new GlobalVariable(
            M, global->getType()->getPointerElementType(), global->isConstant(),
            global->getLinkage(), init, global->getName());
        // using pointer element type because globals are always ptr
        e->external_module->val_map[dyn_cast<GlobalVariable>(global)] = new_global;

        if (global->getName().startswith("my_rank"))
        {
            e->global->rank = new_global;
        }
        else if (global->getName().startswith("numprocs"))
        {
            e->global->size = new_global;
        }
    }
}

void open_rtlib_file(llvm::Module &M, environment *e)
{
    assert(e->external_module != nullptr);
    char *das_tool_root = getenv("DAS_TOOL_ROOT");
    // TODO ? or direct get the complete path as env variable?
    char *rtlib_IR_file_name;

    if (das_tool_root == nullptr)
    {
        errs() << "ERROR: environment variable DAS_TOOL_ROOT was not set!\n";
        return;
    }

    // Pass will compile external_module with 02 in order to load current version
    int PRINTstr_len;
    // get a unique filename for external_module
    PRINTstr(rtlib_IR_file_name, "%s/src/build/rtlib.bc", das_tool_root);
    Debug(errs() << "Filename for rtlib: " << rtlib_IR_file_name << "\n";)

        SMDiagnostic rtlib_error;
    // Load module into the same context!
    e->external_module->M =
        getLazyIRFileModule(rtlib_IR_file_name, rtlib_error, M.getContext());
    // Debug(e->external_module->M->dump();)
    free(rtlib_IR_file_name);
}

void load_external_module_definitions(llvm::Module &M, environment *e)
{
    Debug(errs() << "loading the rtlib\n";)

        // get Types first:
        for (auto *struct_type : e->external_module->M->getIdentifiedStructTypes())
    {
        if (struct_type->getName().startswith("struct.mpi_struct_ptr"))
        {
            e->types->mpi_struct_ptr = struct_type;
        }
        else if (struct_type->getName().startswith("struct.task_info_struct"))
        {
            e->types->task_info_struct = struct_type;
        }
        else if (struct_type->getName().startswith("struct.MPI_Mutex"))
        {
            e->types->mpi_mutex = struct_type;
        }
        else if (struct_type->getName().startswith("struct.array_distribution_info"))
        {
            e->types->array_distribution_info = struct_type;
        }
        else if (struct_type->getName().startswith("struct.single_value_info"))
        {
            e->types->single_value_info = struct_type;
        }
        else if (struct_type->getName().startswith("struct.bcasted_array_info"))
        {
            e->types->bcasted_array_info = struct_type;
        }
        else if (struct_type->getName().startswith("struct.master_based_array_info"))
        {
            e->types->master_based_array_info = struct_type;
        }
    }

    load_external_globals(M, e);

    // now load every function declararion to main module
    for (auto &func : e->external_module->M->functions())
    {
        if (false)
        {
            // this case is needed for makro expansion
            // one can think of the makro like a switch
        }
        MATCH_FUNCTION(e->functions->mpi_init, "_Z8init_mpiv")
        MATCH_FUNCTION(e->functions->mpi_finalize, "_Z12finalize_mpiv")
        MATCH_FUNCTION(e->functions->mpi_update_var_before,
                       "_Z28mpi_update_shared_var_beforeiiPvi")
        MATCH_FUNCTION(e->functions->mpi_update_var_after,
                       "_Z27mpi_update_shared_var_afteriiPvi")
        MATCH_FUNCTION(e->functions->mpi_rank, "_Z8mpi_rankv")
        MATCH_FUNCTION(e->functions->mpi_size, "_Z8mpi_sizev")
        MATCH_FUNCTION(e->functions->mpi_bcast, "_Z9mpi_bcastPviii")
        MATCH_FUNCTION(e->functions->mpi_allreduce, "_Z13mpi_allreducePviii")
        MATCH_FUNCTION(e->functions->mpi_create_dynamic_window,
                       "_Z25mpi_create_dynamic_windowPi")
        MATCH_FUNCTION(e->functions->mpi_create_shared_array_window,
                       "_Z30mpi_create_shared_array_windowPviPii")
        MATCH_FUNCTION(e->functions->mpi_store_in_shared_array,
                       "_Z25mpi_store_in_shared_arrayPviiPii")
        MATCH_FUNCTION(e->functions->mpi_free_shared_array_window,
                       "_Z28mpi_free_shared_array_windowPviiPii")
        MATCH_FUNCTION(e->functions->mpi_load_from_shared_array,
                       "_Z26mpi_load_from_shared_arrayPviiPii")
        MATCH_FUNCTION(e->functions->mpi_one_sided_bcast, "_Z19mpi_one_sided_bcastPviPii")
        MATCH_FUNCTION(e->functions->calculate_for_boundaries_int,
                       "_Z28calculate_for_boundaries_intPiS_S_ii")
        MATCH_FUNCTION(e->functions->calculate_for_boundaries_long,
                       "_Z29calculate_for_boundaries_longPlS_S_ii")
        MATCH_FUNCTION(e->functions->add_missing_recv_calls,
                       "_Z22add_missing_recv_callsiiPvii")
        MATCH_FUNCTION(e->functions->mpi_barrier, "_Z11mpi_barrierv")
        MATCH_FUNCTION(e->functions->mpi_manual_reduce,
                       "_Z17mpi_manual_reducePciPFvS_S_PiS0_E")
        MATCH_FUNCTION(e->functions->mpi_store_to_shared_struct,
                       "_Z26mpi_store_to_shared_structPvP14mpi_struct_ptrPi")
        MATCH_FUNCTION(e->functions->mpi_store_shared_struct_ptr,
                       "_Z27mpi_store_shared_struct_ptrPvmP14mpi_struct_ptr")
        MATCH_FUNCTION(e->functions->mpi_copy_shared_struct_ptr,
                       "_Z26mpi_copy_shared_struct_ptrP14mpi_struct_ptrS0_")
        MATCH_FUNCTION(e->functions->mpi_load_shared_struct_ptr,
                       "_Z26mpi_load_shared_struct_ptrPvP14mpi_struct_ptr")
        MATCH_FUNCTION(e->functions->mpi_cmp_EQ_shared_struct_ptr,
                       "_Z28mpi_cmp_EQ_shared_struct_ptrP14mpi_struct_ptrS0_")
        MATCH_FUNCTION(e->functions->mpi_cmp_NE_shared_struct_ptr,
                       "_Z28mpi_cmp_NE_shared_struct_ptrP14mpi_struct_ptrS0_")
        MATCH_FUNCTION(e->functions->mpi_load_shared_struct,
                       "_Z22mpi_load_shared_structPvP14mpi_struct_ptrPi")
        MATCH_FUNCTION(e->functions->log_memory_allocation, "_Z21log_memory_allocationPvl")
        MATCH_FUNCTION(e->functions->get_array_size_from_address,
                       "_Z27get_array_size_from_addressPv")
        MATCH_FUNCTION(e->functions->check_2d_array_memory_allocation_style,
                       "_Z38check_2d_array_memory_allocation_stylePv")
        MATCH_FUNCTION(e->functions->create_2d_array_window,
                       "_Z22create_2d_array_windowPviPPiPl")
        MATCH_FUNCTION(e->functions->free_2d_array_window, "_Z20free_2d_array_windowPPiPlPvii")
        MATCH_FUNCTION(e->functions->store_in_2d_array_window,
                       "_Z24store_in_2d_array_windowPvS_lliPPiPli")
        MATCH_FUNCTION(e->functions->load_from_2d_array_window,
                       "_Z25load_from_2d_array_windowPvS_lliPPiPli")
        MATCH_FUNCTION(e->functions->create_1d_array_window, "_Z22create_1d_array_windowPvPii")
        MATCH_FUNCTION(e->functions->free_1d_array_window, "_Z20free_1d_array_windowPviPii")
        MATCH_FUNCTION(e->functions->bcast_shared_array_from_master,
                       "_Z30bcast_shared_array_from_masterPv")
        MATCH_FUNCTION(e->functions->mpi_global_tasking_info_init,
                       "_Z28mpi_global_tasking_info_initi")
        MATCH_FUNCTION(e->functions->mpi_global_tasking_info_destroy,
                       "_Z31mpi_global_tasking_info_destroyv")
        MATCH_FUNCTION(e->functions->mpi_add_task, "_Z12mpi_add_taskP16task_info_struct")
        MATCH_FUNCTION(e->functions->mpi_sync_task, "_Z13mpi_sync_taskb")
        MATCH_FUNCTION(e->functions->init_all_shared_vars_for_tasking,
                       "_Z32init_all_shared_vars_for_taskingi")
        MATCH_FUNCTION(e->functions->setup_this_shared_var_for_tasking,
                       "_Z33setup_this_shared_var_for_taskingPvPFvS_Ei")
        MATCH_FUNCTION(e->functions->wrap_up_all_shared_vars_for_tasking,
                       "_Z35wrap_up_all_shared_vars_for_taskingv")
        MATCH_FUNCTION(e->functions->create_new_task_info, "_Z20create_new_task_infoii")
        MATCH_FUNCTION(e->functions->add_shared_param,
                       "_Z16add_shared_paramP16task_info_structPv")
        MATCH_FUNCTION(e->functions->add_private_param,
                       "_Z17add_private_paramP16task_info_structPvm")
        MATCH_FUNCTION(e->functions->fetch_next_shared_param,
                       "_Z23fetch_next_shared_paramP16task_info_struct")
        MATCH_FUNCTION(e->functions->fetch_next_private_param,
                       "_Z24fetch_next_private_paramPvP16task_info_struct")
        MATCH_FUNCTION(e->functions->MPI_Mutex_init, "_Z14MPI_Mutex_initPP9MPI_Mutexi")
        MATCH_FUNCTION(e->functions->MPI_Mutex_destroy, "_Z17MPI_Mutex_destroyP9MPI_Mutex")
        MATCH_FUNCTION(e->functions->MPI_Mutex_lock, "_Z14MPI_Mutex_lockP9MPI_Mutex")
        MATCH_FUNCTION(e->functions->MPI_Mutex_unlock, "_Z16MPI_Mutex_unlockP9MPI_Mutex")
        MATCH_FUNCTION(e->functions->MPI_Mutex_trylock, "_Z17MPI_Mutex_trylockP9MPI_Mutex")
        MATCH_FUNCTION(e->functions->distribute_shared_array_from_master,
                       "_Z35distribute_shared_array_from_masterPvmim")
        MATCH_FUNCTION(e->functions->free_distributed_shared_array_from_master,
                       "_Z41free_distributed_shared_array_from_masterPvm")
        MATCH_FUNCTION(e->functions->cache_shared_array_line,
                       "_Z23cache_shared_array_linePvml")
        MATCH_FUNCTION(e->functions->invlaidate_shared_array_cache,
                       "_Z29invlaidate_shared_array_cachePvm")
        MATCH_FUNCTION(e->functions->invlaidate_shared_array_cache_simple_signature,
                       "_Z29invlaidate_shared_array_cachePv")
        MATCH_FUNCTION(e->functions->store_to_shared_array_line,
                       "_Z26store_to_shared_array_linePvmlS_")
        MATCH_FUNCTION(e->functions->init_single_value_comm_info,
                       "_Z27init_single_value_comm_infoPvm")
        MATCH_FUNCTION(e->functions->free_single_value_comm_info,
                       "_Z27free_single_value_comm_infoPvm")
        MATCH_FUNCTION(e->functions->mpi_store_single_value_var,
                       "_Z26mpi_store_single_value_varPvm")
        MATCH_FUNCTION(e->functions->mpi_load_single_value_var,
                       "_Z25mpi_load_single_value_varPvm")
        MATCH_FUNCTION(e->functions->get_own_upper_array_line, "_Z13get_own_upperPvm")
        MATCH_FUNCTION(e->functions->get_own_lower_array_line, "_Z13get_own_lowerPvm")
        MATCH_FUNCTION(e->functions->print_int, "_Z9print_inti")
        MATCH_FUNCTION(e->functions->print_long, "_Z10print_longl")
        MATCH_FUNCTION(e->functions->is_array_row_own, "_Z16is_array_row_ownPvml")
        MATCH_FUNCTION(e->functions->mpi_store_single_value_var_reading,
                       "_Z41mpi_store_single_value_var_shared_readingPvm")
        MATCH_FUNCTION(e->functions->init_single_value_comm_info_shared_reading,
                       "_Z42init_single_value_comm_info_shared_readingPvm")
        MATCH_FUNCTION(e->functions->bcast_array_from_master,
                       "_Z23bcast_array_from_masterPvmim")
        MATCH_FUNCTION(e->functions->free_bcasted_array_from_master,
                       "_Z30free_bcasted_array_from_masterPvm")
        MATCH_FUNCTION(e->functions->store_to_bcasted_array_line,
                       "_Z27store_to_bcasted_array_linePvmlS_")
        MATCH_FUNCTION(e->functions->init_master_based_array_info,
                       "_Z28init_master_based_array_infoPvmim")
        MATCH_FUNCTION(e->functions->free_master_based_array, "_Z23free_master_based_arrayPvm")
        MATCH_FUNCTION(e->functions->store_to_master_based_array_line,
                       "_Z32store_to_master_based_array_linePvmlS_")
        MATCH_FUNCTION(e->functions->load_from_master_based_array_line,
                       "_Z33load_from_master_based_array_linePvml")
        MATCH_FUNCTION(e->functions->sync_master_based_array, "_Z23sync_master_based_arrayPvm")
        MATCH_FUNCTION(e->functions->invlaidate_shared_array_cache_release_mem,
                       "_Z41invlaidate_shared_array_cache_release_memPvm")
        // special case: this function need to be filled by the task
        else if (func.getName().startswith("_Z14call_this_taskP16task_info_struct"))
        {
            e->execute_task = dyn_cast<Function>(
                M.getOrInsertFunction(func.getName(), func.getFunctionType()));
            // it does not count as exported function as it will be filled by this pass if
            // needed
            e->external_module->val_map[&func] = e->execute_task;
        }
        else
        {
            // "Default case"
            Debug(errs() << "auxiliary function (or MPI Definition): " << func.getName()
                         << "\n";)

                Constant *new_func =
                    M.getOrInsertFunction(func.getName(), func.getFunctionType());
            if (auto *f = dyn_cast<Function>(new_func))
            {
                // else is defined somewhere else (intrinsic?)
                e->external_module->auxiliaryF.push_back(f);
            }
            e->external_module->val_map[&func] = new_func;
        }
    }
}

// insert definitions of direct MPI calls used in the pass
// they have to be inserted the old way
void load_direct_mpi_definitions(llvm::Module &M, environment *e)
{
    assert(e->types != nullptr);
    LLVMContext &Ctx = M.getContext();
    // direct MPI:
    e->functions->MPI_Win_attach =
        M.getOrInsertFunction("MPI_Win_attach", Type::getInt32Ty(Ctx), Type::getInt32Ty(Ctx),
                              Type::getInt8PtrTy(Ctx), Type::getInt64Ty(Ctx));

    e->functions->MPI_Win_detach =
        M.getOrInsertFunction("MPI_Win_detach", Type::getInt32Ty(Ctx), Type::getInt32Ty(Ctx),
                              Type::getInt8PtrTy(Ctx));

    e->functions->MPI_Win_free =
        M.getOrInsertFunction("MPI_Win_free", Type::getInt32Ty(Ctx), Type::getInt32PtrTy(Ctx));

    e->functions->MPI_Op_create = M.getOrInsertFunction(
        "MPI_Op_create", Type::getInt32Ty(Ctx), e->types->MPI_User_function->getPointerTo(),
        Type::getInt32Ty(Ctx), Type::getInt32PtrTy(Ctx));

    e->functions->MPI_Op_free =
        M.getOrInsertFunction("MPI_Op_free", Type::getInt32Ty(Ctx), Type::getInt32PtrTy(Ctx));
}

void check_if_all_functions_where_loaded(environment *e)
{
    // TODO ? alternatively: calculate n from the size of the struct
    int n = OMP2MPI_NUMBER_OF_EXTERNAL_FUNCTIONS;
    assert(sizeof(struct external_functions) == sizeof(Constant *) * n);

    Constant **function_array = (Constant **)e->functions;
    for (int i = 0; i < n; ++i)
    {
        assert(function_array[i] != nullptr);
    }
}

void load_external_definitions(llvm::Module &M, environment *e)
{
    LLVMContext &Ctx = M.getContext();
    e->functions = new struct external_functions;
    memset(e->functions, 0, sizeof(struct external_functions)); // so all ptr will be null
    e->types = new struct types;
    // declare MPI type
    e->types->MPI_User_function =
        FunctionType::get(Type::getVoidTy(Ctx),
                          {Type::getInt8PtrTy(Ctx), Type::getInt8PtrTy(Ctx),
                           Type::getInt32PtrTy(Ctx), Type::getInt32PtrTy(Ctx)},
                          false);
    // other types are loaded from the specific modules
    e->types->sync_point_callback_func_type =
        FunctionType::get(Type::getVoidTy(Ctx), {Type::getInt8PtrTy(Ctx)}, false);

    load_direct_mpi_definitions(M, e);

    e->external_module = new module_info;
    open_rtlib_file(M, e);

    load_external_module_definitions(M, e);

    check_if_all_functions_where_loaded(e);

    // TODO check if all types where loaded?
}

void load_external_implementations(llvm::Module &M, environment *e)
{
    for (auto &func : e->external_module->M->functions())
    {
        // only if function is implemented in external_module
        if (!func.isDeclaration())
        {
            Debug(errs() << "Load Implementation of function " << func.getName() << "\n";)
                // load the function content
                if (Error err = func.materialize())
            {
                // if error occured:
                errs() << "ERROR in loading Function content from rtlib\n";
                // Program will fail then
                return;
            }

            // contains all returns of new function we expect never more than 5 so we
            // preallocate space for 5
            SmallVector<ReturnInst *, 5> returns;
            // function to copy to:
            Function *func_in_module = dyn_cast<Function>(e->external_module->val_map[&func]);
            llvm::ValueToValueMapTy vmap;
            // build the vmap: maping of called functions/ used clobals:
            for (auto iter = e->external_module->val_map.begin();
                 iter != e->external_module->val_map.end(); ++iter)
            {
                Value *oldval = iter->first;
                Value *newval = iter->second;
                vmap.insert(std::make_pair(oldval, newval));
                if (newval == nullptr)
                {
                    if (isa<ConstantExpr>(oldval))
                    {
                        // this is constant function cast
                        // we need to insert cast of the function copy that resides inside our
                        // module
                        auto *old_cast = dyn_cast<ConstantExpr>(oldval);
                        if (old_cast->isCast())
                        {
                            Instruction *inst = old_cast->getAsInstruction();
                            Value *casted_v = inst->getOperand(0);
                            Value *new_to_cast = e->external_module->val_map[casted_v];
                            if (new_to_cast != nullptr && isa<Constant>(new_to_cast))
                            {
                                newval = old_cast->getWithOperandReplaced(
                                    0, dyn_cast<Constant>(new_to_cast));
                                // delete temporary inst:
                                inst->deleteValue();
                                continue; // skip below warning as this case was succesfully
                                          // handled
                            }
                        }
                    }

                    // if not continiued above: warning
                    errs() << "Error mapping null:";
                    oldval->dump();
                }
            }

            // we also need the mapping of the function args:
            auto arg_old = func.arg_begin();
            auto arg_new = func_in_module->arg_begin();

            while (arg_old != func.arg_end())
            {
                vmap.insert(std::make_pair(arg_old, arg_new));
                if (arg_new == nullptr)
                {
                    errs() << "Error mapping null:";
                    arg_old->dump();
                }
                arg_old++;
                arg_new++;
            }
            // assert same number of args
            assert(arg_new == func_in_module->arg_end());

            CloneAndPruneFunctionInto(func_in_module, &func, vmap, true, returns);
            // CloneAndPruneFunctionInto will do some optimizations on the fly
            // CloneFunctionInto(func_in_module, &func, vmap, true, returns);

            // mapping the personality function to function inside this module
            if (func.hasPersonalityFn())
            {
                Debug(errs() << "hasPersonalityFn\n";) Value *new_personality =
                    e->external_module->val_map[func.getPersonalityFn()];
                if (new_personality != nullptr && isa<Constant>(new_personality))
                {
                    func_in_module->setPersonalityFn(dyn_cast<Constant>(new_personality));
                }
                else

                    if (isa<ConstantExpr>(func.getPersonalityFn()))
                {
                    // this is constant function cast
                    // we need to insert cast of the function copy that resides inside our
                    // module
                    auto *old_cast = dyn_cast<ConstantExpr>(func.getPersonalityFn());
                    if (old_cast->isCast())
                    {
                        Instruction *inst = old_cast->getAsInstruction();
                        Value *casted_v = inst->getOperand(0);
                        Value *new_to_cast = e->external_module->val_map[casted_v];
                        if (new_to_cast != nullptr && isa<Constant>(new_to_cast))
                        {
                            func_in_module->setPersonalityFn(old_cast->getWithOperandReplaced(
                                0, dyn_cast<Constant>(new_to_cast)));
                            // delete temporary inst:
                            inst->deleteValue();
                        }
                    }
                }
                else
                {
                    errs() << "Error in handleing the personality Function for: "
                           << func.getName() << "\n";
                }
            }
        }
    }
}

void never_inline_external_functions(llvm::Module &M, environment *e)
{
    for (auto *func : e->external_module->exportedF)
    {
        func->addFnAttr(llvm::Attribute::AttrKind::NoInline);
    }
}

// inlines all calls of given function within the module
void inline_this_function(llvm::Module &M, llvm::Function *func)
{
    Debug(errs() << "Inlining " << func->getName()
                 << "\n";) for (auto *use : get_function_users(M, func->getName()))
    {
        if (auto *call = dyn_cast<CallInst>(use))
        {
            Debug(call->dump();) InlineFunctionInfo info;
            InlineFunction(call, info);
        }
    }
}

void inline_external_functions(llvm::Module &M, environment *e)
{
    for (auto *func : e->external_module->exportedF)
    {
        inline_this_function(M, func);
    }

    // e->external_module->auxiliaryF can then be inlined later if other optimizing passes want
    // to do it
}

// to use with erase remove
bool can_remove_erase_this_function(llvm::Function *func)
{
    if (func->use_empty())
    {
        Debug(errs() << "erase unused: " << func->getName() << "\n";) func->eraseFromParent();
        return true;
    }
    else
    {
        return false;
    }
}

void erase_not_used_external_functions(llvm::Module &M, environment *e)
{
    for (int i = 0; i < OMP2MPI_MAXIMUM_RTLIB_CALL_DEPTH; ++i)
    {
        e->external_module->exportedF.erase(
            std::remove_if(e->external_module->exportedF.begin(),
                           e->external_module->exportedF.end(),
                           can_remove_erase_this_function),
            e->external_module->exportedF.end());
        e->external_module->auxiliaryF.erase(
            std::remove_if(e->external_module->auxiliaryF.begin(),
                           e->external_module->auxiliaryF.end(),
                           can_remove_erase_this_function),
            e->external_module->auxiliaryF.end());
    } // This will erase most unused stuff from rtlib
    // apparently this also removes the unused string literals

    // TODO recursive functions are not removed by this
}

bool is_func_defined_in_module(module_info *mod, Function *f)
{

    for (auto *func : mod->exportedF)
    {
        if (func == f)
        {
            return true;
        }
    }

    for (auto *func : mod->auxiliaryF)
    {
        if (func == f)
        {
            return true;
        }
    }

    // if noone matches
    return false;
}

// determines if function is inserted by our pass
bool is_func_defined_by_pass(environment *e, Function *f)
{
    bool result = false;
    result = result | is_func_defined_in_module(e->external_module, f);

    // direct MPI might be considered as defined  by MPI and not by our pass

    // functions provided when including linked_list.h
    result = result | f->getName().startswith("_ZN15OMP_linked_list");
    result = result | f->getName().startswith("_ZN15MPI_linked_list");
    result = result | f->getName().startswith("_ZN11Linked_list");
    result = result | f->getName().startswith("__Z12get_new_listIiEP11Linked_listIT_Ev");

    return result;
}

// determines if function is inserted by openmp
// functions like omp_get_thread_num are considered defined by user as he explicidly uses
// them
bool is_func_defined_by_openmp(environment *e, Function *f)
{
    bool result = false;
    result = result | f->getName().startswith("__kmpc");
    result = result | f->getName().startswith(".omp_combiner.");
    result = result | f->getName().startswith(".omp.reduction.");

    // ompoutlined is considered defined by user and not by openmp
    return result;
}
