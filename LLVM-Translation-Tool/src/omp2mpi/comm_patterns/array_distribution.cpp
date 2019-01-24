#include <llvm/IR/IRBuilder.h>
//#include <llvm/Support/raw_ostream.h>

#include "../SharedArray.h"
#include "../SharedSingleValue.h"
#include "../external_functions.h"
#include "../helper.h"
#include "commPatterns.h"
#include <llvm/Support/raw_ostream.h>

#include "llvm/IR/InstIterator.h"
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/CodeExtractor.h>

#include "../SCCP.h"

using namespace llvm;

#define DEBUG_ARRAY_DISTRIBUTION 0

#if DEBUG_ARRAY_DISTRIBUTION == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

// This Communication Pattern distribute a shared array
// among the threads so that each process may only hold the part of the array that is needed

// is_comm_allowed is only used when store load is alwasy performend, so that a local buffer
// will be present for shure

// declaration of function below
void handle_usage_of_distributed_array_line(Module &M, environment *e, SharedArray *array,
                                            Value *array_info, Value *local_buffer_size,
                                            Value *line_idx, GetElementPtrInst *gep);

void handle_usage_of_distributed_array_line(Module &M, environment *e, SharedArray *array,
                                            Value *array_info, Value *local_buffer_size,
                                            Value *line_idx, LoadInst *array_ptr_load)
{
    assert(array_ptr_load->getType()->isPointerTy());

    for (auto *u : array_ptr_load->users())
    {
        if (auto *gep = dyn_cast<GetElementPtrInst>(u))
        {
            handle_usage_of_distributed_array_line(M, e, array, array_info, local_buffer_size,
                                                   line_idx, gep);
        }
        else if (auto *load = dyn_cast<LoadInst>(u)) // load of first elem in current layer
        {
            if (load->getType()->isPointerTy())
            {
                // if a ptr was loaded, it is a decent into another array layer
                handle_usage_of_distributed_array_line(M, e, array, array_info,
                                                       local_buffer_size, line_idx, load);
            }
        }
        else if (auto *store =
                     dyn_cast<StoreInst>(u)) // store to first element of current layer
        {
            assert(!store->getValueOperand()->getType()->isPointerTy() &&
                   "Do not rearrange the shared array");
            if (is_comm_allowed(e, store))
            {
                if (array->get_comm_function_on_store(M, e))
                {
                    IRBuilder<> builder(store->getNextNode());
                    Value *store_location_to_void = builder.CreateBitCast(
                        store->getPointerOperand(), builder.getInt8PtrTy());

                    builder.CreateCall(
                        array->get_comm_function_on_store(M, e),
                        {array_info, local_buffer_size, line_idx, store_location_to_void});
                }
            }
        }
        else
        {
            errs() << "This Operation within a shared array is not supported yet\n";
            u->dump();
        }
    }
}

void handle_usage_of_distributed_array_line(Module &M, environment *e, SharedArray *array,
                                            Value *array_info, Value *local_buffer_size,
                                            Value *line_idx, GetElementPtrInst *gep)
{
    for (auto *u : gep->users())
    {
        if (auto *load = dyn_cast<LoadInst>(u))
        {
            if (load->getType()->isPointerTy())
            {
                // if a ptr was loaded, it is a decent into another array layer
                handle_usage_of_distributed_array_line(M, e, array, array_info,
                                                       local_buffer_size, line_idx, load);
            }
        }
        else if (auto *store = dyn_cast<StoreInst>(u))
        {
            assert(!store->getValueOperand()->getType()->isPointerTy() &&
                   "Do not rearrange the shared array");
            if (is_comm_allowed(e, store))
            {
                IRBuilder<> builder(store->getNextNode());
                if (array->get_comm_function_on_store(M, e))
                {
                    Value *store_location_to_void = builder.CreateBitCast(
                        store->getPointerOperand(), builder.getInt8PtrTy());
                    builder.CreateCall(
                        array->get_comm_function_on_store(M, e),
                        {array_info, local_buffer_size, line_idx, store_location_to_void});
                }
            }
        }
        else
        {
            errs() << "This Operation within a shared array is not supported yet\n";
            u->dump();
        }
    }
}

// TODO weiter Verallgemeinern?
// im moment ist die Beschränkung für nuen Komm-Muster
// load Funktionen nehmen die Parameter buffer, buffer_size(=sizeof (ptr)), idx
//     Sie werden 1mal pro erser Dimension aufgerufen, egal wie Viele Load
// store Funktionen nehmen die Parameter buffer, buffer_size(=sizeof (ptr)), idx,
// location_of_store
//     Sie werden einmal pro Element aufgerufen

// Adds MPI communication to shared variables of the shared array pointer type
void distribute_array_array_ptr_var(Module &M, environment *e,
                                    ParallelFunction *microtask_object, SharedArray *array)
{
    IRBuilder<> builder(M.getContext());
    Value *const0 = builder.getInt64(0);

    Value *var = array->value();

    builder.SetInsertPoint(
        get_init_block(M, e, microtask_object->get_function())->getTerminator());

    Value *array_info = builder.CreateBitCast(var, builder.getInt8PtrTy());

    Value *local_buffer_size = array->get_local_buffer_size(M, e);

    for (auto *u : var->users())
    {
        auto *inst = dyn_cast<Instruction>(u);
        if (u != array_info && is_comm_allowed(e, inst) &&
            inst->getFunction() == microtask_object->get_function())
        {
            if (auto *load = dyn_cast<LoadInst>(u))
            {
                builder.SetInsertPoint(load);
                // get the array ptr from array info struct
                // is just the load ptr form first position
                // therefore it does NOT NEED TO CHANGE the openmp code!
                // as first instruction has to be the load of struct ptr anyway

                // chase the usage of shared array element/line/plane/cube/...
                // (first layer)
                for (auto *array_u : load->users())
                {
                    if (auto *gep = dyn_cast<GetElementPtrInst>(array_u))
                    {
                        assert(gep->getNumIndices() == 1 &&
                               "Only one idx should be used at this level\n");
                        auto idx_iter = gep->idx_begin();
                        Value *idx = dyn_cast<Value>(idx_iter);
                        if (array->get_comm_function_on_load(M, e))
                        {
                            builder.SetInsertPoint(gep);
                            builder.CreateCall(array->get_comm_function_on_load(M, e),
                                               {array_info, local_buffer_size, idx});
                        }
                        handle_usage_of_distributed_array_line(M, e, array, array_info,
                                                               local_buffer_size, idx, gep);
                    }
                    else if (auto *firstelem_load = dyn_cast<LoadInst>(array_u))
                    {
                        if (array->get_comm_function_on_load(M, e))
                        {
                            builder.SetInsertPoint(firstelem_load);
                            builder.CreateCall(array->get_comm_function_on_load(M, e),
                                               {array_info, local_buffer_size, const0});
                        }
                        // if it is a decent into the forst line
                        if (firstelem_load->getType()->isPointerTy())
                        {
                            handle_usage_of_distributed_array_line(
                                M, e, array, array_info, local_buffer_size, const0, load);
                        }
                    }
                    else if (auto *firstelem_store = dyn_cast<StoreInst>(array_u))
                    {
                        assert(!firstelem_store->getValueOperand()->getType()->isPointerTy() &&
                               "Do not rearrange the shared array");
                        if (is_comm_allowed(e, firstelem_store))
                        {
                            builder.SetInsertPoint(firstelem_store->getNextNode());
                            if (array->get_comm_function_on_store(M, e))
                            {
                                Value *store_location_to_void =
                                    builder.CreateBitCast(firstelem_store->getPointerOperand(),
                                                          builder.getInt8PtrTy());
                                builder.CreateCall(array->get_comm_function_on_store(M, e),
                                                   {array_info, local_buffer_size, const0,
                                                    store_location_to_void});
                            }
                        }
                    }
                    else
                    {
                        if (array->get_comm_function_on_load(M, e))
                        {
                            // to make shure storage location exists:
                            builder.SetInsertPoint(dyn_cast<Instruction>(array_u));
                            builder.CreateCall(e->functions->cache_shared_array_line,
                                               {array_info, local_buffer_size, const0});
                        }
                        errs() << "Why you do this Operation on a shared Array ptr? it is "
                                  "not supported\n";
                        array_u->dump();
                    }
                }
            }
            else
            { // no load inst
                errs() << "Why you do this Operation on a shared Array ptr? it is not "
                          "supported\n";
                u->dump();
            }
        }
    }

    // before each syncronizatzion Point (barrier): invalidate array caches
    // I think it does not justify the implementation effort to fetch the status wehter the
    // remote line has changed, compared to just fetching the whole line again (if needed)
    // TODO are there other syncronization points when we need to invalidate caches?
    auto barriers = get_function_users(M, e->functions->mpi_barrier->getName());
    for (auto *u : barriers)
    {
        auto *inst = dyn_cast<Instruction>(u);
        if (inst->getFunction() == microtask_object->get_function())
        {
            if (array->get_comm_function_on_sync(M, e))
            {
                builder.SetInsertPoint(inst);
                builder.CreateCall(array->get_comm_function_on_sync(M, e),
                                   {array_info, local_buffer_size});
            }
        }
    }
}

