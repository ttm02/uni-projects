// Functions to Replace openmp functions
// or an openmp Pragma functionality
#include "struct_pointer.h"
#include "comm_patterns/commPatterns.h"
#include "environment.h"
#include "external_functions.h"
#include "helper.h"

#include "llvm/IR/InstIterator.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#define DEBUG_STRUCT_POINTER 0

#if DEBUG_STRUCT_POINTER == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

using namespace llvm;

// Builds the Map old Type -> new type
void build_old_to_new_struct_type_map(Module &M, environment *e)
{
    for (StructType *st : M.getIdentifiedStructTypes())
    {
        if (is_struct_user_defined(M, e, st))
        {
            std::vector<Type *> content;

            for (auto *elemT : st->elements())
            {
                if (elemT->isPointerTy())
                {
                    content.push_back(e->types->mpi_struct_ptr);
                }
                else
                {
                    content.push_back(elemT);
                }
            }

            // the new Type is renamed implicidly
            StructType *newType = StructType::create(M.getContext(), content, st->getName());
            // insert to the map
            e->types->struct_old_new_map[st] = newType;
            Debug(errs() << "Replacing Struct Type: \n"; st->dump(); newType->dump();)
        }
    }
    // TODO also Replace struct within structs
    // at least warn user that he should use pointer within them
}

// gives the ptr to the changed struct (thrid part of comm_info struct for struct type)
Value *get_changed_struct_type_ptr(Module &M, environment *e, Value *normal_ptr,
                                   Instruction *insert_before)
{
    IRBuilder<> builder(insert_before);
    Type *comm_info = get_default_comm_info_struct_type(
        M, e, normal_ptr->getType()->getPointerElementType());
    Value *cast = builder.CreateBitCast(normal_ptr, comm_info->getPointerTo());
    Value *ptr_to_changed_part =
        builder.CreateGEP(cast, {builder.getInt32(0), builder.getInt32(2)});

    assert(ptr_to_changed_part->getType()->getPointerElementType() ==
           e->types->struct_old_new_map[dyn_cast<StructType>(
               normal_ptr->getType()->getPointerElementType())]);
    return ptr_to_changed_part;
}

// gives the ptr to the mpi_struct_ptr (second part of comm_info struct for struct type)
Value *get_mpi_struct_ptr(Module &M, environment *e, Value *normal_ptr,
                          Instruction *insert_before)
{
    IRBuilder<> builder(insert_before);
    Type *comm_info = get_default_comm_info_struct_type(
        M, e, normal_ptr->getType()->getPointerElementType());
    Value *cast = builder.CreateBitCast(normal_ptr, comm_info->getPointerTo());

    Value *ptr_to_mpi_struct_ptr =
        builder.CreateGEP(cast, {builder.getInt32(0), builder.getInt32(1)});
    assert(ptr_to_mpi_struct_ptr->getType() == e->types->mpi_struct_ptr->getPointerTo());

    return ptr_to_mpi_struct_ptr;
}

// forward deklaration of functions below
void handle_usage_of_struct_ptr(Module &M, environment *e, Value *struct_ptr);

void handle_usage_of_shared_struct(Module &M, environment *e, Value *struct_ptr,
                                   GetElementPtrInst *gep_to_member);

// functions to handle the different operations for shared struct members

void handle_load_of_struct_member(Module &M, environment *e, Value *struct_ptr,
                                  GetElementPtrInst *gep_to_member,
                                  std::vector<Instruction *> *to_remove, LoadInst *load)
{
    IRBuilder<> builder(M.getContext());
    bool to_member_ptr = (gep_to_member->getResultElementType()->isPointerTy());

    // load value from struct: we have to fetch its content first
    Value *ptr_to_mpi_ptr_struct = get_mpi_struct_ptr(M, e, struct_ptr, load);
    builder.SetInsertPoint(load);

    // TODO allow to only fetch the accessed part of the struct from remote process

    Value *to_void = builder.CreateBitCast(struct_ptr, builder.getInt8PtrTy());
    std::vector<Value *> args = {to_void, ptr_to_mpi_ptr_struct, e->global->struct_ptr_win};
    CallInst *load_call =
        dyn_cast<CallInst>(builder.CreateCall(e->functions->mpi_load_shared_struct, args));

    if (to_member_ptr)
    {
        Type *tgt_type = gep_to_member->getResultElementType();
        Type *tgt_comm_info =
            get_default_comm_info_struct_type(M, e, tgt_type->getPointerElementType());
        assert(tgt_comm_info != nullptr);

        // allocating a buffer for the value of ptr (as it may be remote)
        // therefore the struct will containt the local pointer
        builder.SetInsertPoint(get_init_block(M, e, load->getFunction())->getTerminator());
        Value *buffer_alloc = builder.CreateAlloca(tgt_comm_info);
        Value *correct_ptr = builder.CreateBitCast(buffer_alloc, tgt_type, load->getName());

        load->replaceAllUsesWith(correct_ptr);
        to_remove->push_back(load);

        // set up the communication info part of local buffer. do not fetch remote content yet
        // (as it may be nullptr)
        std::vector<Value *> idxList;
        unsigned int operands = gep_to_member->getNumOperands();
        // operand 0 is the Pointer to struct
        for (unsigned int i = 1; i < operands; ++i)
        {
            idxList.push_back(gep_to_member->getOperand(i));
        }
        Value *ptr_to_comm_info = get_changed_struct_type_ptr(M, e, struct_ptr, load);
        builder.SetInsertPoint(load);

        Value *gep_in_comm_info_part = builder.CreateGEP(ptr_to_comm_info, idxList);
        assert(gep_in_comm_info_part->getType() == e->types->mpi_struct_ptr->getPointerTo());

        Value *mpi_ptr_in_comm_info = get_mpi_struct_ptr(M, e, correct_ptr, load);
        builder.SetInsertPoint(load); // need to reset insertion point

        builder.CreateCall(e->functions->mpi_copy_shared_struct_ptr,
                           {gep_in_comm_info_part, mpi_ptr_in_comm_info});

        handle_usage_of_struct_ptr(M, e, correct_ptr);
    }
}

