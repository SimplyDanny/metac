#include "metac_alloc.h"
#include "libinterpret/bc_common.h"
#include "libinterpret/backend_interface_funcs.h"
#include "libinterpret/bc_interpreter_backend.h"
#include "libinterpret/printer_backend.c"
#include "repl/exp_eval.h"
#include "metac_codegen.h"

#include <stdarg.h>
uint32_t MetaCCodegen_GetTypeABISize(metac_bytecode_ctx_t* ctx, metac_type_index_t type)
{
    return 0;
}

void MetaCCodegen_doStatement(metac_bytecode_ctx_t* ctx,
                              metac_sema_statement_t* stmt);

BCType MetaCCodegen_GetBCType(metac_bytecode_ctx_t* ctx, metac_type_index_t type)
{
    BCType result = {};

    if (type.Kind == type_index_enum)
        result = (BCType){BCTypeEnum_i32};

    if (type.Kind == type_index_basic)
    {
        switch(type.Index)
        {
            case type_void:
                result = (BCType){BCTypeEnum_Void};
            break;
            case type_bool:
                result = (BCType){BCTypeEnum_u32};
            break;
            case type_char:
                result = (BCType){BCTypeEnum_i8};
            break;
            case type_short:
                result = (BCType){BCTypeEnum_i16};
            break;
            case type_int:
                result = (BCType){BCTypeEnum_i32};
            break;
            case type_long:
                result = (BCType){BCTypeEnum_i64};
            break;
            case type_size_t:
                result = (BCType){BCTypeEnum_u64};
            break;

            case type_float:
                result = (BCType){BCTypeEnum_f23};
            break;
            case type_double:
                result = (BCType){BCTypeEnum_f52};
            break;

            case type_long_long:
                result = (BCType){BCTypeEnum_i64};
            break;
            case type_long_double:
                result = (BCType){BCTypeEnum_f106};
            break;

            case type_unsigned_char:
                result = (BCType){BCTypeEnum_u8};
            break;
            case type_unsigned_short:
                result = (BCType){BCTypeEnum_u16};
            break;
            case type_unsigned_int:
                result = (BCType){BCTypeEnum_u32};
            break;
            case type_unsigned_long:
                result = (BCType){BCTypeEnum_u64};
            break;
            case type_unsigned_long_long:
                result = (BCType){BCTypeEnum_u64};
            break;
            default : assert(0);
        }
    }

    return  result;
}

extern const BackendInterface BCGen_interface;
const BackendInterface* bc;

int MetaCCodegen_RunFunction(metac_bytecode_ctx_t* self,
                             metac_bytecode_function_t f,
                             metac_alloc_t* interpAlloc,
                             const char* fargs, ...)
{
    va_list l;
    va_start(l, fargs);
    STACK_ARENA_ARRAY(BCValue, args, 8, interpAlloc);

    const char* farg = fargs;
    uint32_t nArgs = 0;
    for(char c = *farg++;c; c = *farg++)
    {
        switch(c)
        {
            case 'd' :
            {
                nArgs++;
                int32_t a = va_arg(l, int32_t);
                ARENA_ARRAY_ADD(args, imm32_(a, true));
            }
            break;
            case 'u':
            {
                nArgs++;
                uint32_t a = va_arg(l, uint32_t);
                ARENA_ARRAY_ADD(args, imm32(a));
            }
            break;
            default:
            {
                fprintf(stderr, "arg specifier, '%c' unsupported\n");
                assert(!"Value format unsupported");
            }

        }
    }
    va_end(l);

    BCValue result = bc->run(self->c, f.FunctionIndex, args, nArgs);
    return result.imm32.imm32;
}
void MetaCCodegen_End(metac_bytecode_ctx_t* self)
{
    bc->Finalize(self->c);

    if (bc == &Printer_interface)
    {
        Printer* printer = (Printer*)self->c;
        printf("%s\n\n", printer->BufferStart);
    }

}

void MetaCCodegen_Init(metac_bytecode_ctx_t* self, metac_alloc_t* parentAlloc)
{
    //TODO take BC as a parameter
#if BC_PRINTER
    bc = &Printer_interface;
#else
    bc = &BCGen_interface;
#endif
    (*self) = (metac_bytecode_ctx_t) {};
    Allocator_Init(&self->Allocator, parentAlloc, 0);

    tagged_arena_t* arena =
        AllocateArena(&self->Allocator, bc->sizeof_instance());
    self->c = arena->Memory;
    bc->init_instance(self->c);

    bc->Initialize(self->c, 0);

    ARENA_ARRAY_INIT(metac_bytecode_function_t, self->Functions, &self->Allocator);
}


void MetaCCodegen_Begin(metac_bytecode_ctx_t* self, metac_identifier_table_t* idTable)
{
    assert(self->c != 0);
    self->IdentifierTable = idTable;
    VariableStore_Init(&self->Vstore, idTable);
    ARENA_ARRAY_INIT(metac_bytecode_switch_t, self->SwitchStack, &self->Allocator);
}