void distribute_array_comm_Pattern(llvm::Module &M, environment *e,
                                   ParallelFunction *microtask_func)
{
    for (auto *var : microtask_func->get_shared_vars())
    {
        if (auto *array = dyn_cast<SharedArray>(var))
        {
            distribute_array_array_ptr_var(M, e, microtask_func, array);
        }
        if (auto *single_value_var = dyn_cast<SharedSingleValue>(var))
        {
            default_single_value_var(M, e, microtask_func, single_value_var);
        }
    }
}

std::pair<llvm::Value *, llvm::Value *> distribute_shared_dynamic_array_var_from_master(
    llvm::Module &M, environment *e, llvm::CallInst *insert_before_master,
    llvm::CallInst *insert_before_worker, SharedArray *array_to_bcast)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    Value *array_base_to_void =
        master_builder.CreateBitCast(array_to_bcast->value(), void_ptr_type->getPointerTo());
    Value *elem_size = array_to_bcast->get_base_type_size(M, e);

    Value *dim = array_to_bcast->get_dimension(M, e);

    StructType *info_struct_type = array_to_bcast->get_comm_info_type(M, e);
    Value *local_buffer_size =
        array_to_bcast->get_local_buffer_size(M, e); // here: size of pointer

    Value *master_array_info = master_builder.CreateAlloca(info_struct_type);
    Value *worker_array_info = worker_builder.CreateAlloca(info_struct_type);
    Value *master_array_info_void =
        master_builder.CreateBitCast(master_array_info, void_ptr_type);
    Value *worker_array_info_void =
        worker_builder.CreateBitCast(worker_array_info, void_ptr_type);
    Value *worker_orig_type = worker_builder.CreateBitCast(
        worker_array_info,
        array_to_bcast->getType()); // cast to original type to pass it to microtask func
    Value *master_orig_type =
        master_builder.CreateBitCast(master_array_info, array_to_bcast->getType());

    // master: set array ptr
    Value *val_before = master_builder.CreateLoad(array_to_bcast->value());
    master_builder.CreateStore(val_before, master_orig_type);

    std::vector<Value *> master_args = {master_array_info_void, local_buffer_size, dim,
                                        elem_size};
    std::vector<Value *> worker_args = {worker_array_info_void, local_buffer_size, dim,
                                        elem_size};

    if (array_to_bcast->get_comm_function_on_init(M, e) != nullptr)
    {
        master_builder.CreateCall(array_to_bcast->get_comm_function_on_init(M, e),
                                  master_args);
        worker_builder.CreateCall(array_to_bcast->get_comm_function_on_init(M, e),
                                  worker_args);
    }

    // insert calls to free
    master_builder.SetInsertPoint(insert_before_master->getNextNode());
    worker_builder.SetInsertPoint(insert_before_worker->getNextNode());

    if (array_to_bcast->get_comm_function_on_finish(M, e) != nullptr)
    {
        master_builder.CreateCall(array_to_bcast->get_comm_function_on_finish(M, e),
                                  {master_array_info_void, local_buffer_size});
        worker_builder.CreateCall(array_to_bcast->get_comm_function_on_finish(M, e),
                                  {worker_array_info_void, local_buffer_size});
    }

    // load value after Parallel
    // if DESTROY_GLOBAL_ARRAY_IN_PARALLEL_REGION in rtlib.h was not set, this instructions
    // will have no effect (it will just load the same value as before)
    Value *var_after = master_builder.CreateLoad(master_orig_type);
    master_builder.CreateStore(var_after, array_to_bcast->value());

    return std::make_pair(master_orig_type, worker_orig_type);
}

