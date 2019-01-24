
#include "SharedVariable.h"
#include "SharedArray.h"
#include "SharedSingleValue.h"
#include "helper.h"

using namespace llvm;

#define DEBUG_SHARED_VARIABLE 0

#if DEBUG_SHARED_VARIABLE == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

llvm::Value *SharedVariable::value() { return var; }

llvm::Type *SharedVariable::getType() { return var->getType(); }

SharedVariable *SharedVariable::getParent() { return parent_var; }

SharedVariable *SharedVariable::getOrigin()
{
    if (parent_var == nullptr)
    {
        return this;
    }
    else
    {
        return parent_var->getOrigin();
    }
}

void SharedVariable::dump() { var->dump(); }

llvm::Value *SharedVariable::get_local_buffer_size(llvm::Module &M, environment *e)
{
    size_t size = get_size_in_Byte(M, this->getType()->getPointerElementType());
    return ConstantInt::get(IntegerType::getInt64Ty(M.getContext()), size,
                            /*is_signed=*/false);
}

void SharedVariable::handle_if_global(llvm::Module &M, environment *e)
{
    assert(isa<GlobalVariable>(var) &&
           !dyn_cast<GlobalVariable>(var)->isExternallyInitialized() &&
           "Changing this Global is not allowed");
    auto *original_global = dyn_cast<GlobalVariable>(var);

    // this should call the correct routine
    // therefore it is sufficient to implement this at the base class at the moment

    StructType *comm_info_type = this->get_comm_info_type(M, e);

    assert(comm_info_type->getElementType(0) ==
           original_global->getType()->getPointerElementType());
    assert(comm_info_type->getElementType(1)->isStructTy());

    // initializer: the original value and 0s for the communication info part
    std::vector<Constant *> init_list = {
        original_global->getInitializer(),
        ConstantAggregateZero::get(comm_info_type->getElementType(1))};

    if (original_global->getType()->getPointerElementType()->isStructTy())
    {
        init_list.push_back(ConstantAggregateZero::get(comm_info_type->getElementType(2)));
    }

    Constant *init = ConstantStruct::get(comm_info_type, init_list);

    GlobalVariable *new_global =
        new GlobalVariable(M, comm_info_type, false, original_global->getLinkage(), init,
                           original_global->getName());

    Constant *cast_to_old = ConstantExpr::getBitCast(new_global, original_global->getType());

    // the communication info part it then initialized for every parallel
    // region encountered

    if (original_global->getType()->getPointerElementType()->isPointerTy())
    {
        // just for my sanity:
        assert(original_global->getInitializer()->isNullValue() &&
               "How can a global ptr be initialized with !=nullptr?");
    }
    // use the new global whereever the old is used to be
    original_global->replaceAllUsesWith(cast_to_old);
    // update the reference of the value this instance is representin
    var = cast_to_old;

    // remove the old global
    original_global->eraseFromParent();
}

void parse_annotation(ConstantDataArray *annotated_data, Comm_Type *comm_type)
{
    if (annotated_data->isString())
    {
        auto string = annotated_data->getAsString();

        Debug(errs() << "Parse Annotation: " << string << "\n";)

            if (string.startswith("OMP2MPI_COMM_DEFAULT"))
        {
            *comm_type = Comm_Type::Default;
        }
        if (string.startswith("OMP2MPI_COMM_READING"))
        {
            *comm_type = Comm_Type::Reading;
        }
        if (string.startswith("OMP2MPI_COMM_MASTER"))
        {
            *comm_type = Comm_Type::MasterBased;
        }
        if (string.startswith("OMP2MPI_COMM_DISTRIBUTED"))
        {
            *comm_type = Comm_Type::Distributed;
        }
        if (string.startswith("OMP2MPI_COMM_MEM_AWARE_DISTRIBUTED"))
        {
            *comm_type = Comm_Type::MemAwareDistributed;
        }
        // enter new comm types here:

        // it will just ignore an unknown annotation
    }
}

