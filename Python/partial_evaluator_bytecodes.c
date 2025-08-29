#include "Python.h"
#include "pycore_optimizer.h"
#include "pycore_uops.h"
#include "pycore_uop_ids.h"
#include "internal/pycore_moduleobject.h"

#define op(name, ...) /* NAME is ignored */

typedef struct JitOptPESymbol JitOptPESymbol;
typedef struct JitOptPEContext JitOptPEContext;
typedef struct _Py_UOpsPEAbstractFrame _Py_UOpsPEAbstractFrame;

/* Shortened forms for convenience */
#define sym_is_not_null _Py_uop_pe_sym_is_not_null
#define sym_is_const _Py_uop_pe_sym_is_const
#define sym_get_const _Py_uop_pe_sym_get_const
#define sym_new_unknown _Py_uop_pe_sym_new_unknown
#define sym_new_not_null _Py_uop_pe_sym_new_not_null
#define sym_is_null _Py_uop_pe_sym_is_null
#define sym_new_const _Py_uop_pe_sym_new_const
#define sym_new_null _Py_uop_pe_sym_new_null
#define sym_set_null(SYM) _Py_uop_pe_sym_set_null(ctx, SYM)
#define sym_set_non_null(SYM) _Py_uop_pe_sym_set_non_null(ctx, SYM)
#define sym_set_const(SYM, CNST) _Py_uop_pe_sym_set_const(ctx, SYM, CNST)
#define sym_is_bottom _Py_uop_pe_sym_is_bottom
#define frame_new _Py_uop_pe_frame_new
#define frame_pop _Py_uop_pe_frame_pop

extern PyCodeObject *get_code(_PyUOpInstruction *op);