void handle_store_of_struct_member(Module &M, environment *e, Value *struct_ptr,
                                   GetElementPtrInst *gep_to_member,
                                   std::vector<Instruction *> *to_remove, StoreInst *store)
{
    IRBuilder<> builder(M.getContext());
    bool to_member_ptr = (gep_to_member->getResultElementType()->isPointerTy());

    // store: we first fetch the content, do the store and update remote copy
    // we fetch content first, so that we have the updated values for all other
    // locations of the struct

    // TODO allow to only store the accessed part of the struct to remote process in order to
    // remove unnecessary communication

    Value *ptr_to_mpi_ptr_struct = get_mpi_struct_ptr(M, e, struct_ptr, store);
    builder.SetInsertPoint(store);

    // TODO allow to only fetch the accessed part of the struct from remote process

    Value *to_void = builder.CreateBitCast(struct_ptr, builder.getInt8PtrTy());
    std::vector<Value *> args = {to_void, ptr_to_mpi_ptr_struct, e->global->struct_ptr_win};
    builder.CreateCall(e->functions->mpi_load_shared_struct, args);
    if (to_member_ptr)
    {
        // handle the store of a shared struct ptr
        assert(store->getPointerOperand() == gep_to_member &&
               "WTF Why do you store the LOCATION of a ptr inside a struct? Dont do "
               "That!");

        Type *tgt_type = gep_to_member->getResultElementType();
        Type *tgt_comm_info =
            get_default_comm_info_struct_type(M, e, tgt_type->getPointerElementType());
        assert(tgt_comm_info != nullptr);

        // the pointer that should be stored must be a pointer to a local struct buffer (or
        // null)
        Value *to_store = store->getValueOperand();
        assert(to_store->getType() == tgt_type);

        Value *remote_mpi_struct_ptr = nullptr;
        if (isa<ConstantPointerNull>(to_store))
        {
            remote_mpi_struct_ptr =
                ConstantPointerNull::get(e->types->mpi_struct_ptr->getPointerTo());
        }
        else
        {
            remote_mpi_struct_ptr = get_mpi_struct_ptr(M, e, to_store, store);
        }
        assert(remote_mpi_struct_ptr != nullptr);
        assert(remote_mpi_struct_ptr->getType() == e->types->mpi_struct_ptr->getPointerTo());

        std::vector<Value *> idxList;
        unsigned int operands = gep_to_member->getNumOperands();
        // operand 0 is the Pointer to struct
        for (unsigned int i = 1; i < operands; ++i)
        {
            idxList.push_back(gep_to_member->getOperand(i));
        }
        Value *ptr_to_comm_info = get_changed_struct_type_ptr(M, e, struct_ptr, store);
        builder.SetInsertPoint(store); // need to reset insertion point
        Value *gep_in_comm_info_part = builder.CreateGEP(ptr_to_comm_info, idxList);
        assert(gep_in_comm_info_part->getType() == e->types->mpi_struct_ptr->getPointerTo());

        builder.SetInsertPoint(store); // need to reset insertion point

        std::vector<Value *> args_for_cpy = {remote_mpi_struct_ptr, gep_in_comm_info_part};
        builder.CreateCall(e->functions->mpi_copy_shared_struct_ptr, args_for_cpy);

        // to_remove->push_back(store);
        // the store is not removed, as it is a valid loacal operation
    }
    builder.SetInsertPoint(store->getNextNode());
    builder.CreateCall(e->functions->mpi_store_to_shared_struct, args);
}

void handle_cast_of_struct_member(Module &M, environment *e, Value *struct_ptr,
                                  GetElementPtrInst *gep_to_member,
                                  std::vector<Instruction *> *to_remove, BitCastInst *cast)
{
    IRBuilder<> builder(M.getContext());
    bool to_member_ptr = (gep_to_member->getResultElementType()->isPointerTy());

    if (to_member_ptr)
    {
        errs() << "This should never Happen! there should be a load of ptr value first\n";
        cast->dump();
    }
    // why should this ever happen anyway?
    errs() << "Casting a ptr to struct member is not advised!\n";
    // we will nevertheless fetch the content of struct so instructions after the cast are
    // valid
    Value *ptr_to_mpi_ptr_struct = get_mpi_struct_ptr(M, e, struct_ptr, cast);
    builder.SetInsertPoint(cast);
    // TODO allow to only fetch the accessed part of the struct from remote process

    Value *to_void = builder.CreateBitCast(struct_ptr, builder.getInt8PtrTy());
    std::vector<Value *> args = {to_void, ptr_to_mpi_ptr_struct, e->global->struct_ptr_win};
    CallInst *load_call =
        dyn_cast<CallInst>(builder.CreateCall(e->functions->mpi_load_shared_struct, args));
    // I think this should never happen anyway but
    // maybe we can just enter a for all usage of this cast:
    // fetch remote content
    // do the operation
    // update remote content
}

void handle_GEP_of_struct_member(Module &M, environment *e, Value *struct_ptr,
                                 GetElementPtrInst *gep_to_member,
                                 std::vector<Instruction *> *to_remove, GetElementPtrInst *gep)
{
    IRBuilder<> builder(M.getContext());
    bool to_member_ptr = (gep_to_member->getResultElementType()->isPointerTy());

    if (to_member_ptr)
    {
        errs() << "This should never Happen! there should be a load of ptr value first\n";
        gep->dump();
    }
    errs() << "Structs within structs are not supported yet use ptr to struct instead\n";
}

void handle_call_of_struct_member(Module &M, environment *e, Value *struct_ptr,
                                  GetElementPtrInst *gep_to_member,
                                  std::vector<Instruction *> *to_remove, CallInst *call)
{
    IRBuilder<> builder(M.getContext());
    bool to_member_ptr = (gep_to_member->getResultElementType()->isPointerTy());

    if (to_member_ptr)
    {
        errs() << "This should never Happen! there should be a load of ptr value first\n";
        call->dump();
    }
    // Call: will do the same as store, as callee may modify the struct
    // but exclude calls from rtlib
    if (!is_func_defined_by_pass(e, call->getCalledFunction()))
    {
        Value *ptr_to_mpi_ptr_struct = get_mpi_struct_ptr(M, e, struct_ptr, call);
        builder.SetInsertPoint(call);
        // TODO allow to only fetch the accessed part of the struct from remote process

        Value *to_void = builder.CreateBitCast(struct_ptr, builder.getInt8PtrTy());
        std::vector<Value *> args = {to_void, ptr_to_mpi_ptr_struct,
                                     e->global->struct_ptr_win};
        builder.CreateCall(e->functions->mpi_load_shared_struct, args);

        builder.SetInsertPoint(call->getNextNode());
        builder.CreateCall(e->functions->mpi_store_to_shared_struct, args);
    }
    // this should very rarely happen anyway (more often it will be load and than call)
}

