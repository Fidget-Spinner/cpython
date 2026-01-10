#ifndef Py_INTERNAL_OPTIMIZER_TYPES_H
#define Py_INTERNAL_OPTIMIZER_TYPES_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_uop.h"  // UOP_MAX_TRACE_LENGTH
#include "pycore_uop_ids.h"

// Holds locals, stack, locals, stack ... (in that order)
#define MAX_ABSTRACT_INTERP_SIZE 512

#define TY_ARENA_SIZE (UOP_MAX_TRACE_LENGTH * 5)

#define EXPR_ARENA_SIZE (UOP_MAX_TRACE_LENGTH / 4)

// Need extras for root frame and for overflow frame (see TRACE_STACK_PUSH())
#define MAX_ABSTRACT_FRAME_DEPTH (16)

// The maximum number of side exits that we can take before requiring forward
// progress (and inserting a new ENTER_EXECUTOR instruction). In practice, this
// is the "maximum amount of polymorphism" that an isolated trace tree can
// handle before rejoining the rest of the program.
#define MAX_CHAIN_DEPTH 4

/* Symbols */
/* See explanation in optimizer_symbols.c */


typedef enum _JitSymType {
    JIT_SYM_UNKNOWN_TAG = 1,
    JIT_SYM_NULL_TAG = 2,
    JIT_SYM_NON_NULL_TAG = 3,
    JIT_SYM_BOTTOM_TAG = 4,
    JIT_SYM_TYPE_VERSION_TAG = 5,
    JIT_SYM_KNOWN_CLASS_TAG = 6,
    JIT_SYM_KNOWN_VALUE_TAG = 7,
    JIT_SYM_TUPLE_TAG = 8,
    JIT_SYM_TRUTHINESS_TAG = 9,
    JIT_SYM_COMPACT_INT = 10,
} JitSymType;

typedef struct _jit_opt_known_class {
    uint8_t tag;
    uint32_t version;
    PyTypeObject *type;
} JitOptKnownClass;

typedef struct _jit_opt_known_version {
    uint8_t tag;
    uint32_t version;
} JitOptKnownVersion;

typedef struct _jit_opt_known_value {
    uint8_t tag;
    PyObject *value;
} JitOptKnownValue;

#define MAX_SYMBOLIC_TUPLE_SIZE 10

typedef struct _jit_opt_tuple {
    uint8_t tag;
    uint8_t length;
    uint16_t items[MAX_SYMBOLIC_TUPLE_SIZE];
} JitOptTuple;

typedef struct {
    uint8_t tag;
    bool invert;
    uint16_t value;
} JitOptTruthiness;

typedef struct {
    uint8_t tag;
} JitOptCompactInt;

typedef union _jit_opt_symbol {
    uint8_t tag;
    JitOptKnownClass cls;
    JitOptKnownValue value;
    JitOptKnownVersion version;
    JitOptTuple tuple;
    JitOptTruthiness truthiness;
    JitOptCompactInt compact;
} JitOptSymbol;

// This mimics the _PyStackRef API
typedef union {
    uintptr_t bits;
} JitOptRef;

typedef struct _Py_UOpsAbstractFrame {
    bool globals_watched;
    // The version number of the globals dicts, once checked. 0 if unchecked.
    uint32_t globals_checked_version;
    // Max stacklen
    int stack_len;
    int locals_len;
    PyFunctionObject *func;
    PyCodeObject *code;

    JitOptRef *stack_pointer;
    JitOptRef *stack;
    JitOptRef *locals;
} _Py_UOpsAbstractFrame;

typedef struct ty_arena {
    int ty_curr_number;
    int ty_max_number;
    JitOptSymbol arena[TY_ARENA_SIZE];
} ty_arena;

// For now, we only support up to binary expressions
#define EXPR_MAX_NUM_OPERANDS_SUPPORTED 2

typedef struct JitOptExpr {
    _PyUOpInstruction *inst;
    JitOptRef args[EXPR_MAX_NUM_OPERANDS_SUPPORTED];
    // Next expression of the same kind in the linked list.
    struct JitOptExpr *next;
} JitOptExpr;

typedef struct expr_arena {
    int expr_curr_number;
    JitOptExpr arena[EXPR_ARENA_SIZE];
} expr_arena;

typedef struct _JitOptContext {
    char done;
    char out_of_space;
    bool contradiction;
    // Has the builtins dict been watched?
    bool builtins_watched;
    // The current "executing" frame.
    _Py_UOpsAbstractFrame *frame;
    _Py_UOpsAbstractFrame frames[MAX_ABSTRACT_FRAME_DEPTH];
    int curr_frame_depth;

    // Arena for the symbolic types.
    ty_arena t_arena;

    // Arena for symbolic expressions.
    // For each uop ID, we point to a linked list entry in e_arena.
    // When searching up an expression, we thus only need to find
    // the previous expressions in that slot for that uop.
    // This follows LuaJIT 2's design.
    JitOptExpr *exprs[MAX_UOP_ID + 1];
    expr_arena e_arena;

    JitOptRef *n_consumed;
    JitOptRef *limit;
    JitOptRef locals_and_stack[MAX_ABSTRACT_INTERP_SIZE];
} JitOptContext;


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_OPTIMIZER_TYPES_H */
