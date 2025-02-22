#ifndef _METAC_CODEGEN_H_
#define _METAC_CODEGEN_H_

#include "../os/compat.h"
#include "../os/metac_alloc.h"
#include "../semantic/metac_sematree.h"
#include "../codegen/metac_vstore.h"

#ifndef AT
#  define AT(...)
#endif

typedef struct metac_bytecode_function_t
{
    uint32_t FunctionIndex;
    void* FuncDeclPtr;
} metac_bytecode_function_t;

typedef struct function_metadata_t
{
    metac_parser_t* Parser;
} function_metadata_t;

enum metac_bytecode_casejmp_kind_t
{
    casejmp_condJmp,
    casejmp_jmp
};
typedef struct metac_bytecode_casejmp_t
{
    enum metac_bytecode_casejmp_kind_t Kind;
    union
    {
        CndJmpBegin cndJmp;
        BCAddr jmp;
    };
} metac_bytecode_casejmp_t;

typedef struct metac_bytecode_switch_t
{
    /// expression to compare case against
    BCValue Exp;

    /// set to a fixup location
    ARENA_ARRAY(metac_bytecode_casejmp_t, PrevCaseJumps)

    /// if non-null this goes at the of the switch
    /// such that we fall through to it
    metac_sema_stmt_t* DefaultBody;
} metac_bytecode_switch_t;

typedef struct metac_external_entry_t
{
    uint32_t hash;
    uint32_t externalSize;

    void* externalAddress;

    BCValue ExtValue;
} metac_external_entry_t;

typedef struct metac_bytecode_ctx_t
{
    /// backend context
    AT(transient) void* c;
    metac_alloc_t Allocator;

    ARENA_ARRAY(metac_bytecode_function_t, Functions)
    ARENA_ARRAY(BCValue, Globals)
    ARENA_ARRAY(metac_external_entry_t, Externals)

    ARENA_ARRAY(BCStructType, StructTypes)
    ARENA_ARRAY(BCEnumType, EnumTypes)
    ARENA_ARRAY(BCPointerType, PtrTypes)
    ARENA_ARRAY(BCArrayType, ArrayTypes)

    tagged_arena_t GlobalMemory;
    uint32_t GlobalMemoryOffset;

    BCValue CompilerInterfaceValue;

    AT(transient) const BackendInterface* gen;

    AT(transient) AT(per_function) ARENA_ARRAY(BCValue, Locals)
    AT(transient) AT(per_function) ARENA_ARRAY(BCValue, Parameters)

    AT(transient) AT(per_function) metac_identifier_table_t* IdentifierTable;
    AT(transient) AT(per_function) metac_sema_state_t* Sema;

    // stores fixup locations for generated by break;
    AT(transient) AT(per_function) ARENA_ARRAY(BCAddr, Breaks)
    AT(transient) AT(per_function) ARENA_ARRAY(BCAddr, Continues)

    AT(transient) AT(per_function) ARENA_ARRAY(metac_bytecode_switch_t, SwitchStack)

    AT(transient) variable_store_t Vstore;

    AT(transient) metac_identifier_table_t KnownVariables;
    AT(transient) metac_identifier_table_t KnownFunctions;
} metac_bytecode_ctx_t;


typedef struct metac_function_bytecode_t metac_function_bytecode_t;
void MetaCCodegen_Init(metac_bytecode_ctx_t* self, metac_alloc_t* parentAlloc);

metac_bytecode_function_t MetaCCodegen_GenerateFunctionFromExp(metac_bytecode_ctx_t* ctx,
                                                               metac_sema_expr_t* expr);


void MetaCCodegen_Begin(metac_bytecode_ctx_t* self, metac_identifier_table_t* idTable, metac_sema_state_t* sema);
void MetaCCodegen_End(metac_bytecode_ctx_t* self);

metac_bytecode_function_t MetaCCodegen_GenerateFunction(metac_bytecode_ctx_t* ctx,
                                                        sema_decl_function_t* function);

uint32_t MetaCCodegen_GetTypeABISize(metac_bytecode_ctx_t* ctx, metac_type_index_t type);
BCType MetaCCodegen_GetBCType(metac_bytecode_ctx_t* ctx, metac_type_index_t type);

typedef enum metac_value_type_t
{
    _Rvalue,
    _Cond,
    _Lvalue,
    _Discard
} metac_value_type_t;


void MetaCCodegen_doGlobal(metac_bytecode_ctx_t* ctx, metac_sema_decl_t* decl, uint32_t idx);

static void MetaCCodegen_doExpr(metac_bytecode_ctx_t* ctx,
                                      metac_sema_expr_t* exp,
                                      BCValue* result,
                                      metac_value_type_t lValue);

uint32_t MetaCCodegen_GetStorageSize(metac_bytecode_ctx_t* ctx, BCType bcType);

long MetaCCodegen_RunFunction(metac_bytecode_ctx_t* self,
                              metac_bytecode_function_t f,
                              metac_alloc_t* interpAlloc,
                              BCHeap* heap,
                              const char* fargs, ...);

void MetaCCodegen_SetDefaultInterface(const BackendInterface* defInterface);
void MetaCCodegen_UnsetDefaultInterface(void);

void MetaCCodegen_Free(metac_bytecode_ctx_t* self);


#endif