void handle_invoke_of_struct_member(Module &M, environment *e, Value *struct_ptr,
                                    GetElementPtrInst *gep_to_member,
                                    std::vector<Instruction *> *to_remove, InvokeInst *invoke)
{
    IRBuilder<> builder(M.getContext());
    bool to_member_ptr = (gep_to_member->getResultElementType()->isPointerTy());

    if (to_member_ptr)
    {
        errs() << "This should never Happen! there should be a load of ptr value first\n";
        invoke->dump();
    }
    // invoke: baisically same as call
    if (!is_func_defined_by_pass(e, invoke->getCalledFunction()))
    {
        Value *ptr_to_mpi_ptr_struct = get_mpi_struct_ptr(M, e, struct_ptr, invoke);
        builder.SetInsertPoint(invoke);
        // TODO allow to only fetch the accessed part of the struct from remote process

        Value *to_void = builder.CreateBitCast(struct_ptr, builder.getInt8PtrTy());
        std::vector<Value *> args = {to_void, ptr_to_mpi_ptr_struct,
                                     e->global->struct_ptr_win};
        builder.CreateCall(e->functions->mpi_load_shared_struct, args);

        builder.SetInsertPoint(invoke->getNormalDest()->getFirstNonPHI());
        builder.CreateCall(e->functions->mpi_store_to_shared_struct, args);
    }
}

// functions to handle the different operations for shared struct ptrs

void handle_load_of_struct_ptr(Module &M, environment *e, Value *old_var,
                               std::vector<Instruction *> *to_remove, LoadInst *load)
{
    IRBuilder<> builder(M.getContext());

    // TODO treat it as load of content at idx 0 (maybe insert gep idx 0)
    // this is a load of shared struct content
    errs() << "This should never happen:\n";
    load->dump();
    errs() << "There should be a getelemptr first!\n";
}

void handle_store_of_struct_ptr(Module &M, environment *e, Value *old_var,
                                std::vector<Instruction *> *to_remove, StoreInst *store)
{
    IRBuilder<> builder(M.getContext());

    if (old_var == store->getPointerOperand())
    {
        // TODO treat it as store to content at idx 0 (maybe insert gep idx 0)
        // this is a store to the shared struct
        errs() << "This should never happen:\n";
        store->dump();
        errs() << "There should be a getelemptr first!\n";
    }
    else
    {
        // if the value of the ptr is stored
        Debug(errs() << "store of struct ptr:\n"; store->dump(); errs() << "To address :\n";
              store->getPointerOperand()->dump();)

        // nothing to do.
        // it will be handled, when encountering teh store from the pointer operand side
    }
}

void handle_cast_of_struct_ptr(Module &M, environment *e, Value *old_var,
                               std::vector<Instruction *> *to_remove, BitCastInst *cast)
{
    IRBuilder<> builder(M.getContext());
    if (cast->getDestTy() !=
        get_default_comm_info_struct_type(M, e, old_var->getType()->getPointerElementType())
            ->getPointerTo())
    { // do not report Pass-Inserted casting
        errs() << "it is not advised to cast struct ptr around!\n";
        if (cast->getDestTy() == builder.getInt8PtrTy())
        {
            errs() << "You can ignore this warning if this is done during Reduction\n";
        }
        cast->dump();
    }
}

void handle_GEP_of_struct_ptr(Module &M, environment *e, Value *old_var,
                              std::vector<Instruction *> *to_remove, GetElementPtrInst *gep)
{
    IRBuilder<> builder(M.getContext());

    Debug(errs() << "GEP on a struct ptr\n"; gep->dump();)
        // this instruction will give a ptr to struct content

        // handle the usage of structs content
        // this will insert fetch of remote struct content if needed
        handle_usage_of_shared_struct(M, e, old_var, gep);
}

void handle_call_of_struct_ptr(Module &M, environment *e, Value *old_var,
                               std::vector<Instruction *> *to_remove, CallInst *call)
{
    IRBuilder<> builder(M.getContext());

    Debug(errs() << "call with a struct ptr!\n"; call->dump();)

        // TODO handele it differently if this is a sret argument?

        bool not_defined_in_module = call->getCalledFunction()->isDeclaration() &&
                                     !is_func_defined_by_pass(e, call->getCalledFunction()) &&
                                     !is_func_defined_by_openmp(e, call->getCalledFunction());
    // if function not defined in the current module will use ptr inside this struct the
    // behaviour is undefined

    if (not_defined_in_module)
    {
        builder.SetInsertPoint(call);
        // TODO fetch remote content
        Value *mpi_ptr_struct = get_mpi_struct_ptr(M, e, old_var, call);
        Value *as_void = builder.CreateBitCast(old_var, builder.getInt8PtrTy());

        builder.CreateCall(e->functions->mpi_load_shared_struct,
                           {as_void, mpi_ptr_struct, e->global->struct_ptr_win});

        builder.SetInsertPoint(call->getNextNode());
        // update remote content
        builder.CreateCall(e->functions->mpi_store_to_shared_struct,
                           {as_void, mpi_ptr_struct, e->global->struct_ptr_win});
    }
}

void handle_invoke_of_struct_ptr(Module &M, environment *e, Value *old_var,
                                 std::vector<Instruction *> *to_remove, InvokeInst *invoke)
{
    IRBuilder<> builder(M.getContext());
    Debug(errs() << "invoke with struct ptr!\n";
          invoke->dump();) // shandle it the same as call
        builder.SetInsertPoint(invoke);
    bool not_defined_in_module = invoke->getCalledFunction()->isDeclaration() &&
                                 !is_func_defined_by_pass(e, invoke->getCalledFunction()) &&
                                 !is_func_defined_by_openmp(e, invoke->getCalledFunction());
    if (not_defined_in_module)
    {
        errs() << "Invoking a Function with a struct pointer as arg not defined in this "
                  "module micht lead to undefined behaviour\n";
    }
}