std::vector<llvm::Value *>
distribute_shared_vars_from_master(llvm::Module &M, environment *e, ForkEntry *mikrotask,
                                   llvm::CallInst *insert_before_master,
                                   llvm::CallInst *insert_before_worker,
                                   std::vector<llvm::Value *> *args_to_bcast)
{
    Type *void_ptr_type = Type::getInt8PtrTy(M.getContext());
    IRBuilder<> master_builder(insert_before_master);
    IRBuilder<> worker_builder(insert_before_worker);

    std::vector<llvm::Value *> args_for_workers;
    // bcast all arguments given
    for (unsigned int i = 0; i < args_to_bcast->size(); ++i)
    {
        Value *var = (*args_to_bcast)[i];

        SharedVariable *microtask_parent_var = mikrotask->get_sharedVar_for_arg(i);
        if (microtask_parent_var != nullptr)
        {
            microtask_parent_var = microtask_parent_var->getOrigin();
        }

        if (isa<GlobalVariable>(var))
        { // all globals are bcasted below
            if (microtask_parent_var !=
                nullptr) // if for whatever reason something like a string literal is passed
            {
                assert(microtask_parent_var->value() == var &&
                       "Error in shared Variable matching");
            }
            args_for_workers.push_back(var);
        }
        else if (isa<Constant>(var) && !isa<GlobalVariable>(var))
        { // global is derived from constant
            // no need to bcast a constant
            args_for_workers.push_back(var);
        }
        else
        {
            std::pair<Value *, Value *> new_master_worker_pair;

            if (var->getType()->isPointerTy())
            { // if not ptr: it is a private var

                if (var->getType()->getPointerElementType()->isStructTy())
                {
                    // structs currently have special handeling
                    new_master_worker_pair = bcast_shared_struct_var_from_master(
                        M, e, insert_before_master, insert_before_worker, var);
                }
                else
                {
                    assert(microtask_parent_var->value() == var &&
                           "Error in shared Variable matching");
                }

                if (auto *array = dyn_cast<SharedArray>(microtask_parent_var))
                {
                    new_master_worker_pair = distribute_shared_dynamic_array_var_from_master(
                        M, e, insert_before_master, insert_before_worker, array);
                }

                else if (auto *single = dyn_cast<SharedSingleValue>(microtask_parent_var))
                {
                    {
                        new_master_worker_pair = bcast_shared_single_value_var_from_master(
                            M, e, insert_before_master, insert_before_worker, single);
                    }
                }
            }
            else // no pointer type
            {
                new_master_worker_pair = bcast_firstprivate_single_value_var_from_master(
                    M, e, insert_before_master, insert_before_worker, var);
            }

            assert(new_master_worker_pair.first != nullptr);
            assert(new_master_worker_pair.second != nullptr);
            (*args_to_bcast)[i] = new_master_worker_pair.first;
            args_for_workers.push_back(new_master_worker_pair.second);
        }
    }

    bcast_all_globals(M, e, insert_before_master, insert_before_worker);

    return args_for_workers;
}

// return nullptr if idx was not found
// sets idx_cmp as output
Value *get_loop_idx(Loop *loop, ICmpInst **idx_cmp)
{
    auto header = loop->getHeader();

    auto term = header->getTerminator();
    if (!isa<BranchInst>(term))
    {
        return nullptr;
    }
    auto *cond = dyn_cast<BranchInst>(term)->getCondition();

    if (!isa<ICmpInst>(cond))
    {
        return nullptr;
    }
    auto *cmp = dyn_cast<ICmpInst>(cond);

    *idx_cmp = cmp;
    // find phi in cmp operators

    Value *idx = nullptr;

    for (auto *op : cmp->operand_values())
    {
        if (isa<PHINode>(op))
        {
            if (idx != nullptr)
            {
                // then idx is not unique
                return nullptr;
            }
            idx = op;
        }
    }

    if (idx != nullptr && idx->getType()->isIntegerTy())
    {

        return idx;
    }
    else
    {
        return nullptr;
    }
}

bool fill_usage_of_array_vector(Module &M, environment *e, std::vector<Value *> array_infos,
                                std::vector<CallInst *> *aray_usage)
{
    for (auto *array_info : array_infos)
    {
        for (auto *u : array_info->users())
        {
            if (auto *call = dyn_cast<CallInst>(u))
            {
                if (call->getCalledFunction() == e->functions->cache_shared_array_line ||
                    call->getCalledFunction() == e->functions->store_to_shared_array_line)
                {
                    aray_usage->push_back(call);
                }
                else
                {
                    // WHY SHOULD THIS EVER HAPPEN
                    call->dump();
                    return false;
                }
            }
            else
            {
                // might be a cast to pass it to a user defined function
                // this means we will not optimize loop
                u->dump();
                return false;
            }
        }
    }
    return true;
}

// if defined the loop will be doubled (one with stores and one without) may generate
// significantly more code

//#define ALSO_EXTRACT_STORES

// returns a pair of function calls to inline when one retruns to upper lvl
// but inlining will break all ptrs in current lvl
std::pair<CallInst *, CallInst *>
extract_stores_form_loop(Module &M, environment *e, Function *extracted_loop,
                         std::vector<Value *> array_infos,
                         std::vector<CallInst *> uses_independent_from_idx)
{
    // set condition that check if for all used idx in store holds that row is own
    // call loop without comm if true with comm if false
    // check for all in order to not explode code size

    // for ease of implementation: extract the loop again and duplicate it
    // as this will not extract and duplicate the already handled load of array lines

    bool has_stores = false;
    for (auto *call : uses_independent_from_idx)
    {
        if (call->getCalledFunction() == e->functions->store_to_shared_array_line)
        {
            has_stores = true;
            break;
        }
    }

    if (!has_stores)
    {
        return std::make_pair(nullptr, nullptr); // nothing to do
    }
    // get the dominatortree of the current function
    DominatorTree *DT = new DominatorTree();
    DT->recalculate(*extracted_loop);
    // generate the LoopInfoBase for the current function
    LoopInfo *Loop = new LoopInfo(*DT);

    int optimized = false;
    Loop->releaseMemory();
    Loop->analyze(*DT);
    auto *L = Loop->getLoopsInPreorder()[0];

    IRBuilder<> builder(extracted_loop->getEntryBlock().getTerminator());

    Value *condidion = builder.getInt1(true);
    // buid condition th decide wether to use comm
    for (auto *call : uses_independent_from_idx)
    {
        if (call->getCalledFunction() == e->functions->store_to_shared_array_line)
        {
            Value *is_this_own_line = builder.CreateCall(
                e->functions->is_array_row_own,
                {call->getArgOperand(0), call->getArgOperand(1), call->getArgOperand(2)});
            condidion = builder.CreateAnd(condidion, is_this_own_line);
        }
    }
    // if condidion=true, all accessed arrays are in own

    // extract only the loop part
    CodeExtractor *extractor = new CodeExtractor(*DT, *L);
    assert(extractor->isEligible());
    Function *loop_with_comm = extractor->extractCodeRegion(); // comm is removed later
    llvm::ValueToValueMapTy vmap;
    Function *loop_NO_comm = CloneFunction(loop_with_comm, vmap);

    for (auto *call : uses_independent_from_idx)
    {
        if (call->getCalledFunction() == e->functions->store_to_shared_array_line)
        {
            dyn_cast<Instruction>(vmap[call])->eraseFromParent(); // remove the comm
            // remove it from teh copied function, so that original ptr will remain valid
        }
    }

    // find call in original extracted loop
    CallInst *original_call_with_comm = nullptr;
    for (auto *u : loop_with_comm->users())
    {
        original_call_with_comm = dyn_cast<CallInst>(u);
    }
    assert(original_call_with_comm != nullptr);

    assert(original_call_with_comm->getParent()->getTerminator()->getNumSuccessors() == 1);

    auto *last_block = original_call_with_comm->getParent()->getTerminator()->getSuccessor(0);
    // this Block only contains the retrun

    Instruction *br = original_call_with_comm->getNextNode();
    assert(isa<BranchInst>(br) && !dyn_cast<BranchInst>(br)->isConditional() &&
           dyn_cast<BranchInst>(br)->getSuccessor(0) == last_block);
    br->eraseFromParent(); // will be replaced with conditional br below
    builder.SetInsertPoint(original_call_with_comm);

    BasicBlock *true_block =
        BasicBlock::Create(M.getContext(), "loop_NO_comm", extracted_loop);
    BasicBlock *false_block =
        BasicBlock::Create(M.getContext(), "loop_with_comm", extracted_loop);

    builder.CreateCondBr(condidion, true_block, false_block);

    builder.SetInsertPoint(true_block);
    Instruction *true_insert = builder.CreateBr(last_block);

    builder.SetInsertPoint(false_block);
    Instruction *false_insert = builder.CreateBr(last_block);

    CallInst *call_No_comm = dyn_cast<CallInst>(original_call_with_comm->clone());
    original_call_with_comm->removeFromParent();
    original_call_with_comm->insertBefore(false_insert);
    call_No_comm->setCalledFunction(loop_NO_comm);
    call_No_comm->insertBefore(true_insert);

    return std::make_pair(call_No_comm, original_call_with_comm);
}