static int
dummy_func(void) {

// BEGIN BYTECODES //

    op(_LOAD_FAST_CHECK, (-- value)) {
        COPY_TO_TRACE(this_instr);
        value = GETLOCAL(oparg);
        // We guarantee this will error - just bail and don't optimize it.
        if (sym_is_null(value)) {
            ctx->done = true;
        }
    }

    op(_LOAD_FAST, (-- value)) {
        COPY_TO_TRACE(this_instr);
        value = GETLOCAL(oparg);
    }

    op(_LOAD_FAST_BORROW, (-- value)) {
        COPY_TO_TRACE(this_instr);
        value = GETLOCAL(oparg);
    }

    op(_LOAD_FAST_AND_CLEAR, (-- value)) {
        COPY_TO_TRACE(this_instr);
        value = GETLOCAL(oparg);
        GETLOCAL(oparg) = sym_new_null(ctx);
    }

    op(_LOAD_CONST, (-- value)) {
        // Should've all been converted by specializer.
        Py_UNREACHABLE();
        // Just to please the code generator that value is defined.
        value = NULL;
    }

    op(_LOAD_SMALL_INT, (-- value)) {
        if (is_pe_candidate) {
            ADD_TO_TRACE(_LOAD_TAGGED_INT, 0, oparg, this_instr->target);
            value = sym_new_tagged_int(ctx);
        }
        else {
            COPY_TO_TRACE(this_instr);
            value = sym_new_not_null(ctx);
        }
    }

    op(_LOAD_CONST_INLINE, (ptr/4 -- value)) {
        if (is_pe_candidate) {
            ADD_TO_TRACE(_LOAD_TAGGED_INT, 0, oparg, this_instr->target);
            value = sym_new_tagged_int(ctx);
        }
        else {
            COPY_TO_TRACE(this_instr);
            value = sym_new_not_null(ctx);
        }
    }

    op(_LOAD_CONST_INLINE_BORROW, (ptr/4 -- value)) {
        if (is_pe_candidate) {
            ADD_TO_TRACE(_LOAD_TAGGED_INT, 0, oparg, this_instr->target);
            value = sym_new_tagged_int(ctx);
        }
        else {
            COPY_TO_TRACE(this_instr);
            value = sym_new_not_null(ctx);
        }
    }

    op(_BINARY_OP_ADD_INT, (left, right -- res)) {
        if (sym_is_tagged_int(left) && sym_is_tagged_int(right)) {
            ADD_TO_TRACE(_BINARY_OP_ADD_TAGGED_INT, 0, oparg, this_instr->target);
            res = sym_new_tagged_int(ctx);
        }
        else {
            COPY_TO_TRACE(this_instr);
            res = sym_new_not_null(ctx);
        }
    }

    op(_STORE_FAST, (value --)) {
        COPY_TO_TRACE(this_instr);
        GETLOCAL(oparg) = value;
    }

    op(_POP_TOP, (value --)) {
        COPY_TO_TRACE(this_instr);
    }

    op(_BINARY_OP_SUBSCR_INIT_CALL, (container, sub, getitem  -- new_frame)) {
        COPY_TO_TRACE(this_instr);
        new_frame = NULL;
        ctx->done = true;
    }

    op(_LOAD_ATTR_PROPERTY_FRAME, (fget/4, owner -- new_frame)) {
        COPY_TO_TRACE(this_instr);
        new_frame = NULL;
        ctx->done = true;
    }

    op(_INIT_CALL_PY_EXACT_ARGS, (callable, self_or_null, args[oparg] -- new_frame)) {
        COPY_TO_TRACE(this_instr);
        int argcount = oparg;

        PyCodeObject *co = NULL;
        assert((this_instr + 2)->opcode == _PUSH_FRAME);
        co = get_code_with_logging((this_instr + 2));
        if (co == NULL) {
            ctx->done = true;
            break;
        }

        assert(self_or_null != NULL);
        assert(args != NULL);
        if (sym_is_not_null(self_or_null)) {
            // Bound method fiddling, same as _INIT_CALL_PY_EXACT_ARGS in VM
            args--;
            argcount++;
        }

        if (sym_is_null(self_or_null) || sym_is_not_null(self_or_null)) {
            new_frame = (JitOptPESymbol *)frame_new(ctx, co, 0, args, argcount);
        } else {
            new_frame = (JitOptPESymbol *)frame_new(ctx, co, 0, NULL, 0);
        }
    }

    op(_PY_FRAME_GENERAL, (callable, self_or_null, args[oparg] -- new_frame)) {
        COPY_TO_TRACE(this_instr);
        PyCodeObject *co = NULL;
        assert((this_instr + 2)->opcode == _PUSH_FRAME);
        co = get_code_with_logging((this_instr + 2));
        if (co == NULL) {
            ctx->done = true;
            break;
        }

        new_frame = (JitOptPESymbol *)frame_new(ctx, co, 0, NULL, 0);
    }

    op(_PY_FRAME_KW, (callable, self_or_null, args[oparg], kwnames -- new_frame)) {
        COPY_TO_TRACE(this_instr);
        new_frame = NULL;
        ctx->done = true;
    }

    op(_CREATE_INIT_FRAME, (init, self, args[oparg] -- init_frame)) {
        COPY_TO_TRACE(this_instr);
        init_frame = NULL;
        ctx->done = true;
    }

    op(_FOR_ITER_GEN_FRAME, (unused, unused -- unused, unused, gen_frame)) {
        COPY_TO_TRACE(this_instr);
        gen_frame = NULL;
        /* We are about to hit the end of the trace */
        ctx->done = true;
    }

    op(_SEND_GEN_FRAME, (unused, unused -- unused, gen_frame)) {
        COPY_TO_TRACE(this_instr);
        gen_frame = NULL;
        // We are about to hit the end of the trace:
        ctx->done = true;
    }

    op(_PUSH_FRAME, (new_frame --)) {
        COPY_TO_TRACE(this_instr);
        SYNC_SP();
        ctx->frame->stack_pointer = stack_pointer;
        ctx->frame = (_Py_UOpsPEAbstractFrame *)new_frame;
        ctx->curr_frame_depth++;
        stack_pointer = ((_Py_UOpsPEAbstractFrame *)new_frame)->stack_pointer;
        co = get_code(this_instr);
        if (co == NULL) {
            // should be about to _EXIT_TRACE anyway
            ctx->done = true;
            break;
        }

        /* Stack space handling */
        int framesize = co->co_framesize;
        assert(framesize > 0);
        curr_space += framesize;
        if (curr_space < 0 || curr_space > INT32_MAX) {
            // won't fit in signed 32-bit int
            ctx->done = true;
            break;
        }
        max_space = curr_space > max_space ? curr_space : max_space;
        if (first_valid_check_stack == NULL) {
            first_valid_check_stack = corresponding_check_stack;
        }
        else if (corresponding_check_stack) {
            // delete all but the first valid _CHECK_STACK_SPACE
            corresponding_check_stack->opcode = _NOP;
        }
        corresponding_check_stack = NULL;
    }

    op(_RETURN_VALUE, (retval -- res)) {
        COPY_TO_TRACE(this_instr);
        SYNC_SP();
        ctx->frame->stack_pointer = stack_pointer;
        frame_pop(ctx);
        stack_pointer = ctx->frame->stack_pointer;
        res = retval;

        /* Stack space handling */
        assert(corresponding_check_stack == NULL);
        assert(co != NULL);
        int framesize = co->co_framesize;
        assert(framesize > 0);
        assert(framesize <= curr_space);
        curr_space -= framesize;

        co = get_code(this_instr);
        if (co == NULL) {
            // might be impossible, but bailing is still safe
            ctx->done = true;
        }
    }

    op(_RETURN_GENERATOR, ( -- res)) {
        COPY_TO_TRACE(this_instr);
        SYNC_SP();
        ctx->frame->stack_pointer = stack_pointer;
        frame_pop(ctx);
        stack_pointer = ctx->frame->stack_pointer;
        res = sym_new_unknown(ctx);

        /* Stack space handling */
        assert(corresponding_check_stack == NULL);
        assert(co != NULL);
        int framesize = co->co_framesize;
        assert(framesize > 0);
        assert(framesize <= curr_space);
        curr_space -= framesize;

        co = get_code(this_instr);
        if (co == NULL) {
            // might be impossible, but bailing is still safe
            ctx->done = true;
        }
    }

    op(_UNPACK_SEQUENCE, (seq -- values[oparg], top[0])) {
        COPY_TO_TRACE(this_instr);
        (void)top;
        /* This has to be done manually */
        for (int i = 0; i < oparg; i++) {
            values[i] = sym_new_unknown(ctx);
        }
    }

    op(_UNPACK_EX, (seq -- values[oparg & 0xFF], unused, unused[oparg >> 8], top[0])) {
        COPY_TO_TRACE(this_instr);
        (void)top;
        /* This has to be done manually */
        int totalargs = (oparg & 0xFF) + (oparg >> 8) + 1;
        for (int i = 0; i < totalargs; i++) {
            values[i] = sym_new_unknown(ctx);
        }
    }

    op(_JUMP_TO_TOP, (--)) {
        COPY_TO_TRACE(this_instr);
        ctx->done = true;
    }

    op(_EXIT_TRACE, (exit_p/4 --)) {
        COPY_TO_TRACE(this_instr);
        (void)exit_p;
        ctx->done = true;
    }

// END BYTECODES //

}