void handle_cmp_of_struct_ptr(Module &M, environment *e, Value *old_var,
                              std::vector<Instruction *> *to_remove, CmpInst *cmp)
{
    IRBuilder<> builder(M.getContext());

    Debug(errs() << "cmp with ptr!\n"; cmp->dump();)

        // catch the case when this cmp was already handled but is encountered again for other
        // operand
        if (cmp->user_empty())
    {
        // nothing to do
        return;
    }

    Constant *cmp_func = nullptr;
    if (cmp->getPredicate() == CmpInst::Predicate::ICMP_EQ)
    {
        cmp_func = e->functions->mpi_cmp_EQ_shared_struct_ptr;
    }
    else if (cmp->getPredicate() == CmpInst::Predicate::ICMP_NE)
    {
        cmp_func = e->functions->mpi_cmp_NE_shared_struct_ptr;
    }
    else
    {
        errs() << "Why are you doing this comparision on struct ptr? (treating it "
                  "as not equal comparision)\n";
        cmp->dump();
        cmp_func = e->functions->mpi_cmp_NE_shared_struct_ptr;
    }
    assert(cmp_func != nullptr);
    // cmp always have 2 operands
    assert(cmp->getNumOperands() == 2);
    Value *op1 = cmp->getOperand(0);
    Value *op2 = cmp->getOperand(1);
    // comparision is only supported between structs of same type
    assert(op1->getType() == op2->getType() && op1->getType() == old_var->getType());

    if (isa<ConstantPointerNull>(op1))
    {
        // get it with mpi_struct_ptr type
        op1 = ConstantPointerNull::get(e->types->mpi_struct_ptr->getPointerTo());
    }
    else
    {
        op1 = get_mpi_struct_ptr(M, e, op1, cmp);
    }

    if (isa<ConstantPointerNull>(op2))
    {
        // get it with mpi_struct_ptr type
        op2 = ConstantPointerNull::get(e->types->mpi_struct_ptr->getPointerTo());
    }
    else
    {
        op2 = get_mpi_struct_ptr(M, e, op2, cmp);
    }

    assert(op1->getType() == e->types->mpi_struct_ptr->getPointerTo());
    assert(op2->getType() == e->types->mpi_struct_ptr->getPointerTo());
    std::vector<Value *> args = {op1, op2};
    builder.SetInsertPoint(cmp); // reset insertion point
    Value *new_cmp = builder.CreateCall(cmp_func, args);
    cmp->replaceAllUsesWith(new_cmp);
    to_remove->push_back(cmp);
}

void handle_return_of_struct_ptr(Module &M, environment *e, Value *old_var,
                                 std::vector<Instruction *> *to_remove, ReturnInst *ret)
{
    IRBuilder<> builder(M.getContext());
    Debug(errs() << "ret with struct ptr!\n"; ret->dump();)
    // nothing to do
}

void handle_PHI_of_struct_ptr(Module &M, environment *e, Value *old_var,
                              std::vector<Instruction *> *to_remove, PHINode *phi)
{
    IRBuilder<> builder(M.getContext());
    // handle phi node (Zuweisung nach if)
    Debug(errs() << "phi with struct ptr!\n"; phi->dump();)

        // we need to make shure this phi is only handeled once
        static std::set<llvm::PHINode *>
            already_handeled;

    if (std::get<bool>(already_handeled.insert(phi)))
    {
        // only if insertion took place

        // as result of phi is a register we need a new buffer (mpi struct ptr cannot be held
        // in a register)
        Type *comm_info_type =
            get_default_comm_info_struct_type(M, e, phi->getType()->getPointerElementType());
        builder.SetInsertPoint(get_init_block(M, e, phi->getFunction())->getTerminator());
        Value *buffer_alloc = builder.CreateAlloca(comm_info_type);
        Value *correct_ptr =
            builder.CreateBitCast(buffer_alloc, phi->getType(), phi->getName());

        phi->replaceAllUsesWith(correct_ptr);
        // phi is not erased!
        // as it is used to determine the pointer that will be used to intialize the
        // communication part of the buffer

        Instruction *insertion_pt = phi->getParent()->getFirstNonPHI();

        // init communication info of buffer
        Value *mpi_ptr_in_buffer = get_mpi_struct_ptr(M, e, correct_ptr, insertion_pt);
        Value *mpi_ptr_in_phi = get_mpi_struct_ptr(M, e, phi, insertion_pt);

        builder.SetInsertPoint(insertion_pt);
        builder.CreateCall(e->functions->mpi_copy_shared_struct_ptr,
                           {mpi_ptr_in_phi, mpi_ptr_in_buffer});

        handle_usage_of_struct_ptr(M, e, correct_ptr);
    }
}

// handle the usage of a shared struct contents
// void_local_buffer is the local struct content buffer
// new_gep is the new getelemptr inst that gives a ptr to a struct elem
// old_gep is the old gep instruction that should be replaced with new_gep
// ptr_to_mpi_ptr_struct is a ptr to the mpi_ptr_struct associated wit the local_buffer (it
// contains the ptr to remote)
void handle_usage_of_shared_struct(Module &M, environment *e, Value *struct_ptr,
                                   GetElementPtrInst *gep_to_member)
{
    assert(struct_ptr != nullptr);
    assert(gep_to_member != nullptr);

    IRBuilder<> builder(M.getContext());
    std::vector<Instruction *> to_remove;

    // new_gep->getFunction()->dump();

    // is this gep pointing to a shared ptr as an element of the currens struct?
    bool to_member_ptr = (gep_to_member->getResultElementType()->isPointerTy());

    for (auto *u : gep_to_member->users())
    {
        auto *inst = dyn_cast<Instruction>(u);

        switch (inst->getOpcode())
        {
        case Instruction::Load:
            handle_load_of_struct_member(M, e, struct_ptr, gep_to_member, &to_remove,
                                         dyn_cast<LoadInst>(inst));
            break;
        case Instruction::Store:
            handle_store_of_struct_member(M, e, struct_ptr, gep_to_member, &to_remove,
                                          dyn_cast<StoreInst>(inst));
            break;
        case Instruction::GetElementPtr:
            handle_GEP_of_struct_member(M, e, struct_ptr, gep_to_member, &to_remove,
                                        dyn_cast<GetElementPtrInst>(inst));
            break;
        case Instruction::BitCast:
            handle_cast_of_struct_member(M, e, struct_ptr, gep_to_member, &to_remove,
                                         dyn_cast<BitCastInst>(inst));
            break;
        case Instruction::Call:
            handle_call_of_struct_member(M, e, struct_ptr, gep_to_member, &to_remove,
                                         dyn_cast<CallInst>(inst));
            break;
        case Instruction::Invoke:
            handle_invoke_of_struct_member(M, e, struct_ptr, gep_to_member, &to_remove,
                                           dyn_cast<InvokeInst>(inst));
            break;

        default:

            if (to_member_ptr)
            {
                errs() << "This should never Happen! there should be a load of ptr value "
                          "first\n";
                inst->dump();
            }
            // other operation we will do the same as store to be secure
            // might be something like an atomic incment
            Value *ptr_to_mpi_ptr_struct = get_mpi_struct_ptr(M, e, struct_ptr, inst);
            builder.SetInsertPoint(inst);

            // TODO allow to only fetch the accessed part of the struct from remote process
            Value *to_void = builder.CreateBitCast(struct_ptr, builder.getInt8PtrTy());
            std::vector<Value *> args = {to_void, ptr_to_mpi_ptr_struct,
                                         e->global->struct_ptr_win};
            builder.CreateCall(e->functions->mpi_load_shared_struct, args);

            builder.SetInsertPoint(inst->getNextNode());
            builder.CreateCall(e->functions->mpi_store_to_shared_struct, args);

            break;
        }
    }

    for (auto *inst : to_remove)
    {
        if (!inst->use_empty())
        {
            // than the order of the instructions removal does not matter:
            inst->replaceAllUsesWith(UndefValue::get(inst->getType()));
            errs() << "Nevertheless insertion of undef should not hapen\n";
        }
        inst->eraseFromParent();
    }
}