// extrtact the load of the line when its idx is independent from array
std::pair<CallInst *, CallInst *>
optimize_non_dependent_array_idx(Module &M, environment *e, Function *extracted_loop,
                                 std::vector<Value *> array_infos,
                                 std::vector<CallInst *> uses_independent_from_idx)
{
    BasicBlock *entryB = &extracted_loop->getEntryBlock();
    if (!uses_independent_from_idx.empty())
    {
        Debug(
            errs() << "Moving load of array lines one lvl up";) for (auto *call :
                                                                     uses_independent_from_idx)
        {
            if (call->getCalledFunction() == e->functions->cache_shared_array_line)
            {
                if (!isa<Argument>(
                        call->getArgOperand(2))) // also move instruction that is idx
                {
                    Instruction *inst = dyn_cast<Instruction>(call->getArgOperand(2));
                    inst->removeFromParent();
                    inst->insertBefore(entryB->getFirstNonPHI());
                }
                // just move it before the loop
                call->removeFromParent();
                call->insertBefore(entryB->getTerminator());
            }
#ifdef ALSO_EXTRACT_STORES
            else if (call->getCalledFunction() == e->functions->store_to_shared_array_line)
            { // move the idx to top
                if (!isa<Argument>(
                        call->getArgOperand(2))) // also move instruction that is idx
                {
                    Instruction *inst = dyn_cast<Instruction>(call->getArgOperand(2));
                    inst->removeFromParent();
                    inst->insertBefore(entryB->getFirstNonPHI());
                }
            }
#endif
        }

#ifdef ALSO_EXTRACT_STORES
        return extract_stores_form_loop(M, e, extracted_loop, array_infos,
                                        uses_independent_from_idx);
#else
        return std::make_pair(nullptr, nullptr);
#endif
    }
    return std::make_pair(nullptr, nullptr);
}

