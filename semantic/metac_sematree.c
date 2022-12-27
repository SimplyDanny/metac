#include "metac_sematree.h"
#include "metac_semantic.h"
#include "../hash/crc32c.h"

#undef walker_fn

int MetaCSemaTree_Walk_Debug(metac_node_t node, struct metac_sema_state_t* sema,
                             const char* fn_name, walker_function_t walker_fn, void* ctx)
{
    // make sure the context confusion cookie is set
    // printf("fn_name: %s\n", fn_name);
    assert((*(uint32_t*) ctx) == crc32c_nozero(~0, fn_name, strlen(fn_name)));

    return MetaCSemaTree_Walk_Real(node, sema, walker_fn, ctx);
}

metac_type_t TypePtrToNode(metac_type_index_t typeIdx,metac_sema_state_t* sema)
{
    metac_type_t result;
    uint32_t idx = TYPE_INDEX_INDEX(typeIdx);
    switch(TYPE_INDEX_KIND(typeIdx))
    {
        case type_index_basic:
            assert(0);
        break;
        case type_index_struct:
            result = (metac_type_t)StructPtr(sema, idx);
        break;
        case type_index_union:
            result = (metac_type_t)UnionPtr(sema, idx);
        break;
        case type_index_ptr:
            result = (metac_type_t)PtrTypePtr(sema, idx);
        break;
        case type_index_array:
            result = (metac_type_t)ArrayTypePtr(sema, idx);
        break;
        case type_index_enum:
            result = (metac_type_t)EnumTypePtr(sema, idx);
        break;
        case type_index_tuple:
            result = (metac_type_t)TupleTypePtr(sema, idx);
        break;
        case type_index_typedef:
            result = (metac_type_t)TypedefPtr(sema, idx);
        break;
        case type_index_functiontype:
            result = (metac_type_t)FunctiontypePtr(sema, idx);
        break;
        default:
            assert(0); // Not currently supported

    }
    return result;
}

