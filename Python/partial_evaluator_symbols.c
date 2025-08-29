#ifdef _Py_TIER2

#include "Python.h"

#include "pycore_code.h"
#include "pycore_frame.h"
#include "pycore_long.h"
#include "pycore_optimizer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Symbols
   =======
   Documentation TODO gh-120619
 */

#ifdef Py_DEBUG
static inline int get_lltrace(void) {
    char *uop_debug = Py_GETENV("PYTHON_OPT_DEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
    return lltrace;
}
#define DPRINTF(level, ...) \
    if (get_lltrace() >= (level)) { printf(__VA_ARGS__); }
#else
#define DPRINTF(level, ...)
#endif


static JitOptPESymbol NO_SPACE_SYMBOL = {
    .tag = JIT_PE_BOTTOM_TAG
};

static JitOptPESymbol *
allocation_base(JitOptPEContext *ctx)
{
    return ctx->p_arena.arena;
}

static inline JitOptPESymbol *
out_of_space(JitOptPEContext *ctx)
{
    ctx->done = true;
    ctx->out_of_space = true;
    return &NO_SPACE_SYMBOL;
}

static JitOptPESymbol *
sym_new(JitOptPEContext *ctx)
{
    JitOptPESymbol *self = &ctx->p_arena.arena[ctx->p_arena.p_curr_number];
    if (ctx->p_arena.p_curr_number >= ctx->p_arena.p_max_number) {
        // OPT_STAT_INC(optimizer_failure_reason_no_memory);
        DPRINTF(1, "out of space for symbolic expression type\n");
        return NULL;
    }
    ctx->p_arena.p_curr_number++;
    self->tag = JIT_SYM_UNKNOWN_TAG;;
    return self;
}

static inline void
sym_set_bottom(JitOptPEContext *ctx, JitOptPESymbol *sym)
{
    sym->tag = JIT_SYM_BOTTOM_TAG;
    ctx->done = true;
    ctx->contradiction = true;
}

bool
_Py_uop_pe_sym_is_bottom(JitOptPESymbol *sym)
{
    return sym->tag == JIT_PE_BOTTOM_TAG;
}

bool
_Py_uop_pe_sym_is_not_null(JitOptPESymbol *sym) {
    return sym->tag == JIT_PE_NON_NULL_TAG || sym->tag > JIT_PE_BOTTOM_TAG;
}

bool
_Py_uop_pe_sym_is_null(JitOptPESymbol *ref)
{
    return ref->tag == JIT_PE_NULL_TAG;
}


void
_Py_uop_pe_sym_set_null(JitOptPEContext *ctx, JitOptPESymbol *sym)
{
    if (sym->tag == JIT_PE_UNKNOWN_TAG) {
        sym->tag = JIT_PE_NULL_TAG;
    }
    else if (sym->tag > JIT_PE_NULL_TAG) {
        sym_set_bottom(ctx, sym);
    }
}

void
_Py_uop_pe_sym_set_non_null(JitOptPEContext *ctx, JitOptPESymbol *sym)
{
    if (sym->tag == JIT_PE_UNKNOWN_TAG) {
        sym->tag = JIT_PE_NON_NULL_TAG;
    }
    else if (sym->tag == JIT_PE_NULL_TAG) {
        sym_set_bottom(ctx, sym);
    }
}

JitOptPESymbol *
_Py_uop_pe_sym_new_unknown(JitOptPEContext *ctx)
{
    JitOptPESymbol *res = sym_new(ctx);
    if (res == NULL) {
        return out_of_space(ctx);
    }
    return res;
}

JitOptPESymbol *
_Py_uop_pe_sym_new_null(JitOptPEContext *ctx)
{
    JitOptPESymbol *null_sym = sym_new(ctx);
    if (null_sym == NULL) {
        return out_of_space(ctx);
    }
    _Py_uop_pe_sym_set_null(ctx, null_sym);
    return null_sym;
}

JitOptPESymbol *
_Py_uop_pe_sym_new_not_null(JitOptPEContext *ctx)
{
    JitOptPESymbol *res = sym_new(ctx);
    if (res == NULL) {
        return out_of_space(ctx);
    }
    res->tag = JIT_PE_NON_NULL_TAG;
    return res;
}

JitOptPESymbol *
_Py_uop_pe_sym_new_tagged_int(JitOptPEContext *ctx)
{
    JitOptPESymbol *sym = sym_new(ctx);
    if (sym == NULL) {
        return out_of_space(ctx);
    }
    sym->tag = JIT_PE_TAGGED_INT_TAG;
    return sym;
}

bool
_Py_uop_pe_sym_is_tagged_int(JitOptPESymbol *sym)
{
    return (bool)(sym->tag == JIT_PE_TAGGED_INT_TAG);
}

// 0 on success, -1 on error.
_Py_UOpsPEAbstractFrame *
_Py_uop_pe_frame_new(
    JitOptPEContext *ctx,
    PyCodeObject *co,
    int curr_stackentries,
    JitOptPESymbol **args,
    int arg_len)
{
    assert(ctx->curr_frame_depth < MAX_ABSTRACT_FRAME_DEPTH);
    _Py_UOpsPEAbstractFrame *frame = &ctx->frames[ctx->curr_frame_depth];

    frame->stack_len = co->co_stacksize;
    frame->locals_len = co->co_nlocalsplus;

    frame->locals = ctx->n_consumed;
    frame->stack = frame->locals + co->co_nlocalsplus;
    frame->stack_pointer = frame->stack + curr_stackentries;
    ctx->n_consumed = ctx->n_consumed + (co->co_nlocalsplus + co->co_stacksize);
    if (ctx->n_consumed >= ctx->limit) {
        ctx->done = true;
        ctx->out_of_space = true;
        return NULL;
    }

    // Initialize with the initial state of all local variables
    for (int i = 0; i < arg_len; i++) {
        frame->locals[i] = args[i];
    }

    for (int i = arg_len; i < co->co_nlocalsplus; i++) {
        JitOptPESymbol *local = _Py_uop_pe_sym_new_unknown(ctx);
        frame->locals[i] = local;
    }


    // Initialize the stack as well
    for (int i = 0; i < curr_stackentries; i++) {
        JitOptPESymbol *stackvar = _Py_uop_pe_sym_new_unknown(ctx);
        frame->stack[i] = stackvar;
    }

    return frame;
}

void
_Py_uop_pe_abstractcontext_fini(JitOptPEContext *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->curr_frame_depth = 0;
    int tys = ctx->p_arena.p_curr_number;
}

void
_Py_uop_pe_abstractcontext_init(JitOptPEContext *ctx)
{
    static_assert(sizeof(JitOptPESymbol) <= 3 * sizeof(uint64_t), "JitOptPESymbol has grown");
    ctx->limit = ctx->locals_and_stack + MAX_ABSTRACT_INTERP_SIZE;
    ctx->n_consumed = ctx->locals_and_stack;
#ifdef Py_DEBUG // Aids debugging a little. There should never be NULL in the abstract interpreter.
    for (int i = 0 ; i < MAX_ABSTRACT_INTERP_SIZE; i++) {
        ctx->locals_and_stack[i] = NULL;
    }
#endif

    // Setup the arena for sym expressions.
    ctx->p_arena.p_curr_number = 0;
    ctx->p_arena.p_max_number = TY_ARENA_SIZE;

    // Frame setup
    ctx->curr_frame_depth = 0;

    // Ctx signals.
    // Note: this must happen before frame_new, as it might override
    // the result should frame_new set things to bottom.
    ctx->done = false;
    ctx->out_of_space = false;
    ctx->contradiction = false;
}

int
_Py_uop_pe_frame_pop(JitOptPEContext *ctx)
{
    _Py_UOpsPEAbstractFrame *frame = ctx->frame;
    ctx->n_consumed = frame->locals;
    ctx->curr_frame_depth--;
    assert(ctx->curr_frame_depth >= 1);
    ctx->frame = &ctx->frames[ctx->curr_frame_depth - 1];

    return 0;
}

#define TEST_PREDICATE(PRED, MSG) \
do { \
    if (!(PRED)) { \
        PyErr_SetString( \
            PyExc_AssertionError, \
            (MSG)); \
        goto fail; \
    } \
} while (0)

PyObject *
_Py_uop_pe_symbols_test(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(ignored))
{
    JitOptPEContext context;
    JitOptPEContext *ctx = &context;
    _Py_uop_pe_abstractcontext_init(ctx);
    PyObject *val_42 = NULL;
    PyObject *val_43 = NULL;
    PyObject *val_big = NULL;
    PyObject *tuple = NULL;

    // Use a single 'sym' variable so copy-pasting tests is easier.
    JitOptPESymbol *ref = _Py_uop_pe_sym_new_unknown(ctx);
    if (ref == NULL) {
        goto fail;
    }
    TEST_PREDICATE(!_Py_uop_pe_sym_is_null(ref), "top is NULL");
    TEST_PREDICATE(!_Py_uop_pe_sym_is_not_null(ref), "top is not NULL");
    TEST_PREDICATE(!_Py_uop_pe_sym_is_bottom(ref), "top is bottom");

    _Py_uop_pe_abstractcontext_fini(ctx);
    Py_DECREF(val_42);
    Py_DECREF(val_43);
    Py_DECREF(val_big);
    Py_DECREF(tuple);
    Py_RETURN_NONE;

fail:
    _Py_uop_pe_abstractcontext_fini(ctx);
    Py_XDECREF(val_42);
    Py_XDECREF(val_43);
    Py_XDECREF(val_big);
    Py_DECREF(tuple);
    return NULL;
}

#endif /* _Py_TIER2 */