void split_this_loop(Module &M, environment *e, Function *extracted_loop, Value *start_idx,
                     Value *stop_idx, CallInst *loop_call, std::vector<Value *> array_infos,
                     std::vector<std::pair<long, CallInst *>> uses_dependent_from_idx,
                     bool inline_again, bool uses_equal_comparision)
{
    Debug(errs() << "Split the loop to remove communication from strictly local part\n";)
        assert(!uses_dependent_from_idx.empty());

    IRBuilder<> builder(loop_call);
    // TODO maybe get the right size here (and not hardcode it to ptr)
    Value *buffer_elem_size = builder.getInt64(get_size_in_Byte(M, builder.getInt8PtrTy()));
    // needed to acces the array_info struct (is sizeof pointer for array)

    // copy the loop again this version will have communication:
    llvm::ValueToValueMapTy vmap;
    Function *loop_with_comm = CloneFunction(extracted_loop, vmap);
    Function *loop_NO_comm = extracted_loop; // just for code readability

    long min_idx_offset = std::numeric_limits<long>::max();
    long max_idx_offset = std::numeric_limits<long>::min();

    for (auto pair : uses_dependent_from_idx)
    {
        min_idx_offset = min_idx_offset < pair.first ? min_idx_offset : pair.first;
        max_idx_offset = max_idx_offset > pair.first ? max_idx_offset : pair.first;
        // remove communication in original outlined loop
        pair.second->eraseFromParent(); // erases the comm in loop_NO_comm function
    }

    // now split the loop into three parts:
    // first part: WITH COMM from start to < min((own_upper+ abs(min_offset),end))
    // second part: NO COMM from max((own_upper+ abs(min_offset)),start) to
    // min((own_lower-abs(max_offset)),end)
    // third part: WITH COMM from max((own_lower-abs(max_offset)),start) to end

    // if some of the parts will have an invalid range than the loop is not executed
    // (because first loop condition check will fail)

    // this will also work if compiler hase done index transformation before (offset might
    // be higher then)
    bool is_start_ptr = start_idx->getType()->isPointerTy();
    bool is_stop_ptr = stop_idx->getType()->isPointerTy();

    Value *start = start_idx;
    if (is_start_ptr)
    {
        start = builder.CreateLoad(start);
    }
    assert(!start->getType()->isPointerTy());
    Value *end = stop_idx;
    if (is_stop_ptr)
    {
        end = builder.CreateLoad(end);
    }
    assert(!end->getType()->isPointerTy());
    assert(start->getType() == end->getType());

    // get lowest own range (in order to not explode code size, we do not differentiate
    // between arrays)

    auto iter = array_infos.begin();
    Value *this_array_info = *iter;
    Value *own_lower = builder.CreateCall(e->functions->get_own_lower_array_line,
                                          {this_array_info, buffer_elem_size});
    Value *own_upper = builder.CreateCall(e->functions->get_own_upper_array_line,
                                          {this_array_info, buffer_elem_size});
    ++iter;
    // need to extract this from loop so that code will work if only one array present
    for (; iter != array_infos.end(); iter++) // loop is initialized above
    {
        this_array_info = *iter;
        Value *this_array_lower = builder.CreateCall(e->functions->get_own_lower_array_line,
                                                     {this_array_info, buffer_elem_size});
        Value *this_array_upper = builder.CreateCall(e->functions->get_own_upper_array_line,
                                                     {this_array_info, buffer_elem_size});
        // find max for upper
        Value *cmp_max =
            builder.CreateICmp(CmpInst::Predicate::ICMP_SGE, own_upper, this_array_upper);
        own_upper = builder.CreateSelect(cmp_max, own_upper, this_array_upper);

        // and min for lower
        Value *cmp_min =
            builder.CreateICmp(CmpInst::Predicate::ICMP_SLE, own_lower, this_array_lower);
        own_lower = builder.CreateSelect(cmp_min, own_lower, this_array_lower);
    }

    // this means own loxer and own upper will hold the range where all arrays lines are
    // present on this rank this code should work if one want to change the distributioon
    // of rows as long as rows are distibuted continouus

    Value *min_offset = builder.getInt64(abs(min_idx_offset));
    Value *max_offset = builder.getInt64(abs(max_idx_offset));
    Value *own_upper_and_min = builder.CreateAdd(own_upper, min_offset);
    Value *own_lower_and_max = builder.CreateSub(own_lower, max_offset);

    if (start->getType() == builder.getInt32Ty())
    { // also use the int32 part of the colculation result
        own_upper_and_min =
            builder.CreateTruncOrBitCast(own_upper_and_min, builder.getInt32Ty());
        own_lower_and_max =
            builder.CreateTruncOrBitCast(own_lower_and_max, builder.getInt32Ty());
    }
    else
    {
        assert(start->getType() == builder.getInt64Ty());
    }

    Value *first_start = start;

    Value *min_cmp1 = builder.CreateICmp(CmpInst::Predicate::ICMP_SLE, end, own_upper_and_min);
    Value *first_end = builder.CreateSelect(
        min_cmp1, end, own_upper_and_min); //=min((own_upper+ abs(min_offset),end))

    Value *max_cmp1 =
        builder.CreateICmp(CmpInst::Predicate::ICMP_SGE, start, own_upper_and_min);
    Value *second_start = builder.CreateSelect(
        max_cmp1, start, own_upper_and_min); //=max((own_upper+ abs(min_offset),start))

    Value *min_cmp2 = builder.CreateICmp(CmpInst::Predicate::ICMP_SLE, end, own_lower_and_max);
    Value *second_end = builder.CreateSelect(
        min_cmp2, end, own_lower_and_max); //=min((own_lower-abs(max_offset)),end)

    Value *max_cmp2 =
        builder.CreateICmp(CmpInst::Predicate::ICMP_SGE, start, own_lower_and_max);
    Value *third_start = builder.CreateSelect(
        max_cmp2, start, own_lower_and_max); //=max((own_lower-abs(max_offset)),start)

    Value *third_end = end;

    if (uses_equal_comparision)
    {
        // shrink the middle range where no communication is so that it does not overlap
        // shrinking the region with no comm is safe, as the other regoins will have the
        // otehr loop iterations with communication if needed
        second_start =
            builder.CreateAdd(second_start, ConstantInt::get(second_start->getType(), 1));
        second_end = builder.CreateSub(second_end, ConstantInt::get(second_end->getType(), 1));

        // if the middle now contains 0 elements: we have to adjust the boundaries for one
        // of the other region to be shure no overlap occurs
        Value *third_start_incrm =
            builder.CreateAdd(third_start, ConstantInt::get(third_start->getType(), 1));
        Value *is_eq = builder.CreateICmpEQ(first_end, third_start);
        third_start = builder.CreateSelect(is_eq, third_start_incrm, third_start);
    }

    int arg_pos_of_start = -1;
    int arg_pos_of_stop = -1;
    std::vector<Value *> args_for_part_1;
    std::vector<Value *> args_for_part_2;
    std::vector<Value *> args_for_part_3;

    for (unsigned int i = 0; i < loop_call->getNumArgOperands(); ++i)
    {
        if (loop_call->getArgOperand(i) == start_idx)
        {
            arg_pos_of_start = i;
        }
        if (loop_call->getArgOperand(i) == stop_idx)
        {
            arg_pos_of_stop = i;
        }
        args_for_part_1.push_back(loop_call->getArgOperand(i));
        args_for_part_2.push_back(loop_call->getArgOperand(i));
        args_for_part_3.push_back(loop_call->getArgOperand(i));
    }
    assert(arg_pos_of_start != -1);
    assert(arg_pos_of_stop != -1);

    Debug(if (start->getType() == builder.getInt32Ty()) {
        builder.CreateCall(e->functions->print_int, {first_start});
        builder.CreateCall(e->functions->print_int, {first_end});
        builder.CreateCall(e->functions->print_int, {second_start});
        builder.CreateCall(e->functions->print_int, {second_end});
        builder.CreateCall(e->functions->print_int, {third_start});
        builder.CreateCall(e->functions->print_int, {third_end});
    } else {
        builder.CreateCall(e->functions->print_long, {first_start});
        builder.CreateCall(e->functions->print_long, {first_end});
        builder.CreateCall(e->functions->print_long, {second_start});
        builder.CreateCall(e->functions->print_long, {second_end});
        builder.CreateCall(e->functions->print_long, {third_start});
        builder.CreateCall(e->functions->print_long, {third_end});
    })

        // now create the calls to the three parts of the loop
        if (is_start_ptr)
    {
        builder.CreateStore(first_start, start_idx);
    }
    else { args_for_part_1[arg_pos_of_start] = first_start; }
    if (is_stop_ptr)
    {
        builder.CreateStore(first_end, stop_idx);
    }
    else
    {
        args_for_part_1[arg_pos_of_stop] = first_end;
    }
    CallInst *call1 = dyn_cast<CallInst>(builder.CreateCall(loop_with_comm, args_for_part_1));
    // 2 (no comm)
    if (is_start_ptr)
    {
        builder.CreateStore(second_start, start_idx);
    }
    else
    {
        args_for_part_2[arg_pos_of_start] = second_start;
    }
    if (is_stop_ptr)
    {
        builder.CreateStore(second_end, stop_idx);
    }
    else
    {
        args_for_part_2[arg_pos_of_stop] = second_end;
    }
    CallInst *call2 = dyn_cast<CallInst>(builder.CreateCall(loop_NO_comm, args_for_part_2));
    // 3
    if (is_start_ptr)
    {
        builder.CreateStore(third_start, stop_idx);
    }
    else
    {
        args_for_part_3[arg_pos_of_start] = third_start;
    }
    if (is_stop_ptr)
    {
        builder.CreateStore(third_end, stop_idx);
    }
    else
    {
        args_for_part_3[arg_pos_of_stop] = third_end;
    }
    CallInst *call3 = dyn_cast<CallInst>(builder.CreateCall(loop_with_comm, args_for_part_3));

    // reset value of loop start idxif nbeeded
    if (is_start_ptr)
    {
        builder.CreateStore(start, start_idx);
    }
    // nothing to do for stop it contains the correct value (we checked that it does not
    // change within the loop)

    // DEBUG:
    // loop_with_comm->dump();
    // loop_NO_comm->dump();
    // loop_call->getParent()->dump();
    // ASK_TO_CONTINIUE

    loop_call->eraseFromParent(); // not longer needed

    // we let later optimization passes decide if inlining of the loops is a good choice
    if (inline_again)
    { // Needed for deeper lvl loops
        InlineFunctionInfo info;
        InlineFunction(call1, info);
        InlineFunctionInfo info2;
        InlineFunction(call2, info2);
        InlineFunctionInfo info3;
        InlineFunction(call3, info3);
        // as they wehere inlined again:
        loop_with_comm->eraseFromParent();
        loop_NO_comm->eraseFromParent();
    }
}