// handles the usage of a ptr to shared struct
// oldType is the old struct type the pointer was to
// newType is the new struct type the pointer should be
// old_var is the old pointer Value (type= oldType*)
// mpi_ptr_struct is the ptr to mpi_ptr_struct
void handle_usage_of_struct_ptr(Module &M, environment *e, Value *struct_ptr)
{
    assert(struct_ptr != nullptr);

    IRBuilder<> builder(M.getContext());

    std::vector<Instruction *> to_remove;
    // first we have to insert new instructions
    // then we have to remove old instructions (oterhwise it might break the iterator)
    for (auto *u : struct_ptr->users())
    {
        auto *inst = dyn_cast<Instruction>(u);

        switch (inst->getOpcode())
        {
        case Instruction::Load:
            handle_load_of_struct_ptr(M, e, struct_ptr, &to_remove, dyn_cast<LoadInst>(inst));
            break;
        case Instruction::Store:
            handle_store_of_struct_ptr(M, e, struct_ptr, &to_remove,
                                       dyn_cast<StoreInst>(inst));
            break;
        case Instruction::GetElementPtr:
            handle_GEP_of_struct_ptr(M, e, struct_ptr, &to_remove,
                                     dyn_cast<GetElementPtrInst>(inst));
            break;
        case Instruction::BitCast:
            handle_cast_of_struct_ptr(M, e, struct_ptr, &to_remove,
                                      dyn_cast<BitCastInst>(inst));
            break;
        case Instruction::Call:
            handle_call_of_struct_ptr(M, e, struct_ptr, &to_remove, dyn_cast<CallInst>(inst));
            break;
        case Instruction::Invoke:
            handle_invoke_of_struct_ptr(M, e, struct_ptr, &to_remove,
                                        dyn_cast<InvokeInst>(inst));
            break;
        case Instruction::ICmp:
            // Fcmp on ptr type make no sense
            handle_cmp_of_struct_ptr(M, e, struct_ptr, &to_remove, dyn_cast<ICmpInst>(inst));
            break;
        case Instruction::Ret:
            handle_return_of_struct_ptr(M, e, struct_ptr, &to_remove,
                                        dyn_cast<ReturnInst>(inst));
            break;
        case Instruction::PHI:
            handle_PHI_of_struct_ptr(M, e, struct_ptr, &to_remove, dyn_cast<PHINode>(inst));
            break;
        default:

            errs() << "This Operation on a struct ptr is not supported "
                      "yet:\n";
            inst->dump();

            break;
        }
    }

    for (auto *inst : to_remove)
    {
        if (!inst->use_empty())
        {
            // than the order of the instructions removal does not matter:
            inst->replaceAllUsesWith(UndefValue::get(inst->getType()));
            errs() << "Nevertheless insertion of undef should not hapen\n";
        }
        inst->eraseFromParent();
    }
}

// functions to handle the different originations of struct ptrs:

// this handles struct ptr in function args
// giving structs directly by value to a func is not supported
void handle_struct_ptr_in_function_args(Module &M, environment *e)
{
    for (auto &F : M.getFunctionList())
    {
        // only if F is user defined in this module
        if (!F.isDeclaration() && !is_func_defined_by_pass(e, &F) &&
            !is_func_defined_by_openmp(e, &F))
        {
            for (auto &arg : F.args())
            {
                if (auto *arg_ptr_ty = dyn_cast<PointerType>(arg.getType()))
                {
                    if (auto *struct_ty =
                            dyn_cast<StructType>(arg_ptr_ty->getPointerElementType()))
                    {
                        if (is_struct_user_defined(M, e, struct_ty))
                        {
                            if (!arg.hasStructRetAttr())
                            {
                                // TODO:
                                // if this is struct return we will not handle it and
                                // instead just build a local buffer for it when calling
                                // the func
                                Debug(errs() << "handle struct ptr for arg "; arg.dump();
                                      errs() << "in function" << F.getName() << "\n";)

                                    handle_usage_of_struct_ptr(M, e, &arg);
                            }
                        }
                    }
                }
            }
        }
    }
}

// initializes the mpi_struct_ptr when a new struct is allocated and attaches it to mpi_win
void build_init_of_struct_comm_info(Module &M, environment *e, Value *new_struct_ptr,
                                    Instruction *insert_before)
{
    IRBuilder<> builder(insert_before);

    Value *mpi_win = builder.CreateLoad(e->global->struct_ptr_win);
    Value *void_ptr =
        builder.CreateBitCast(new_struct_ptr, Type::getInt8PtrTy(M.getContext()));
    Value *size = builder.getInt64(
        get_size_in_Byte(M, new_struct_ptr->getType()->getPointerElementType()));
    std::vector<Value *> args = {mpi_win, void_ptr, size};
    builder.CreateCall(e->functions->MPI_Win_attach, args);
    // build the mpi pointer_struct
    Value *mpi_struct_ptr =
        builder.CreateGEP(new_struct_ptr, {builder.getInt32(0), builder.getInt32(1)});
    std::vector<Value *> args2 = {void_ptr, size, mpi_struct_ptr};
    builder.CreateCall(e->functions->mpi_store_shared_struct_ptr, args2);
}

void handle_struct_alloca(Module &M, environment *e,
                          std::vector<std::pair<AllocaInst *, StructType *>> alloca_list)
{
    IRBuilder<> builder(M.getContext());
    for (auto alloc_pair : alloca_list)
    {
        Debug(errs() << "replacing struct type for struct allocated at\n";
              alloc_pair.first->dump();) AllocaInst *alloc = alloc_pair.first;
        StructType *old_struct_type = alloc_pair.second;
        StructType *new_struct_type = get_default_comm_info_struct_type(M, e, old_struct_type);
        assert(new_struct_type != nullptr);

        builder.SetInsertPoint(alloc->getNextNode());
        // renamed implicidly anyway but the new name is at least similar to old one:
        Value *new_alloc = builder.CreateAlloca(new_struct_type, nullptr, alloc->getName());
        Value *cast_to_old = builder.CreateBitCast(new_alloc, old_struct_type->getPointerTo());
        alloc->replaceAllUsesWith(cast_to_old);
        build_init_of_struct_comm_info(M, e, new_alloc, dyn_cast<Instruction>(cast_to_old));

        // TODO add mpi win detach at end of function

        handle_usage_of_struct_ptr(M, e, cast_to_old);
        alloc->eraseFromParent();
    }
}