int MetaCSemaTree_Walk_Real(metac_node_t node, struct metac_sema_state_t* sema,
                            walker_function_t walker_fn, void* ctx)
{

#define walker_fn(NODE, CTX) \
    walker_fn((metac_node_t)NODE, CTX)

#define MetaCSemaTree_Walk_Real(NODE, SEMA, FN, CTX) \
    MetaCSemaTree_Walk_Real((metac_node_t)NODE, SEMA, FN, CTX)

#define emptyNode \
    ((metac_node_t) 0x1)

    if(((metac_node_t)node) == emptyNode)
        return 0;

    int result = walker_fn(node, ctx);

    if (result)
        return result;

    switch(node->Kind)
    {
#define CASE_(EXP) \
    case node_ ## EXP:

        default:
        case node_decl_min:
        case node_decl_max:
            assert(0);
        FOREACH_PRIMARY_EXP(CASE_)
            break;

        FOREACH_BINARY_EXP(CASE_)
        {
            metac_sema_expr_t* e = (metac_sema_expr_t*) node;
            result = walker_fn(e->E1, ctx);
            if (result)
                return result;

            result = walker_fn(e->E2, ctx);
            if (result)
                return result;
        } break;

        FOREACH_UNARY_EXP(CASE_)
        case node_expr_paren:
        {
            metac_sema_expr_t* e = (metac_sema_expr_t*) node;
            result = walker_fn(e->E1, ctx);
            if (result)
                return result;
        } break;

        case node_decl_variable:
        {
            sema_decl_variable_t* variable = (sema_decl_variable_t*) node;
            // result = MetaCSemaTree_Walk_Real(variable->, sema, walker_fn, ctx);
            if (result)
                return result;
        } break;
        case node_decl_enum_member:
        {
            sema_decl_enum_member_t* enum_member = (sema_decl_enum_member_t*) node;
            result = walker_fn(enum_member->Value, ctx);
            if (result)
                return result;
        } break;
        case node_decl_type:
        {
            sema_decl_type_t* type = (sema_decl_type_t*) node;
            if (result)
                return result;
        } break;
        case node_decl_type_struct:
        {
            metac_type_aggregate_t* type_struct = (metac_type_aggregate_t*) node;

            for(uint32_t i = 0; i < type_struct->FieldCount; i++)
            {
                metac_type_aggregate_field_t* field;
                result = walker_fn(field, ctx);
                if (result)
                    return result;

            }
            result = MetaCSemaTree_Walk_Real(type_struct->Fields, sema, walker_fn, ctx);
            if (result)
                return result;
        } break;
        case node_decl_type_union:
        {
            sema_decl_type_union_t* type_union = (sema_decl_type_union_t*) node;
            result = MetaCSemaTree_Walk_Real(type_union->Fields, sema, walker_fn, ctx);
            if (result)
                return result;
        } break;
        case node_decl_type_enum:
        {
            sema_decl_type_enum_t* type_enum = (sema_decl_type_enum_t*) node;
            result = MetaCSemaTree_Walk_Real(type_enum->Members, sema, walker_fn, ctx);
            if (result)
                return result;
        } break;
        case node_decl_type_array:
        {
            sema_decl_type_array_t* type_array = (sema_decl_type_array_t*) node;
            result = MetaCSemaTree_Walk_Real(type_array->ElementType, sema, walker_fn, ctx);
            if (result)
                return result;
            result = walker_fn(type_array->Dim, ctx);
            if (result)
                return result;
        } break;
        case node_decl_type_ptr:
        {
            sema_decl_type_ptr_t* type_ptr = (sema_decl_type_ptr_t*) node;
            metac_type_t typePtr = TypePtrToNode(type_ptr->ElementType, sema);
            result = MetaCSemaTree_Walk_Real(typePtr, sema, walker_fn, ctx);
            if (result)
                return result;
        } break;
        case node_decl_type_functiontype:
        {
            sema_decl_type_functiontype_t* type_functiontype = (sema_decl_type_functiontype_t*) node;
            metac_type_t returnType = TypePtrToNode(type_functiontype->ReturnType, sema);
            result = MetaCSemaTree_Walk_Real(returnType, sema, walker_fn, ctx);
            if (result)
                return result;

            for(uint32_t i = 0; i < type_functiontype->ParameterTypeCount; i++)
            {
                metac_type_index_t parameterTypeIdx = type_functiontype->ParameterTypes[i];
                metac_type_t parameterType = TypePtrToNode(parameterTypeIdx, sema);
                result = MetaCSemaTree_Walk_Real(parameterType, sema, walker_fn, ctx);
            }

            if (result)
                return result;
        } break;
        case node_decl_type_typedef:
        {
            sema_decl_type_typedef_t* typedef_ = (sema_decl_type_typedef_t*) node;
            metac_type_t type = TypePtrToNode(typedef_->Type, sema);
            result = MetaCSemaTree_Walk_Real(type, sema, walker_fn, ctx);
            if (result)
                return result;
        } break;
        case node_decl_function:
        {
            sema_decl_function_t* function_ = (sema_decl_function_t*) node;

            metac_type_functiontype_t* functionType =
                (metac_type_functiontype_t*)TypePtrToNode(function_->TypeIndex, sema);

            for(int32_t i = 0; i < functionType->ParameterTypeCount; i++)
            {
                sema_decl_variable_t* param = function_->Parameters + i;
                result = MetaCSemaTree_Walk_Real(param, sema, walker_fn, ctx);
                if (result)
                    return result;
            }
            result = MetaCSemaTree_Walk_Real(function_->Parameters, sema, walker_fn, ctx);
            if (result)
                return result;
            result = walker_fn(function_->FunctionBody, ctx);
            if (result)
                return result;
        } break;
    }

    return 0;
}

#undef MetaCSemaTree_Walk_Real
#undef walker_fn
#undef emptyNode