bool is_this_annotation(llvm::User *u, llvm::Value *annotated_var, Comm_Type *comm_type)
{

    if (auto *call = dyn_cast<CallInst>(u))
    {
        auto *func = call->getCalledFunction();
        if (func->isIntrinsic())
        {
            if (func->getName().equals("llvm.var.annotation"))
            {
                assert(call->getArgOperand(0) == annotated_var);
                Value *annotated_arg = call->getArgOperand(1);

                // if parsing of this annotation failes: nothing will happen (pretend that no
                // annotation is there)
                if (isa<ConstantExpr>(annotated_arg))
                {
                    auto *as_inst = dyn_cast<ConstantExpr>(annotated_arg)->getAsInstruction();
                    auto *operand = as_inst->getOperandUse(0).get();
                    as_inst->deleteValue();
                    if (isa<GlobalVariable>(operand))
                    {
                        auto *as_global = dyn_cast<GlobalVariable>(operand);
                        if (as_global->hasInitializer() &&
                            isa<ConstantDataArray>(as_global->getInitializer()))
                        {
                            auto *annotated_data =
                                dyn_cast<ConstantDataArray>(as_global->getInitializer());
                            parse_annotation(annotated_data, comm_type);
                            return true;
                        }
                    }
                }
                else if (isa<GlobalVariable>(annotated_arg))
                {
                    auto *as_global = dyn_cast<GlobalVariable>(annotated_arg);
                    if (as_global->hasInitializer() &&
                        isa<ConstantDataArray>(as_global->getInitializer()))
                    {
                        auto *annotated_data =
                            dyn_cast<ConstantDataArray>(as_global->getInitializer());
                        parse_annotation(annotated_data, comm_type);
                        return true;
                    }
                }
            }
        }
    }

    // is not an annotation
    return false;
}

void handle_global_annoataion(llvm::Module &M, environment *e, llvm::Value *var,
                              Comm_Type *comm_type)
{
    auto *global_annoations_var = M.getGlobalVariable("llvm.global.annotations");
    if (global_annoations_var)
    {
        auto *global_annoations =
            dyn_cast<ConstantArray>(global_annoations_var->getInitializer());
        assert(global_annoations != nullptr);

        unsigned int num_elems = global_annoations->getType()->getNumElements();
        for (unsigned int i = 0; i < num_elems; ++i)
        {
            auto *this_elem =
                dyn_cast<ConstantStruct>(global_annoations->getAggregateElement(i));
            assert(this_elem != nullptr && this_elem->getType()->getNumElements() == 4);
            // first is the variable that is annotated
            // second the annotation we are interested in

            unsigned int num =
                0; // needed so compiler knows which getAggregateElement func to call
            auto *annotation_is_for = this_elem->getAggregateElement(num);
            bool is_this_for_current_var = false;

            if (auto *cast = dyn_cast<ConstantExpr>(annotation_is_for))
            {
                auto *as_inst = cast->getAsInstruction();
                auto *operand = as_inst->getOperandUse(0).get();
                as_inst->deleteValue();
                if (isa<GlobalVariable>(operand))
                {
                    is_this_for_current_var = (operand == var);
                }
            }
            else if (isa<GlobalVariable>(annotation_is_for))
            {
                is_this_for_current_var = (annotation_is_for == var);
            }
            if (is_this_for_current_var)
            {
                unsigned int num1 = 1;
                auto *annotated_arg = this_elem->getAggregateElement(num1);
                if (isa<ConstantExpr>(annotated_arg))
                {
                    auto *as_inst = dyn_cast<ConstantExpr>(annotated_arg)->getAsInstruction();
                    auto *operand = as_inst->getOperandUse(0).get();
                    as_inst->deleteValue();
                    if (isa<GlobalVariable>(operand))
                    {
                        auto *as_global = dyn_cast<GlobalVariable>(operand);
                        if (as_global->hasInitializer() &&
                            isa<ConstantDataArray>(as_global->getInitializer()))
                        {
                            auto *annotated_data =
                                dyn_cast<ConstantDataArray>(as_global->getInitializer());
                            parse_annotation(annotated_data, comm_type);
                        }
                    }
                }
                else if (isa<GlobalVariable>(annotated_arg))
                {
                    auto *as_global = dyn_cast<GlobalVariable>(annotated_arg);
                    if (as_global->hasInitializer() &&
                        isa<ConstantDataArray>(as_global->getInitializer()))
                    {
                        auto *annotated_data =
                            dyn_cast<ConstantDataArray>(as_global->getInitializer());
                        parse_annotation(annotated_data, comm_type);
                    }
                }
                // we do not stop as there may be multiple annotations
                // break;
            } // if not for current var: go to next global annotation
        }     // end for each annoataion in global annotations
    }
}

// returns false if no annotation found
// comm type output may only be overwritten if annotation found
void SharedVariable::find_comm_type_from_annotation(llvm::Module &M, environment *e,
                                                    llvm::Value *var, Comm_Type *comm_type)
{
    Type *void_ptr_ty = Type::getInt8PtrTy(var->getContext());

    // get the annotation
    if (isa<GlobalValue>(var))
    {
        handle_global_annoataion(M, e, var, comm_type);
    }
    else
    {
        for (auto *u : var->users())
        {
            if (auto *cast = dyn_cast<BitCastInst>(u))
            {
                if (cast->getType() == void_ptr_ty)
                {
                    for (auto *u2 : cast->users())
                    {
                        if (is_this_annotation(u2, cast, comm_type))
                        {
                            // do not stop, maybe there are multiple annotations?
                        }
                    }
                }
            }
            // e.g. a char*
            // i8ptr does not need a cast
            if (var->getType() == void_ptr_ty)
            {
                if (is_this_annotation(u, var, comm_type))
                {
                    // do not stop, maybe there are multiple annotations?
                }
            }
        }
    }
}