// handles if struct is created using malloc or other allocation function
void handle_struct_malloc(Module &M, environment *e,
                          std::vector<std::pair<BitCastInst *, StructType *>> bitCast_list)
{
    IRBuilder<> builder(M.getContext());
    for (auto bitCast_pair : bitCast_list)
    {

        BitCastInst *bitCast = bitCast_pair.first;
        StructType *old_struct_type = bitCast_pair.second;
        StructType *new_struct_type = get_default_comm_info_struct_type(M, e, old_struct_type);

        assert(new_struct_type != nullptr);
        Debug(errs() << "Handle Allocation of struct casted in "; bitCast->dump();
              errs() << " origin: "; bitCast->getOperand(0)->dump();)

            // struct is typed to new struct type
            assert(bitCast->getNumOperands() == 1);
        // bitcast only has one operand

        if (bitCast->getSrcTy() != Type::getInt8PtrTy(M.getContext()) &&
            bitCast->getSrcTy() != old_struct_type->getPointerTo() &&
            bitCast->getSrcTy() != e->types->mpi_struct_ptr->getPointerTo())
        {
            // if it was casted from mpi_struct_ptr it originates as a function arg
            // this has already been handled then
            // void* or direct struct* are handled in here and therefore not reported :-)
            bitCast->dump();
            errs() << "WARNING: It is not advised to cast struct ptr around! This might lead "
                      "to undefined behaiviour.\n";
        }
        Value *casted_ptr = bitCast->getOperand(0);

        // change size argument of allocation
        if (auto *call_inst = dyn_cast<CallInst>(casted_ptr))
        {
            int replace_argument = -1;
            if (call_inst->getCalledFunction()->getName().startswith("malloc"))
            {
                replace_argument = 0;
            }
            if (call_inst->getCalledFunction()->getName().startswith("calloc"))
            {
                replace_argument = 1;
            }
            // new struct XYZ leads to call to
            if (call_inst->getCalledFunction()->getName().startswith("_Znwm"))
            {
                replace_argument = 0;
            }

            if (replace_argument != -1)
            {
                if (ConstantInt *size =
                        dyn_cast<ConstantInt>(call_inst->getArgOperand(replace_argument)))
                {
                    ConstantInt *old_size = ConstantInt::get(
                        size->getType(), get_size_in_Byte(M, old_struct_type));
                    if (old_size == size)
                    {
                        ConstantInt *new_size = ConstantInt::get(
                            size->getType(), get_size_in_Byte(M, new_struct_type));
                        call_inst->setArgOperand(replace_argument, new_size);
                        // errs() << "replaced alloc size with "; new_size->dump();
                        builder.SetInsertPoint(call_inst->getNextNode());
                        Value *cast_to_new_type =
                            builder.CreateBitCast(casted_ptr, new_struct_type->getPointerTo());
                        build_init_of_struct_comm_info(
                            M, e, cast_to_new_type,
                            dyn_cast<Instruction>(cast_to_new_type)->getNextNode());

                        handle_usage_of_struct_ptr(M, e, bitCast);
                    }
                    else
                    {
                        errs() << "It is not supported to allocate static sized Arrays of "
                                  "structs yet\n"
                               << "size given: ";
                        call_inst->getArgOperand(replace_argument)->dump();
                    }
                }
                else
                {
                    errs() << "It is not supported to allocate dynamic sized Arrays of "
                              "structs yet\n"
                           << "size given: ";
                    call_inst->getArgOperand(replace_argument)->dump();
                }
            }
            else
            {
                errs() << "unknown Allocation Call:\n";
                call_inst->dump();
            }
        }
        else // if origin of cast not is a CallInst
        {
            // struct* are supposed to pass around as ptr of old type
            // it will be handled in handle_struct_ptr_in_function_args
            if (!isa<Argument>(casted_ptr))
            {
                if (auto *load = dyn_cast<LoadInst>(casted_ptr))
                {
                    if (casted_ptr->getType() != Type::getInt8PtrTy(M.getContext()))
                    {
                        // loading of ptr from void* array is done during reduction
                        // if this is done, there is no need to handle it
                        errs() << "Error in replacing the load of struct ptr in";
                        casted_ptr->dump();
                    }
                }
                else
                {
                    errs() << "Error do not found allocation of ";
                    casted_ptr->dump();
                }
            }
        }
    }
}

// handles if a function returns a struct ptr
void handle_struct_ptr_origin_from_return(
    Module &M, environment *e, std::vector<std::pair<CallInst *, StructType *>> call_list,
    std::vector<std::pair<InvokeInst *, StructType *>> invoke_list)
{
    IRBuilder<> builder(M.getContext());
    for (auto call_pair : call_list)
    {
        CallInst *call = call_pair.first;
        StructType *old_struct_type = call_pair.second;
        handle_usage_of_struct_ptr(M, e, call);
    }
    // same for invoke
    // but insertion point of cast instruction is different
    for (auto invoke_pair : invoke_list)
    {
        InvokeInst *invoke = invoke_pair.first;
        StructType *old_struct_type = invoke_pair.second;

        handle_usage_of_struct_ptr(M, e, invoke);
    }
}