metac_bytecode_function_t MetaCCodegen_GenerateFunction(metac_bytecode_ctx_t* ctx,
                                                        sema_decl_function_t* function)
{
    void* c = ctx->c;

    uint32_t frameSize = 0;
    uint32_t functionParameterCount = 1;
    STACK_ARENA_ARRAY(BCValue, parameters, 16, 0);

    const char* fName =
        IdentifierPtrToCharPtr(ctx->IdentifierTable, function->Identifier);

    uint32_t functionId =
        bc->beginFunction(c, 0, fName);

    bc->Comment(c, "Function Begin.");

    metac_bytecode_function_t result;
    result.FunctionIndex = functionId;

    for(uint32_t i = 0;
        i < functionParameterCount;
        i++)
    {
        sema_decl_variable_t* paramVar = function->Parameters + i;
        uint32_t paramSize = MetaCCodegen_GetTypeABISize(ctx, paramVar->TypeIndex);

        const char* paramName = IdentifierPtrToCharPtr(ctx->IdentifierTable,
                                                       paramVar->VarIdentifier);
        BCType paramType = MetaCCodegen_GetBCType(ctx, paramVar->TypeIndex);
        BCValue param = bc->genParameter(c, paramType, paramName);
        ARENA_ARRAY_ADD(parameters, param);
        VariableStore_AddVariable(&ctx->Vstore, paramVar, &parameters[parametersCount - 1]);

        frameSize += paramSize;
    }
    assert(parametersCount == functionParameterCount);
    ctx->Parameters = parameters;
    ctx->ParameterCount = functionParameterCount;

    for (uint32_t i = 0;
         i < function->FunctionBody->StatementCount;
         i++)
    {
        MetaCCodegen_doStatement(ctx, function->FunctionBody->Body[i]);
    }
    bc->Comment(c, "Function body end");

    bc->endFunction(c, result.FunctionIndex);
/*
    if (bc == &BCGen_interface)
    {
        BCGen_PrintCode(c, 0, 600);
    }
*/
    return result;
}
static BCValue MetaCCodegen_doExpression(metac_bytecode_ctx_t* ctx, metac_sema_expression_t* exp)
{
    BCType expType = MetaCCodegen_GetBCType(ctx, exp->TypeIndex);
    BCValue v;
    if (exp->Kind != exp_signed_integer)
        v = bc->genTemporary(ctx->c, expType);
    WalkTree(ctx->c, &v, exp, &ctx->Vstore);
    return v;
}

static metac_bytecode_switch_t* MetaCCodegen_PushSwitch(metac_bytecode_ctx_t* ctx, BCValue exp)
{
    metac_bytecode_switch_t swtch = {};
    swtch.Exp = exp;
    METAC_NODE(swtch.DefaultBody) = emptyNode;
    ARENA_ARRAY_INIT(CndJmpBegin, swtch.PrevCaseJumps, &ctx->Allocator);

    ARENA_ARRAY_ADD(ctx->SwitchStack, swtch);

    return ctx->SwitchStack + (ctx->SwitchStackCount - 1);
}

static void MetaCCodegen_PopSwitch(metac_bytecode_ctx_t* ctx, BCValue exp)
{
    assert(ctx->SwitchStackCount > 0);

    metac_bytecode_switch_t* swtch =
        ctx->SwitchStack + (ctx->SwitchStackCount - 1);
    assert(BCValue_eq(&exp, &swtch->Exp));
    //FreeArena(ctx->Sw)
    --ctx->SwitchStackCount;
}

static void FixupBreaks(metac_bytecode_ctx_t* ctx, uint32_t breakCount, BCLabel breakLabel)
{
    void* c = ctx->c;
    if (ctx->BreaksCount > breakCount)
    {
        for(uint32_t i = breakCount;
            i < ctx->BreaksCount; i++)
        {
            bc->endJmp(c, (BCAddr)ctx->Breaks[i], breakLabel);
        }
        ctx->BreaksCount = breakCount;
    }
}