SharedVariable *SharedVariable::CreateArray(llvm::Value *var, Comm_Type comm)
{
    // assert that it is no arrray of structs or array of static sized arrays
    assert(var->getType()->getPointerElementType()->isPointerTy());

    Type *elem_type = var->getType();
    for (int i = 0; i < get_pointer_depth(var->getType()); ++i)
    {
        elem_type = elem_type->getPointerElementType();
    }
    assert(!elem_type->isPointerTy());
    if (elem_type->isArrayTy() || elem_type->isStructTy())
    {
        errs() << "Arrays of Structs or Arrays of static sized Arrays are not supported yet\n";
        return CreateUnhandleledType(var, comm);
    }

    switch (comm)
    {
    case Comm_Type::Reading:
        return new SharedArrayBcasted(var);
        break;
    case Comm_Type::MasterBased:
        return new SharedArrayMasterBased(var);
        break;
    case Comm_Type::MemAwareDistributed:
        return new SharedArrayDistributedMemoryAware(var);
    case Comm_Type::Distributed:
    case Comm_Type::Default:
        return new SharedArrayDistributed(var);
        break;
    default:
        errs() << "WARNING: Unknown communication Type for Array (assuming Default)\n";
        return new SharedArrayDistributed(var);
        break;
    }
}

SharedVariable *SharedVariable::CreateSingleValue(llvm::Value *var, Comm_Type comm)
{
    // assert that it single value type (and not struct)
    assert(!var->getType()->getPointerElementType()->isPointerTy() &&
           !var->getType()->getPointerElementType()->isStructTy() &&
           !var->getType()->getPointerElementType()->isArrayTy());

    switch (comm)
    {
        // enter other comm types here
    case Comm_Type::Reading:
        return new SharedSingleValueReading(var);
        break;
    case Comm_Type::MasterBased:
    case Comm_Type::Distributed: // distributed makes no sense for single value vars anyway
                                 // (TODO? print warning (like below))
    case Comm_Type::Default:
        return new SharedSingleValueDefault(var);
        break;
    default:
        errs() << "WARNING: Unknown communication Type for single Value (assuming Default)\n";
        return new SharedSingleValueDefault(var);
        break;
    }
}

SharedVariable *SharedVariable::CreateUnhandleledType(llvm::Value *var, Comm_Type comm)
{
    // assert that it is unhandled type (is this rly needed?)
    assert(var->getType()->getPointerElementType()->isStructTy() ||
           var->getType()->getPointerElementType()->isArrayTy());

    return new SharedUnhandledType(var);
}

SharedVariable *SharedVariable::Create(llvm::Module &M, environment *e, SharedVariable *parent,
                                       llvm::Value *var)
{
    switch (parent->Type_ID)
    {
    case SharedArrayDistributedID:
        return new SharedArrayDistributed(parent, var);
        break;
    case SharedArrayDistributedMemoryAwareID:
        return new SharedArrayDistributedMemoryAware(parent, var);
        break;
    case SharedArrayBcastedID:
        return new SharedArrayBcasted(parent, var);
        break;
    case SharedArrayMasterBasedID:
        return new SharedArrayMasterBased(parent, var);
        break;
    case SharedSingleValueDefaultID:
        return new SharedSingleValueDefault(parent, var);
        break;
    case SharedSingleValueReadingID:
        return new SharedSingleValueReading(parent, var);
        break;
    case SharedUnhandledTypeID:
        return new SharedUnhandledType(parent, var);
        break;
        // TODO add annotehr case for every otehr constructable subclass
    default:
        // should never happen
        errs() << "ERROR unknown shared Variable Type\n";
        return nullptr;
        break;
    }
}

// factory that constructs a shared Variable with their specific type
SharedVariable *SharedVariable::Create(llvm::Module &M, environment *e, llvm::Value *var)
{
    return SharedVariable::Create(M, e, var, Comm_Type::Default, true);
}

SharedVariable *SharedVariable::Create(llvm::Module &M, environment *e, llvm::Value *var,
                                       Comm_Type comm, bool override_comm_if_annotation)
{
    // else the var is private:
    assert(var->getType()->isPointerTy());

    Comm_Type comm_to_use = comm;
    if (override_comm_if_annotation)
    {
        find_comm_type_from_annotation(M, e, var, &comm_to_use);
    }

    if (var->getType()->getPointerElementType()->isPointerTy())
    {
        return CreateArray(var, comm_to_use);
    }
    else if (var->getType()->getPointerElementType()->isStructTy() ||
             var->getType()->getPointerElementType()->isArrayTy())
    {
        return CreateUnhandleledType(var, comm_to_use);
    }
    else
    {
        return CreateSingleValue(var, comm_to_use);
    }
}