// replaces all usage of a struct Pointer within the module
void handle_struct_pointer(Module &M, environment *e)
{
    // sores the instructions where structs a created
    std::vector<std::pair<BitCastInst *, StructType *>>
        bitCast_list; // follow ptr to where it is allocated
    std::vector<std::pair<AllocaInst *, StructType *>>
        alloca_list; // direct alloca of struct type
    std::vector<std::pair<AllocaInst *, StructType *>>
        ptr_alloca_list; // ptr alloca of struct type

    // malloced ptr are not supported

    // where struct ptr are given as return val
    std::vector<std::pair<CallInst *, StructType *>> call_list;
    std::vector<std::pair<InvokeInst *, StructType *>> invoke_list;

    // for ALL instructions in the module:
    // find all allocations of structs
    for (auto &F : M.getFunctionList())
    {
        // only handle user defined functions
        if (!is_func_defined_by_pass(e, &F) && !is_func_defined_by_openmp(e, &F))
        {
            for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
            {
                if (auto *bitCast = dyn_cast<BitCastInst>(&*I))
                {
                    // only if it is a cast to struct type ptr
                    if (auto *dest_ptr_ty = dyn_cast<PointerType>(bitCast->getDestTy()))
                    {
                        if (auto *dest_ty =
                                dyn_cast<StructType>(dest_ptr_ty->getElementType()))
                        {
                            if (is_struct_user_defined(M, e, dest_ty))
                            {
                                // whe than chase the ptr to the allocation call in
                                // handle_struct_malloc function
                                bitCast_list.push_back(std::make_pair(bitCast, dest_ty));
                            }
                        }
                    }
                }
                if (auto *alloc = dyn_cast<AllocaInst>(&*I))
                {
                    // only if this is a struct allocation
                    if (auto *dest_ty = dyn_cast<StructType>(alloc->getAllocatedType()))
                    {
                        if (is_struct_user_defined(M, e, dest_ty))
                        {
                            alloca_list.push_back(std::make_pair(alloc, dest_ty));
                        }
                        // or if this is a struct ptr alloc
                    }
                    else if (auto *ptr_ty = dyn_cast<PointerType>(alloc->getAllocatedType()))
                    {
                        if (auto *dest_ty =
                                dyn_cast<StructType>(ptr_ty->getPointerElementType()))
                        {
                            if (is_struct_user_defined(M, e, dest_ty))
                            {
                                ptr_alloca_list.push_back(std::make_pair(alloc, dest_ty));
                            }
                        }
                    }
                }
                if (auto *call = dyn_cast<CallInst>(&*I))
                {
                    // why there was a nullptr as called function?
                    // maybe it was related to the usage of virtual functions in
                    // linked_lists.h?
                    if (call->getCalledFunction() != nullptr &&
                        !is_func_defined_by_pass(e, call->getCalledFunction()) &&
                        !is_func_defined_by_openmp(e, call->getCalledFunction()))
                    {
                        // only if this returns a struct ptr
                        if (auto *ret_ptr_ty = dyn_cast<PointerType>(
                                call->getCalledFunction()->getReturnType()))
                        {
                            if (auto *ret_struct_ty =
                                    dyn_cast<StructType>(ret_ptr_ty->getPointerElementType()))
                            {
                                if (is_struct_user_defined(M, e, ret_struct_ty))
                                {
                                    call_list.push_back(std::make_pair(call, ret_struct_ty));
                                }
                            }
                        }
                        else if (auto *ret_ty = dyn_cast<StructType>(
                                     call->getCalledFunction()->getReturnType()))
                        {
                            if (is_struct_user_defined(M, e, ret_ty))
                            {
                                errs() << "The direct return of structs are not supported "
                                          "Return "
                                          "pointer to struct instead \n Function: "
                                       << call->getCalledFunction()->getName() << "\n";
                            }
                        }
                    }
                }
                // same as for call
                if (auto *invoke = dyn_cast<InvokeInst>(&*I))
                {
                    if (invoke->getCalledFunction() != nullptr &&
                        !is_func_defined_by_pass(e, invoke->getCalledFunction()) &&
                        !is_func_defined_by_openmp(e, invoke->getCalledFunction()))
                    {
                        // only if this returns a struct ptr
                        if (auto *ret_ptr_ty = dyn_cast<PointerType>(
                                invoke->getCalledFunction()->getReturnType()))
                        {
                            if (auto *ret_struct_ty =
                                    dyn_cast<StructType>(ret_ptr_ty->getPointerElementType()))
                            {
                                if (is_struct_user_defined(M, e, ret_struct_ty))
                                {
                                    invoke_list.push_back(
                                        std::make_pair(invoke, ret_struct_ty));
                                }
                            }
                        }
                        else if (auto *ret_ty = dyn_cast<StructType>(
                                     invoke->getCalledFunction()->getReturnType()))
                        {
                            if (is_struct_user_defined(M, e, ret_ty))
                            {
                                errs() << "The direct return of structs are not supported "
                                          "Return "
                                          "pointer to struct instead \n Function: "
                                       << invoke->getCalledFunction()->getName() << "\n";
                            }
                        }
                    }
                }
            }
        }
    }

    Debug(errs() << "Now replacing struct Pointer with mpi_ptr_struct\n";)

        for (auto alloc_pair : ptr_alloca_list)
    {
        errs() << "found allocation of ptr\n";
        alloc_pair.first->dump();
        errs() << "this is not supported yet\n";
        return;
    }

    // handle the different cases from where a struct ptr can originate:
    handle_struct_ptr_in_function_args(M, e);
    // handle new struct (bitcast + alloca)
    handle_struct_alloca(M, e, alloca_list);
    handle_struct_malloc(M, e, bitCast_list);
    // handle originations from calls and invokes
    handle_struct_ptr_origin_from_return(M, e, call_list, invoke_list);

    Debug(errs() << "Handled all struct ptr\n";)
}

