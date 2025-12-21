// This file is the backwards pass of optimizer_bytecodes.c. Essentially,
// everything here is done in reverse. This means the outputs and inputs of the
// bytecodes DSL is swapped as well.
// For example:
// op(_LOAD_FAST, (-- value))
// actually means _LOAD_FAST takes in a single intput --- `value`!.
// Instead of a forward pass where we gain information to see what we can deduce
// about the world to produce a set of outputs, the backward pass gains
// information about what is *required* of the world for a given set of
// inputs. It is the dual of the forward pass!
// Instructions are also emitted in reverse order.
#include "Python.h"
#include "pycore_optimizer.h"
#include "pycore_uops.h"
#include "pycore_uop_ids.h"
#include "internal/pycore_moduleobject.h"

#define op(name, ...) /* NAME is ignored */

typedef struct _Py_UOpsAbstractFrame _Py_UOpsAbstractFrame;

/* Shortened forms for convenience */
#define sym_is_not_null _Py_uop_sym_is_not_null
#define sym_is_const _Py_uop_sym_is_const
#define sym_get_const _Py_uop_sym_get_const
#define sym_new_unknown _Py_uop_sym_new_unknown
#define sym_new_not_null _Py_uop_sym_new_not_null
#define sym_new_type _Py_uop_sym_new_type
#define sym_is_null _Py_uop_sym_is_null
#define sym_new_const _Py_uop_sym_new_const
#define sym_new_null _Py_uop_sym_new_null
#define sym_matches_type _Py_uop_sym_matches_type
#define sym_matches_type_version _Py_uop_sym_matches_type_version
#define sym_get_type _Py_uop_sym_get_type
#define sym_has_type _Py_uop_sym_has_type
#define sym_set_null(SYM) _Py_uop_sym_set_null(ctx, SYM)
#define sym_set_non_null(SYM) _Py_uop_sym_set_non_null(ctx, SYM)
#define sym_set_type(SYM, TYPE) _Py_uop_sym_set_type(ctx, SYM, TYPE)
#define sym_set_type_version(SYM, VERSION) _Py_uop_sym_set_type_version(ctx, SYM, VERSION)
#define sym_set_const(SYM, CNST) _Py_uop_sym_set_const(ctx, SYM, CNST)
#define sym_set_compact_int(SYM) _Py_uop_sym_set_compact_int(ctx, SYM)
#define sym_is_bottom _Py_uop_sym_is_bottom
#define frame_new _Py_uop_frame_new
#define frame_pop _Py_uop_frame_pop
#define sym_new_tuple _Py_uop_sym_new_tuple
#define sym_tuple_getitem _Py_uop_sym_tuple_getitem
#define sym_tuple_length _Py_uop_sym_tuple_length
#define sym_is_immortal _Py_uop_symbol_is_immortal
#define sym_new_compact_int _Py_uop_sym_new_compact_int
#define sym_is_compact_int _Py_uop_sym_is_compact_int
#define sym_new_truthiness _Py_uop_sym_new_truthiness

extern int
optimize_to_bool(
    _PyUOpInstruction *this_instr,
    JitOptContext *ctx,
    JitOptSymbol *value,
    JitOptSymbol **result_ptr);

extern void
eliminate_pop_guard(_PyUOpInstruction *this_instr, bool exit);

extern PyCodeObject *get_code(_PyUOpInstruction *op);

static int
dummy_func(void) {

    PyCodeObject *co;
    int oparg;
    JitOptSymbol *flag;
    JitOptSymbol *left;
    JitOptSymbol *right;
    JitOptSymbol *value;
    JitOptSymbol *res;
    JitOptSymbol *iter;
    JitOptSymbol *top;
    JitOptSymbol *bottom;
    _Py_UOpsAbstractFrame *frame;
    _Py_UOpsAbstractFrame *new_frame;
    JitOptContext *ctx;
    _PyUOpInstruction *this_instr;
    _PyBloomFilter *dependencies;
    int modified;
    int curr_space;
    int max_space;
    _PyUOpInstruction *first_valid_check_stack;
    _PyUOpInstruction *corresponding_check_stack;

// BEGIN BYTECODES //

    op(_LOAD_FAST_CHECK, (-- value)) {
        EMIT_OP_FROM_INST(this_instr);
        // We guarantee this will error - just bail and don't optimize it.
        if (sym_is_null(value)) {
            ctx->done = true;
        }
        value = GETLOCAL(oparg);
    }

    op(_LOAD_FAST, (-- value)) {
        EMIT_OP_FROM_INST(this_instr);
        value = GETLOCAL(oparg);
    }

    op(_LOAD_FAST_BORROW, (-- value)) {
        EMIT_OP_FROM_INST(this_instr);
        value = GETLOCAL(oparg);
    }

    op(_LOAD_FAST_AND_CLEAR, (-- value)) {
        EMIT_OP_FROM_INST(this_instr);
        value = GETLOCAL(oparg);
        JitOptRef temp = sym_new_null(ctx);
        GETLOCAL(oparg) = temp;
    }

    op(_STORE_FAST, (value --)) {
        EMIT_OP_FROM_INST(this_instr);
        GETLOCAL(oparg) = value;
    }

    op(_CREATE_INIT_FRAME, (init, self, args[oparg] -- init_frame)) {
        EMIT_OP_FROM_INST(this_instr);
        ctx->done = true;
        init_frame = PyJitRef_NULL;
    }

    op(_RETURN_VALUE, (retval -- res)) {
        EMIT_OP_FROM_INST(this_instr);
        res = PyJitRef_StripReferenceInfo(retval);
        // For now, reconstruction with multiple frames is
        // hard to reason about efficiently.
        // So we leave that for later.
        ctx->done = true;
    }

    op(_RETURN_GENERATOR, ( -- res)) {
        EMIT_OP_FROM_INST(this_instr);
        res = sym_new_unknown(ctx);
        ctx->done = true;
    }

    op(_YIELD_VALUE, (retval -- value)) {
        EMIT_OP_FROM_INST(this_instr);
        value = PyJitRef_StripReferenceInfo(retval);
        ctx->done = true;
    }

    op(_FOR_ITER_GEN_FRAME, (unused, unused -- unused, unused, gen_frame)) {
        EMIT_OP_FROM_INST(this_instr);
        gen_frame = PyJitRef_NULL;
        /* We are about to hit the end of the trace */
        ctx->done = true;
    }

    op(_SEND_GEN_FRAME, (unused, unused -- unused, gen_frame)) {
        EMIT_OP_FROM_INST(this_instr);
        gen_frame = PyJitRef_NULL;
        // We are about to hit the end of the trace:
        ctx->done = true;
    }

    op(_PUSH_FRAME, (new_frame -- )) {
        EMIT_OP_FROM_INST(this_instr);
        new_frame = sym_new_unknown(ctx);
        // For now, reconstruction with multiple frames is
        // hard to reason about efficiently.
        // So we leave that for later.
        ctx->done = true;
    }



// END BYTECODES //

}