// retuns nullptr if it is too complex to fuigure out loop start idx
Value *find_start_idx(Module &M, environment *e, Function *extracted_loop, Value *idx)
{
    assert(isa<PHINode>(idx));
    PHINode *idx_phi = dyn_cast<PHINode>(idx);

    auto &entryB = extracted_loop->getEntryBlock();

    int blk_idx = idx_phi->getBasicBlockIndex(&entryB);
    if (blk_idx == -1)
    { // not found
        return nullptr;
    }

    Value *incoming = idx_phi->getIncomingValueForBlock(&entryB);

    if (auto *load = dyn_cast<LoadInst>(incoming))
    {

        Value *start_idx_ptr = load->getPointerOperand();
        if (isa<Argument>(start_idx_ptr))
        {
            // to be safe we assert that it is only used in this load
            for (auto *u : start_idx_ptr->users())
            {
                if (u != load)
                {
                    return nullptr;
                }
            }
            return start_idx_ptr;
        }
        else
        {
            return nullptr;
        }
    }
    if (isa<Argument>(incoming))
    {
        return incoming;
    }
    if (isa<Constant>(incoming))
    {
        return incoming;
    }

    return nullptr; // not found
}

// only use it when loop is extracted function
Value *find_stop_idx(Module &M, environment *e, ICmpInst *loop_cmp, Value *idx)
{
    Value *idx_canidate = nullptr;

    assert(loop_cmp->getNumOperands() == 2); // cmp should always have 2 operands
    if (loop_cmp->getOperand(0) == idx)      // the max_loop_idx is the other operand
    {
        idx_canidate = loop_cmp->getOperand(1);
    }
    else
    {
        idx_canidate = loop_cmp->getOperand(0);
    }

    if (auto *load = dyn_cast<LoadInst>(idx_canidate))
    {

        Value *stop_idx_ptr = load->getPointerOperand();
        if (isa<Argument>(stop_idx_ptr))
        {
            // to be safe we assert that it is only used in this load
            for (auto *u : stop_idx_ptr->users())
            {
                if (u != load)
                {
                    return nullptr;
                }
            }
            return stop_idx_ptr;
        }
        else
        {
            return nullptr;
        }
    }
    if (isa<Argument>(idx_canidate))
    {
        return idx_canidate;
    }
    if (isa<Constant>(idx_canidate))
    {
        return idx_canidate;
    }

    return nullptr; // not found
}

bool does_call_use_array(Module &M, environment *e, std::vector<SharedVariable *> array_ptrs,
                         CallInst *call)
{
    for (auto *this_array_ptr : array_ptrs)
    {
        if (isa<GlobalValue>(this_array_ptr->value()))
        {
            // not shure if calee actually use the array but safe is safe
            return true;
        }
        for (unsigned int i = 0; i < call->getNumArgOperands(); ++i)
        {
            if (this_array_ptr->value() == call->getArgOperand(i))
            {
                return true;
            }
        }
    }
    // passed all above
    return false;
}

bool check_if_loop_calls_other_functions(Module &M, environment *e,
                                         std::vector<SharedVariable *> array_ptrs, Loop *L)
{
    for (auto BB : L->blocks())
    {
        for (auto I = BB->begin(); I != BB->end(); ++I)
        {

            if (CallInst *call = dyn_cast<CallInst>(I))
            {
                if (!is_func_defined_by_pass(e, call->getCalledFunction()))
                {
                    if (does_call_use_array(M, e, array_ptrs, call))
                    {
                        return true; // we do not optimize loops that branch out
                    }
                }
            }
            if (InvokeInst *inv = dyn_cast<InvokeInst>(I))
            {
                // TODO one may stll optimize it if callee uses no of the arrays
                //  therefore one must also check that arrays are not globals
                return true; // we do not optimize loops that branch out
            }
        }
    }

    return false; // if passed above;
}

// declare function defined below
int optimize_this_loop(Module &M, environment *e, Function *F,
                       std::vector<Value *> array_infos, DominatorTree *DT, Loop *loop,
                       bool inline_again);

int rec_optimize_loop_lvl(Module &M, environment *e, Function *function_with_loops,
                          std::vector<Value *> array_infos)
{
    // get the dominatortree of the current function
    DominatorTree *DT = new DominatorTree();
    DT->recalculate(*function_with_loops);
    // generate the LoopInfoBase for the current function
    LoopInfo *Loop = new LoopInfo(*DT);

    int optimized = false;
    Loop->releaseMemory();
    Loop->analyze(*DT);

    for (auto *L : Loop->getLoopsInPreorder())
    {
        if (L->getLoopDepth() ==
            2) // lvl 1 means the extracted loop but we want to decent one lvl
        {
            optimized +=
                optimize_this_loop(M, e, function_with_loops, array_infos, DT, L, true);
        }
    }
    return optimized;
}