void build_reduce_wrapper_for_struct_type_reduction(Module &M, environment *e,
                                                    Function *omp_combiner,
                                                    BasicBlock *insertF)
{

    IRBuilder<> builder(insertF);

    Debug(errs() << "recuction with structs: build new ompcombiner\n";)

        // get the real user defined func
        Function *user_defined_func = nullptr;
    for (inst_iterator I = inst_begin(omp_combiner), E = inst_end(omp_combiner); I != E; ++I)
    {
        if (auto *call = dyn_cast<CallInst>(&*I))
        {
            if (!call->getCalledFunction()->isIntrinsic())
            {
                user_defined_func = call->getCalledFunction();
            }
        }
    }
    assert(user_defined_func != nullptr);

    // Wrapper-Function = .ompcombiner. nachbauen
    Type *correct_type_t =
        omp_combiner->getFunctionType()->getFunctionParamType(0)->getPointerElementType();
    assert(isa<StructType>(correct_type_t));
    StructType *correct_type = dyn_cast<StructType>(correct_type_t);
    StructType *comm_info_type = get_default_comm_info_struct_type(M, e, correct_type);
    assert(comm_info_type != nullptr);
    // function takes 3 args (first arg is return)
    assert(user_defined_func->getFunctionType()
               ->getFunctionParamType(0)
               ->getPointerElementType() == correct_type);
    assert(user_defined_func->getFunctionType()
               ->getFunctionParamType(1)
               ->getPointerElementType() == correct_type);
    assert(user_defined_func->getFunctionType()
               ->getFunctionParamType(2)
               ->getPointerElementType() == correct_type);

    int i = 0;
    Value *arg1 = nullptr;
    Value *arg2 = nullptr;
    for (auto &arg : insertF->getParent()->args())
    {
        if (i == 0)
        {
            arg1 = &arg;
        }
        else if (i == 1)
        {
            arg2 = &arg;
        }
        i++;
        // other arguments are ignored
    }
    assert(arg1 != nullptr && arg2 != nullptr);
    assert(arg1->getType() == builder.getInt8PtrTy());
    assert(arg2->getType() == builder.getInt8PtrTy());

    // same semantics as MPI_User_function:
    Value *buffer = builder.CreateAlloca(comm_info_type);
    Value *v_ptr = builder.CreateBitCast(buffer, builder.getInt8PtrTy());
    Value *size = builder.getInt64(get_size_in_Byte(M, comm_info_type));

    Value *old_t1 = builder.CreateBitCast(arg1, correct_type->getPointerTo());
    Value *old_t2 = builder.CreateBitCast(arg2, correct_type->getPointerTo());
    Value *old_t3 = builder.CreateBitCast(buffer, correct_type->getPointerTo());

    Value *struct_ptr_2 = get_mpi_struct_ptr(M, e, old_t2, dyn_cast<Instruction>(old_t3));
    // there should be no need to reset isert point as current point is after the instruction
    // given to function

    // fetch content of struct 2
    builder.CreateCall(e->functions->mpi_load_shared_struct,
                       {v_ptr, struct_ptr_2, e->global->struct_ptr_win});

    builder.CreateCall(user_defined_func, {old_t3, old_t1, old_t2});

    // the structs chould be passed by pointer as they have increased size
    auto iter = user_defined_func->arg_begin();
    iter++; // arg 1 (arg 0 has no byval attr as it is sret, which is handeled correctly)
    Argument *arg = iter;
    arg->removeAttr(Attribute::AttrKind::ByVal);
    iter++; // arg 2
    arg = iter;
    arg->removeAttr(Attribute::AttrKind::ByVal);

    // update content of struct 2 with resulting value
    builder.CreateCall(e->functions->mpi_store_to_shared_struct,
                       {v_ptr, struct_ptr_2, e->global->struct_ptr_win});

    builder.CreateRetVoid();

    // now we have to alter all calls to the combiner func
    // combiner is called on reducction result a last time
    // but as this WrapperF has done the whole Reduction
    // the next calls to ompcombiner should only be a copy
    // copy from first to second arg

    std::vector<User *> combiner_users = get_function_users(M, omp_combiner->getName());
    for (auto *u : combiner_users)
    {
        if (auto *call = dyn_cast<CallInst>(u))
        {
            builder.SetInsertPoint(call);
            std::vector<Value *> args;
            for (unsigned int i = 0; i < call->getNumArgOperands(); ++i)
            {
                args.push_back(call->getArgOperand(i));
                // builder.CreateBitCast(call->getArgOperand(i), builder.getInt8PtrTy()));
            }
            assert(args.size() == 2);
            Value *cast_arg_1 = builder.CreateBitCast(args[0], correct_type->getPointerTo());
            Value *cast_arg_2 = builder.CreateBitCast(args[1], correct_type->getPointerTo());

            Value *mpi_ptr_1 = get_mpi_struct_ptr(M, e, cast_arg_1, call);
            Value *mpi_ptr_2 = get_mpi_struct_ptr(M, e, cast_arg_2, call);
            builder.SetInsertPoint(call);

            // see below
            Value *mpi_ptr_buffer = builder.CreateAlloca(e->types->mpi_struct_ptr);
            builder.CreateCall(e->functions->mpi_copy_shared_struct_ptr,
                               {mpi_ptr_1, mpi_ptr_buffer});

            Value *copy_buffer = builder.CreateAlloca(comm_info_type);
            Value *copy_buffer_void =
                builder.CreateBitCast(copy_buffer, builder.getInt8PtrTy());
            // copy buffer is needed, as result of reduction might be on anotehr rank (0)
            builder.CreateCall(e->functions->mpi_load_shared_struct,
                               {copy_buffer_void, mpi_ptr_2, e->global->struct_ptr_win});
            builder.CreateCall(e->functions->mpi_store_to_shared_struct,
                               {copy_buffer_void, mpi_ptr_buffer, e->global->struct_ptr_win});
            // this will result in copy from struct 2 to struct 1
            // but it will result in copying the whole comm_info part as well.
            // therefore we need to overwrite the pointer part again
            builder.CreateCall(e->functions->mpi_copy_shared_struct_ptr,
                               {mpi_ptr_buffer, mpi_ptr_1});

            call->eraseFromParent();
        }
        // same for invoke (but also need a branch to next block)
        if (auto *invoke = dyn_cast<InvokeInst>(u))
        {
            builder.SetInsertPoint(invoke);
            std::vector<Value *> args;
            for (unsigned int i = 0; i < invoke->getNumArgOperands(); ++i)
            {
                args.push_back(invoke->getArgOperand(i));
                // builder.CreateBitCast(call->getArgOperand(i), builder.getInt8PtrTy()));
            }
            assert(args.size() == 2);
            Value *cast_arg_1 = builder.CreateBitCast(args[0], correct_type->getPointerTo());
            Value *cast_arg_2 = builder.CreateBitCast(args[1], correct_type->getPointerTo());

            Value *mpi_ptr_1 = get_mpi_struct_ptr(M, e, cast_arg_1, invoke);
            Value *mpi_ptr_2 = get_mpi_struct_ptr(M, e, cast_arg_2, invoke);
            builder.SetInsertPoint(invoke);

            // see below
            Value *mpi_ptr_buffer = builder.CreateAlloca(e->types->mpi_struct_ptr);
            builder.CreateCall(e->functions->mpi_copy_shared_struct_ptr,
                               {mpi_ptr_1, mpi_ptr_buffer});

            Value *copy_buffer = builder.CreateAlloca(comm_info_type);
            Value *copy_buffer_void =
                builder.CreateBitCast(copy_buffer, builder.getInt8PtrTy());
            // copy buffer is needed, as result of reduction might be on anotehr rank (0)
            builder.CreateCall(e->functions->mpi_load_shared_struct,
                               {copy_buffer_void, mpi_ptr_2, e->global->struct_ptr_win});
            builder.CreateCall(e->functions->mpi_store_to_shared_struct,
                               {copy_buffer_void, mpi_ptr_buffer, e->global->struct_ptr_win});
            // this will result in copy from struct 2 to struct 1
            // but it will result in copying the whole comm_info part as well.
            // therefore we need to overwrite the pointer part again
            builder.CreateCall(e->functions->mpi_copy_shared_struct_ptr,
                               {mpi_ptr_buffer, mpi_ptr_1});
            builder.CreateBr(invoke->getNormalDest());
            invoke->eraseFromParent();
        }
    }
}