static void MetaCCodegen_doBlockStmt(metac_bytecode_ctx_t* ctx,
                                     sema_stmt_block_t* stmt)
{
    assert(stmt->StmtKind == stmt_block);
    uint32_t variableCount = ctx->VariablesCount;
    for(uint32_t i = 0; i < stmt->StatementCount; i++)
    {
        MetaCCodegen_doStatement(ctx, stmt->Body[i]);
    }
}
static inline void MetaCCodegen_doCaseStmt(metac_bytecode_ctx_t* ctx,
                                           sema_stmt_case_t* caseStmt)
{
    void* c = ctx->c;
    assert(ctx->SwitchStackCount > 0);

    metac_bytecode_switch_t* swtch = &ctx->SwitchStack[ctx->SwitchStackCount - 1];
    metac_sema_expression_t* caseExp = caseStmt->CaseExp;
    metac_sema_statement_t*  caseBody = caseStmt->CaseBody;

    // if there's no exp it's the default case
    if (METAC_NODE(caseExp) == emptyNode)
    {
        // make sure we haven't yet seen a default
        assert(METAC_NODE(swtch->DefaultBody) == emptyNode);
        swtch->DefaultBody = caseBody;
        return ;
    }
    BCValue* switchExp = &swtch->Exp;
    BCValue exp_result = MetaCCodegen_doExpression(ctx, caseExp);
    bc->Eq3(c, 0,
            switchExp,
            &exp_result);

    bool hasBody =
        !(caseBody && caseBody->StmtKind == stmt_case);
    // if we have our own body we want to jump is the cnd is false
    // otherwise we want to jump if it's true
    CndJmpBegin cndJmp = bc->beginCndJmp(c, 0, !hasBody);
    if (hasBody)
    {
        BCLabel caseLabel;
        const uint32_t caseCount = swtch->PrevCaseJumpsCount;

        if (caseCount > 0)
        {
            caseLabel = bc->genLabel(c);
            for(uint32_t i = 0;
                i < caseCount;
                i++)
            {
                CndJmpBegin* cj = swtch->PrevCaseJumps + i;
                bc->endCndJmp(c, cj, caseLabel);
            }
            swtch->PrevCaseJumpsCount = 0;
        }

        MetaCCodegen_doStatement(ctx, caseStmt->CaseBody);
        bc->endCndJmp(c, &cndJmp, bc->genLabel(c));
    }
    else
    {
        ARENA_ARRAY_ADD(swtch->PrevCaseJumps, cndJmp);
        if (caseBody)
            MetaCCodegen_doStatement(ctx, caseBody);
    }
}
void MetaCCodegen_doStatement(metac_bytecode_ctx_t* ctx,
                              metac_sema_statement_t* stmt)
{
    void* c = ctx->c;
    switch(stmt->StmtKind)
    {
        case stmt_switch:
        {
            uint32_t currentBreakCount = ctx->BreaksCount;

            sema_stmt_switch_t* switchStatement = cast(sema_stmt_switch_t*) stmt;
            BCValue switchExp = MetaCCodegen_doExpression(ctx, switchStatement->SwitchExp);
            metac_bytecode_switch_t* swtch =
                MetaCCodegen_PushSwitch(ctx, switchExp);
            MetaCCodegen_doBlockStmt(ctx, switchStatement->SwitchBody);
            // gen default case if there is one.
            if (swtch->DefaultBody)
            {
                MetaCCodegen_doStatement(ctx, swtch->DefaultBody);
            }
            MetaCCodegen_PopSwitch(ctx, switchExp);
            BCLabel breakLabel = bc->genLabel(c);
            FixupBreaks(ctx, currentBreakCount, breakLabel);
        } break;

        case stmt_block:
        {
            MetaCCodegen_doBlockStmt(ctx, stmt);
        } break;

        case stmt_break:
        {
            ARENA_ARRAY_ADD(ctx->Breaks, bc->beginJmp(c));
        } break;

        case stmt_case:
        {
            sema_stmt_case_t* caseStmt = cast(sema_stmt_case_t*) stmt;
            assert(ctx->SwitchStackCount > 0);
            MetaCCodegen_doCaseStmt(ctx, caseStmt);
        } break;

        case stmt_return:
        {
            sema_stmt_return_t* returnStmt = cast(sema_stmt_return_t*) stmt;
            BCValue retVal = MetaCCodegen_doExpression(ctx, returnStmt->ReturnExp);
            bc->Ret(c, &retVal);
        } break;

        case stmt_exp:
        {
            sema_stmt_exp_t* expStmt = cast(sema_stmt_exp_t*) stmt;
            MetaCCodegen_doExpression(ctx, expStmt->Expression);
        } break;
        case stmt_if:
        {
            sema_stmt_if_t* ifStmt = cast(sema_stmt_if_t*) stmt;
            BCValue cond = MetaCCodegen_doExpression(ctx, ifStmt->IfCond);
            CndJmpBegin cj = bc->beginCndJmp(c, &cond, false);
            {
                MetaCCodegen_doStatement(ctx, ifStmt->IfBody);
            }
            BCAddr skipElse;
            if (METAC_NODE(ifStmt->ElseBody) != emptyNode)
                skipElse =  bc->beginJmp(c);
            bc->endCndJmp(c, &cj, bc->genLabel(c));

            if (METAC_NODE(ifStmt->ElseBody) != emptyNode)
            {
                {
                    MetaCCodegen_doStatement(ctx, ifStmt->ElseBody);
                }
                bc->endJmp(c, skipElse, bc->genLabel(c));
            }
        } break;

        default:
        {
            printf("Statement unsupported %s\n", StatementKind_toChars(stmt->StmtKind));
        } break;
    }
}
