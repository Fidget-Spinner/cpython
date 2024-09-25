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
        // If the locals is known, we can copy propagate/dead store eliminate it.
        if (value.sym->locals_idx != -1) {
            SET_STATIC_INST();
        }
        else if (value.is_virtual &&
            (sym_is_const(value) && sym_is_const(GETLOCAL(oparg))) &&
            sym_get_const(value) == sym_get_const(GETLOCAL(oparg))) {
            SET_STATIC_INST();
        }
        else {
            reify_shadow_stack(ctx, true);
            _Py_UopsLocalsPlusSlot old_value = value;
            if (sym_is_const(old_value)) {
                value = sym_new_const(ctx, sym_get_const(old_value));
            }
            else {
                value = sym_new_not_null(ctx);
            }
            value.sym->locals_idx = oparg;
            value.is_virtual = false;
        }
        GETLOCAL(oparg) = value;
    }

    override op(_POP_TOP, (pop --)) {
        if (pop.is_virtual) {
            SET_STATIC_INST();
        }
        else {
            reify_shadow_stack(ctx, true);
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
        reify_shadow_stack(ctx, true);
        co = get_code(this_instr);
        if (co == NULL) {
            // might be impossible, but bailing is still safe
            ctx->done = true;
        }

        if ((this_instr+2)->opcode == _UNPACK_SEQUENCE_TWO_TUPLE &&
            (_Py_uop_sym_is_tuple(retval) && retval.sym->tuple_size == 2) &&
            // Check the callee stack has enough space.
            _Py_uop_frame_prev(ctx)->stack_len >= 2 &&
            trace_dest[ctx->n_trace_dest - 1].opcode == BUILD_TUPLE) {
            trace_dest[ctx->n_trace_dest - 1].opcode = _NOP;
            APPEND_OP(_RETURN_N, 2, 0);
            DONT_EMIT_N_INSTRUCTIONS(3);
        }
        SYNC_SP();
        ctx->frame->stack_pointer = stack_pointer;
        frame_pop(ctx);
        stack_pointer = ctx->frame->stack_pointer;
        res = sym_new_unknown(ctx);
    }

    override op(_INIT_CALL_PY_EXACT_ARGS, (callable, self_or_null, args[oparg] -- new_frame)) {
        int argcount = oparg;

        (void)callable;

        PyCodeObject *co = NULL;
        assert((this_instr + 2)->opcode == _PUSH_FRAME);
        uint64_t push_operand = (this_instr + 2)->operand;
        if (push_operand & 1) {
            co = (PyCodeObject *)(push_operand & ~1);
            DPRINTF(3, "code=%p ", co);
            assert(PyCode_Check(co));
        }
        else {
            PyFunctionObject *func = (PyFunctionObject *)push_operand;
            DPRINTF(3, "func=%p ", func);
            if (func == NULL) {
                DPRINTF(3, "\n");
                DPRINTF(1, "Missing function\n");
                ctx->done = true;
                break;
            }
            co = (PyCodeObject *)func->func_code;
            DPRINTF(3, "code=%p ", co);
        }

        assert(self_or_null.sym != NULL);
        assert(args != NULL);
        if (sym_is_not_null(self_or_null)) {
            // Bound method fiddling, same as _INIT_CALL_PY_EXACT_ARGS in VM
            args--;
            argcount++;
        }

        if (sym_is_null(self_or_null) || sym_is_not_null(self_or_null)) {
            new_frame.sym = (_Py_UopsSymbol *)frame_new(ctx, co, 0, args, argcount, false);
        } else {
            new_frame.sym = (_Py_UopsSymbol *)frame_new(ctx, co, 0, NULL, 0, false);

        }
    }

    override op(_SET_IP, (instr_ptr/4 --)) {
        SET_STATIC_INST();
        ctx->frame->instr_ptr = (_Py_CODEUNIT *)instr_ptr;
    }

    override op(_CHECK_VALIDITY_AND_SET_IP, (instr_ptr/4 --)) {
        ctx->frame->instr_ptr = (_Py_CODEUNIT *)instr_ptr;
    }

    override op(_SAVE_RETURN_OFFSET, (--)) {
        ctx->frame->return_offset = oparg;
    }

    override op(_COPY, (bottom, unused[oparg-1] -- bottom, unused[oparg-1], top)) {
        assert(oparg > 0);
        top = sym_new_not_null(ctx);
    }

    // Partial to Max Bernstein from https://github.com/python/cpython/pull/119478/files
    // But also credits to me because I taught him that code :).
    op(_BUILD_TUPLE, (values[oparg] -- tup)) {
        bool all_virtual = oparg <= 6;
        for (int i = 0; i < oparg; i++) {
            all_virtual = all_virtual && values[i].is_virtual;
        }
        if (all_virtual) {
            SET_STATIC_INST();
        }
        else {
            reify_shadow_stack(ctx, true);
        }
        if (oparg <= 6) {
            tup = _Py_uop_sym_new_tuple(ctx, oparg);
            for (int i = 0; i < oparg; i++) {
                _Py_uop_sym_tuple_setitem(ctx, tup, i, values[i]);
            }
            tup.is_virtual = all_virtual;
        }
        else {
            tup = sym_new_not_null(ctx);
        }
    }

// END BYTECODES //

}