// return flag to indicate wether an optimization was performed
int optimize_this_loop(Module &M, environment *e, Function *F,
                       std::vector<Value *> array_infos, DominatorTree *DT, Loop *loop,
                       bool inline_again)
{
    Debug(errs() << "Considering loop\n"; loop->dump();)

        int opt_in_lower_lvl = 0;
    ICmpInst *idx_cmp;
    Value *idx_val = get_loop_idx(loop, &idx_cmp); // sets idx_cmp for furtehr analysis

    if (idx_val == nullptr)
    {
        Debug(errs() << "Not Found loop idx, will not optimize it\n";) return opt_in_lower_lvl;
    }
    // if loop idx was found:

    // extract loop
    CodeExtractor *extractor = new CodeExtractor(*DT, *loop);
    if (!extractor->isEligible())
    {
        Debug(errs() << "Cannot outline loop\n";) return opt_in_lower_lvl;
    }

    Function *extractedLoop = extractor->extractCodeRegion();
    // the refernce to idx automatically "update" as the instruction is moved

    // get Value* array_info in extracted loop function
    // this must be a parameter of function if not the array is not used in this function
    // and we can stop
    CallInst *extracted_loop_call = nullptr;
    for (auto *array_info : array_infos)
    { // need to get loop call regardless which array is used within the loop
        for (auto *u : array_info->users())
        {
            if (auto *call = dyn_cast<CallInst>(u))
            {
                if (call->getCalledFunction() == extractedLoop)
                {
                    extracted_loop_call = call;
                    break;
                }
            }
        }
    }

    if (extracted_loop_call == nullptr)
    {
        Debug(errs() << "Loop does not use array (nothing to do)\n";)
            // inline the extracted loop again
            inline_this_function(M, extractedLoop);
        return opt_in_lower_lvl;
    }

    if (!extractedLoop->getReturnType()->isVoidTy())
    {
        Debug(errs() << "Extracted Loop has return value, this is not supported\n";)
            // inline the extracted loop again
            inline_this_function(M, extractedLoop);
        return opt_in_lower_lvl;
    }

    // get index of argument that is array_info

    std::vector<int> num_array_infos_arg;
    // actually the mapping which arrayinfo belongs to which arg does not matter ATM
    for (auto this_array_info : array_infos)
    {
        int num_this_array_info_arg = -1;
        for (unsigned int i = 0; i < extracted_loop_call->getNumArgOperands(); ++i)
        {
            if (extracted_loop_call->getArgOperand(i) == this_array_info)
            {
                num_this_array_info_arg = i;
                break;
            }
        }
        if (num_this_array_info_arg != -1)
        { // if this array is used on this lvl
            num_array_infos_arg.push_back(num_this_array_info_arg);
        }
    }
    // get the corresponding values in extracetd_function
    std::vector<Value *> array_infos_extracted;

    for (int num_this_array_info_arg : num_array_infos_arg)
    {
        auto arg_extracted_iter = extractedLoop->arg_begin();
        // go to the correct argument
        for (int i = 0; i < num_this_array_info_arg; ++i)
        {
            arg_extracted_iter++;
        }
        array_infos_extracted.push_back(*&arg_extracted_iter);
    }

    Debug(errs() << "Optimize deeper loops \n";)
        // first optimize deeper lvl loops (this will must reslut in whloe loop within this
        // function again

        opt_in_lower_lvl += rec_optimize_loop_lvl(M, e, extractedLoop, array_infos_extracted);

    Debug(errs() << "deeper Loops optimized: " << opt_in_lower_lvl << "\n";)
        // extractedLoop->dump();

        // here the real analysis  of this loop starts

        Value *const0 = ConstantInt::get(idx_val->getType(), 0);
    Value *const1 = ConstantInt::get(idx_val->getType(), 1);

    // analyze this loop for constant0 idx
    llvm::ValueToValueMapTy vmap0;
    Function *Analysis0 = CloneFunction(extractedLoop, vmap0);
    Value *idx_in_0 = vmap0[idx_val];
    idx_in_0->replaceAllUsesWith(const0);

    // analyze loop for constant 1 idx
    llvm::ValueToValueMapTy vmap1;
    Function *Analysis1 = CloneFunction(extractedLoop, vmap1);
    Value *idx_in_1 = vmap1[idx_val];
    idx_in_1->replaceAllUsesWith(const1);

    // do constant propagation
    //    SCCPPass* constant_propagation_pass= new SCCPPass();
    // FunctionPass *constant_propagation_pass = llvm::createSCCPPass();
    // FunctionAnalysisManager AM;
    // constant_propagation_pass->runOnFunction(*Analysis0);
    // TODO remove this Hack: which just links all the needed code to our pass
    runSCCP(*Analysis0, M.getDataLayout(), e->TLI);
    runSCCP(*Analysis1, M.getDataLayout(), e->TLI);

    std::vector<Value *> array_infos_0;
    std::vector<Value *> array_infos_1;

    for (int num_this_array_info_arg : num_array_infos_arg)
    {
        auto arg_0_iter = Analysis0->arg_begin();
        auto arg_1_iter = Analysis1->arg_begin();
        // go to the correct argument
        for (int i = 0; i < num_this_array_info_arg; ++i)
        {
            arg_0_iter++;
            arg_1_iter++;
        }
        array_infos_0.push_back(*&arg_0_iter);
        array_infos_1.push_back(*&arg_1_iter);
    }

    // to later find the needed matching instruction in original outlined:
    std::map<Value *, Value *> reverse_map_0;
    for (auto *this_array_info_extracted : array_infos_extracted)
    {
        for (auto *u : this_array_info_extracted->users())
        {
            if (auto *call = dyn_cast<CallInst>(u))
            {
                if (call->getCalledFunction() == e->functions->cache_shared_array_line ||
                    call->getCalledFunction() == e->functions->store_to_shared_array_line)
                {
                    Value *call_in_other = vmap0[call];
                    reverse_map_0.insert(std::make_pair(call_in_other, dyn_cast<Value>(call)));
                    // here we loose the information to which array the call belong
                    // but this information is not needed atm
                }
            }
        }
    }
    // TODO Check if loop is rum by increment (arbitrary step width is supported)
    // optimization will break if it is decrement or other wired operation like mul

    std::vector<CallInst *> used_aray_idx_0;
    std::vector<CallInst *> used_aray_idx_1;

    bool continiue = true;
    continiue = continiue && fill_usage_of_array_vector(M, e, array_infos_0, &used_aray_idx_0);
    continiue = continiue && fill_usage_of_array_vector(M, e, array_infos_1, &used_aray_idx_1);

    if (!continiue)
    {
        Debug(errs() << "Loop is to complicated for array optimization\n";)
            inline_this_function(M, extractedLoop);
        return opt_in_lower_lvl;
    }

    Debug(errs() << "Usage when idx=0:\n"; for (auto *c
                                                : used_aray_idx_0) {
        c->getArgOperand(2)->dump();
    } errs() << "Usage when idx=1:\n";
          for (auto *c
               : used_aray_idx_1) { c->getArgOperand(2)->dump(); })

        // Filter the idx to the classes
        // unknown (not constant when loop idx is constant)
        // constant dependend on loop idx
        // constant independend from loop idx (loop might over be a deeper array dimension)

        std::vector<std::pair<long, CallInst *>>
            dependent_from_idx;
    std::vector<CallInst *> independent_from_loop_idx;
    std::vector<CallInst *> unknown;

    if (used_aray_idx_0.size() != used_aray_idx_1.size())
    { // this means SSCP has erased some dead code when idx is constant we need to assume
      // that at least the array usage is never dead
        Debug(errs() << "Loop is to complicated for array optimization\n";)
            inline_this_function(M, extractedLoop);
        Analysis0->eraseFromParent();
        Analysis1->eraseFromParent();
        return opt_in_lower_lvl;
    }

    for (unsigned int i = 0; i < used_aray_idx_0.size(); ++i)
    {
        CallInst *c0 = used_aray_idx_0[i];
        CallInst *c1 = used_aray_idx_1[i];
        Value *idx0 = c0->getArgOperand(2);
        Value *idx1 = c1->getArgOperand(2);
        if (isa<Constant>(idx0) && isa<Constant>(idx1))
        {
            long val0 = dyn_cast<ConstantInt>(idx0)->getSExtValue();
            long val1 = dyn_cast<ConstantInt>(idx1)->getSExtValue();
            if (val0 == val1)
            {
                Debug(errs() << "Independent line idx found\n";)
                    independent_from_loop_idx.push_back(dyn_cast<CallInst>(reverse_map_0[c0]));
            }
            else
            {
                if (abs(val0 - val1) > 1)
                { // unknown dependency between idx and value
                    Debug(errs() << "unknown dependency found\n";)
                        unknown.push_back(dyn_cast<CallInst>(reverse_map_0[c0]));
                }
                else
                { // assuming dependent from idx only by offset
                  // TODO check if this assumption is really true for the given loop
                  // this means val0 is the offset (e.g. check if not ixd = i*constant)
                    Debug(errs() << "dependent line idx found\n"; reverse_map_0[c0]->dump();)
                        dependent_from_idx.push_back(
                            std::make_pair(val0, dyn_cast<CallInst>(reverse_map_0[c0])));
                }
            }
        }
        else
        { // no constant

            // also allow if idx is sign extended from a func arg, as the pass always
            // expect long as idx val
            Value *is_this_arg = idx0;
            if (auto *sext = dyn_cast<SExtInst>(idx0))
            {
                is_this_arg = sext->getOperand(0);
            }
            if (isa<Argument>(is_this_arg))
            {
                assert(isa<Argument>(idx1) || isa<SExtInst>(idx1)); // just for sanity
                // check if this is a var that do not change within the loop (outlined
                // function)
                // then treat it as constant (regarding the loop execution)
                Debug(errs() << "Independent line idx found\n";)
                    independent_from_loop_idx.push_back(dyn_cast<CallInst>(reverse_map_0[c0]));
            }
            else
            {
                Debug(errs() << "unknown line idx found\n";)
                    unknown.push_back(dyn_cast<CallInst>(reverse_map_0[c0]));
            }
        }
    }

    // free the temporary analysis functions when all data is gatehred
    Analysis0->eraseFromParent();
    Analysis1->eraseFromParent();

    Value *start_idx = find_start_idx(M, e, extractedLoop, idx_val);
    Value *stop_idx = find_stop_idx(M, e, idx_cmp, idx_val);

    if (start_idx == nullptr || stop_idx == nullptr || isa<Constant>(start_idx) ||
        isa<Constant>(stop_idx))
    {
        // TODO handle this?
        // as it is meant for omp for loop this should not be constant because dependent on
        // rank
        Debug(errs() << "constant Loop boundaries are currently not supported for  array "
                        "optimization\n";) inline_this_function(M, extractedLoop);
        return opt_in_lower_lvl;
    }
    else
    {
        // get the idx values in the parent function
        int start_idx_arg_num = dyn_cast<Argument>(start_idx)->getArgNo();
        start_idx = extracted_loop_call->getArgOperand(start_idx_arg_num);
        int stop_idx_arg_num = dyn_cast<Argument>(stop_idx)->getArgNo();
        stop_idx = extracted_loop_call->getArgOperand(stop_idx_arg_num);
    }

    // TODO handle independent constant first
    // also kommunikation innerhalb der extracted-loop function aus loop in loop header
    // verschiebnen, daher keinen einfluss auf loop_split optimierung

    if (idx_cmp->getPredicate() != ICmpInst::Predicate::ICMP_SLT &&
        idx_cmp->getPredicate() != ICmpInst::Predicate::ICMP_SLE &&
        idx_cmp->getPredicate() != ICmpInst::Predicate::ICMP_ULE &&
        idx_cmp->getPredicate() != ICmpInst::Predicate::ICMP_ULT)
    {
        Debug(errs() << "Loop condition: only < or <= is currently supported for  array "
                        "optimization\n";) inline_this_function(M, extractedLoop);
        return opt_in_lower_lvl;
    }
    bool uses_equal_comparision = idx_cmp->isTrueWhenEqual();
    // we need this assumption to enable to use stop idx for part where own lines where
    // used for start idx for part beyond the own lines

    // does not depend on the actual value on teh loop idx:
    auto to_inline = optimize_non_dependent_array_idx(
        M, e, extractedLoop, array_infos_extracted, independent_from_loop_idx);

    if (!dependent_from_idx.empty())
    {
        split_this_loop(M, e, extractedLoop, start_idx, stop_idx, extracted_loop_call,
                        array_infos, dependent_from_idx, inline_again, uses_equal_comparision);
    }

    if (inline_again)
    {
        if (to_inline.first != nullptr)
        {
            InlineFunctionInfo info;
            Function *func = to_inline.first->getCalledFunction();
            InlineFunction(to_inline.first, info);
            func->eraseFromParent();
        }
        if (to_inline.second != nullptr)
        {
            InlineFunctionInfo info;
            Function *func = to_inline.second->getCalledFunction();
            InlineFunction(to_inline.second, info);
            func->eraseFromParent();
        }
    }

    // extractedLoop->dump();
    // F->dump();
    // ASK_TO_CONTINIUE

    return opt_in_lower_lvl + 1; // +1 for this loop
}

