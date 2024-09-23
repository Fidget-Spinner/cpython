#include "Python.h"
#include "pycore_optimizer.h"
#include "pycore_uops.h"
#include "pycore_uop_ids.h"
#include "internal/pycore_moduleobject.h"

#define op(name, ...) /* NAME is ignored */

typedef struct _Py_UopsSymbol _Py_UopsSymbol;
typedef struct _Py_UOpsContext _Py_UOpsContext;
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
#define sym_is_bottom _Py_uop_sym_is_bottom
#define frame_new _Py_uop_frame_new
#define frame_pop _Py_uop_frame_pop

extern int
optimize_to_bool(
    _PyUOpInstruction *this_instr,
    _Py_UOpsContext *ctx,
    _Py_UopsSymbol *value,
    _Py_UopsSymbol **result_ptr);

extern void
eliminate_pop_guard(_PyUOpInstruction *this_instr, bool exit);

extern PyCodeObject *get_code(_PyUOpInstruction *op);

static int
dummy_func(void) {

// BEGIN BYTECODES //

    override op(_LOAD_FAST, (-- value)) {
        value = GETLOCAL(oparg);
        sym_set_locals_idx(value, oparg);
        SET_STATIC_INST();
        value.is_virtual = true;
    }

    override op(_LOAD_FAST_AND_CLEAR, (-- value)) {
        value = GETLOCAL(oparg);
        GETLOCAL(oparg) = sym_new_null(ctx);
    }

    override op(_LOAD_CONST, (-- value)) {
        // Should've all been converted by specializer.
        Py_UNREACHABLE();
    }

    override op(_LOAD_CONST_INLINE, (ptr/4 -- value)) {
        value = sym_new_const(ctx, ptr);
        SET_STATIC_INST();
        value.is_virtual = true;
    }

    override op(_LOAD_CONST_INLINE_BORROW, (ptr/4 -- value)) {
        value = sym_new_const(ctx, ptr);
        SET_STATIC_INST();
        value.is_virtual = true;
    }

    override op(_STORE_FAST, (value --)) {
        // Gets rid of stores by the same load
        if (value.is_virtual && oparg == sym_get_locals_idx(value)) {
            SET_STATIC_INST();
        }
        else {
            reify_shadow_stack(ctx);
            value.is_virtual = false;
        }
        GETLOCAL(oparg) = value;
    }

    override op(_POP_TOP, (pop --)) {
        if (pop.is_virtual) {
            SET_STATIC_INST();
        }
        else {
            reify_shadow_stack(ctx);
        }
    }

    override op(_NOP, (--)) {
        SET_STATIC_INST();
    }

    override op(_CHECK_STACK_SPACE_OPERAND, ( -- )) {
        (void)framesize;
    }

    override op(_PUSH_FRAME, (new_frame -- unused if (0))) {
        SYNC_SP();
        APPEND_OP(_SET_IP, 0, (uintptr_t)ctx->frame->instr_ptr);
        ctx->frame->stack_pointer = stack_pointer;
        ctx->frame = (_Py_UOpsAbstractFrame *)new_frame.sym;
        ctx->curr_frame_depth++;
        stack_pointer = ((_Py_UOpsAbstractFrame *)new_frame.sym)->stack_pointer;
        co = get_code(this_instr);
        if (co == NULL) {
            // should be about to _EXIT_TRACE anyway
            ctx->done = true;
            break;
        }
    }

    override op(_RETURN_GENERATOR, ( -- res)) {
        SYNC_SP();
        APPEND_OP(_SET_IP, 0, (uintptr_t)ctx->frame->instr_ptr);
        ctx->frame->stack_pointer = stack_pointer;
        frame_pop(ctx);
        stack_pointer = ctx->frame->stack_pointer;
        res = sym_new_unknown(ctx);

        co = get_code(this_instr);
        if (co == NULL) {
            // might be impossible, but bailing is still safe
            ctx->done = true;
        }
    }

    override op(_RETURN_VALUE, (retval -- res)) {
        SYNC_SP();
        ctx->frame->stack_pointer = stack_pointer;
        frame_pop(ctx);
        stack_pointer = ctx->frame->stack_pointer;
        res = retval;

        co = get_code(this_instr);
        if (co == NULL) {
            // might be impossible, but bailing is still safe
            ctx->done = true;
        }
    }

    override op(_SET_IP, (instr_ptr/4 --)) {
        SET_STATIC_INST();
        ctx->frame->instr_ptr = (_Py_CODEUNIT *)instr_ptr;
    }

    override op(_CHECK_VALIDITY_AND_SET_IP, (instr_ptr/4 --)) {
        ctx->frame->instr_ptr = (_Py_CODEUNIT *)instr_ptr;
    }

// END BYTECODES //

}