// to use as predicate below as isa is not directly valid as a predicate
bool is_distributed_array_var(SharedVariable *v) { return isa<SharedArrayDistributed>(v); }

// This Optimization will break array_cache Profiling, as it eliminates unnecessary checks
// if row it in cache, the information is not updated properly anymore

// it is only ment for omp for loops
// also die selben einschänkungen an die loops, wie omp for
void optimize_array_loop_acces(Module &M, environment *e, ParallelFunction *microtask_object)
{
    Function *parallel_func = microtask_object->get_function();
    std::vector<SharedVariable *> shared_vars = microtask_object->get_shared_vars();
    std::vector<SharedVariable *> shared_array_vars;

    std::copy_if(shared_vars.begin(), shared_vars.end(), std::back_inserter(shared_array_vars),
                 is_distributed_array_var);

    std::vector<Value *> array_infos;

    if (!shared_array_vars.empty())
    {
        for (auto *array_var : shared_array_vars)
        {
            for (auto *u : array_var->value()->users())
            {
                if (auto *cast = dyn_cast<BitCastInst>(u))
                {
                    if (cast->getDestTy() == PointerType::getInt8PtrTy(M.getContext()))
                    {
                        array_infos.push_back(
                            cast); // array_info is passed as void* to the comm_functions
                    }
                }
            }
        }

        Debug(errs() << "Analyze LOOPs\n";)
            // get the dominatortree of the current function
            DominatorTree *DT = new DominatorTree();
        DT->recalculate(*parallel_func);
        // generate the LoopInfoBase for the current function
        LoopInfo *Loop = new LoopInfo(*DT);

        int optimized = 0;
        Loop->releaseMemory();
        Loop->analyze(*DT);

        for (auto *L : Loop->getLoopsInPreorder())
        {
            // it will recursively apply optimization for nested loops therefore we only
            // consider lv 1 loops here
            if (L->getLoopDepth() == 2 &&
                !check_if_loop_calls_other_functions(M, e, shared_array_vars, L))
            {
                optimized +=
                    optimize_this_loop(M, e, parallel_func, array_infos, DT, L, false);
            }
            // TODO currently hardcoded loop lvl 2 for usage with partdiff
            //(Funktioniert auch ohne, ermöglicht den andren Optimierern aber selber über
            // inlining
            // zu entscheiden)
        }

        errs() << "Optimized " << optimized << " Loops\n";
    }
}
