#include "Python.h"
#include "pycore_optimizer.h"
#include "pycore_uops.h"
#include "pycore_uop_ids.h"
#include "internal/pycore_moduleobject.h"

#define op(name, ...) /* NAME is ignored */

typedef struct _Py_UopsPESymbol _Py_UopsPESymbol;
typedef struct _Py_UOpsPEContext _Py_UOpsPEContext;
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
        MATERIALIZE_INST();
        value = GETLOCAL(oparg);
        // We guarantee this will error - just bail and don't optimize it.
        if (sym_is_null(&value)) {
            ctx->done = true;
        }
        this_instr->oparg = oparg + ctx->frame->inline_localsplus_offset_from_caller;
    }

    op(_LOAD_FAST, (-- value)) {
        value = GETLOCAL(oparg);
        sym_set_origin_inst_override(&value, this_instr);
        this_instr->oparg = oparg + ctx->frame->inline_localsplus_offset_from_caller;
    }

    op(_LOAD_FAST_AND_CLEAR, (-- value)) {
        MATERIALIZE_INST();
        value = GETLOCAL(oparg);
        GETLOCAL(oparg) = sym_new_null(ctx);
        sym_set_origin_inst_override(&value, this_instr);
        this_instr->oparg = oparg + ctx->frame->inline_localsplus_offset_from_caller;
    }

    op(_LOAD_CONST, (-- value)) {
        // Should've all been converted by specializer.
        Py_UNREACHABLE();
        // Just to please the code generator that value is defined.
        value = sym_new_const(ctx, NULL);
    }

    op(_LOAD_CONST_INLINE, (ptr/4 -- value)) {
        MATERIALIZE_INST();
        value = sym_new_const(ctx, ptr);
        sym_set_origin_inst_override(&value, this_instr);
    }

    op(_LOAD_CONST_INLINE_BORROW, (ptr/4 -- value)) {
        MATERIALIZE_INST();
        value = sym_new_const(ctx, ptr);
        sym_set_origin_inst_override(&value, this_instr);
    }

    op(_LOAD_CONST_INLINE_WITH_NULL, (ptr/4 -- value, null)) {
        value = sym_new_const(ctx, ptr);
        null = sym_new_null(ctx);
        sym_set_origin_inst_override(&value, this_instr);
        sym_set_origin_inst_override(&null, this_instr);
    }

    op(_LOAD_CONST_INLINE_BORROW_WITH_NULL, (ptr/4 -- value, null)) {
        value = sym_new_const(ctx, ptr);
        null = sym_new_null(ctx);
        sym_set_origin_inst_override(&value, this_instr);
        sym_set_origin_inst_override(&null, this_instr);
    }

    op(_PUSH_NULL, (-- res)) {
        res = sym_new_null(ctx);
        sym_set_origin_inst_override(&res, this_instr);
    }

    op(_STORE_FAST, (value --)) {
        _PyUOpInstruction *origin = sym_get_origin(&value);
        // Gets rid of things like x = x.
        if (sym_is_virtual(&value) &&
            origin != NULL &&
            origin->opcode == _LOAD_FAST &&
            origin->oparg == oparg) {
            // Leave it as virtual.
        }
        else {
            materialize(&value);
            MATERIALIZE_INST();
            GETLOCAL(oparg) = value;
        }
        this_instr->oparg = oparg + ctx->frame->inline_localsplus_offset_from_caller;

    }

    op(_POP_TOP, (value --)) {
        if (!sym_is_virtual(&value)) {
            MATERIALIZE_INST();
        }
    }

    op(_NOP, (--)) {
    }

    op(_CHECK_STACK_SPACE_OPERAND, ( -- )) {
        MATERIALIZE_INST();
    }

    op(_BINARY_OP_NO_ESCAPE, (lhs, rhs -- res)) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        res = sym_new_not_null(ctx);
    }

    op(_BINARY_SUBSCR_INIT_CALL, (container, sub -- new_frame)) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        new_frame = (_Py_UopsPESlot){NULL, NULL};
        ctx->done = true;
    }

    op(_LOAD_ATTR_PROPERTY_FRAME, (fget/4, owner -- new_frame)) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        new_frame = (_Py_UopsPESlot){NULL, NULL};
        ctx->done = true;
    }

    op(_INIT_CALL_PY_EXACT_ARGS, (callable[1], self_or_null[1], args[oparg] -- new_frame)) {
        int argcount = oparg;

        PyCodeObject *co = NULL;
        assert((this_instr + 2)->opcode == _PUSH_FRAME);
        co = get_code_with_logging((this_instr + 2));
        if (co == NULL) {
            ctx->done = true;
            break;
        }


        assert(self_or_null->sym != NULL);
        assert(args != NULL);
        if (sym_is_not_null(self_or_null)) {
            // Bound method fiddling, same as _INIT_CALL_PY_EXACT_ARGS in VM
            args--;
            argcount++;
        }

        _Py_UopsPESlot temp;
        // Materialize inputs, but not the frame creation instruction!
        materialize(self_or_null);
        materialize(callable);
        for (int x = 0; x < argcount; x++) {
            materialize(&args[x]);
        }
        if (sym_is_null(self_or_null) || sym_is_not_null(self_or_null)) {
            temp = (_Py_UopsPESlot) {
                (_Py_UopsPESymbol *) frame_new(ctx, co, 0, args, oparg,
                                               oparg, this_instr, sym_is_not_null(self_or_null)), this_instr,
            };
        }
        else {
            // Not statically known, cannot inline.
            temp = (_Py_UopsPESlot) {
                (_Py_UopsPESymbol *) frame_new(ctx, co, 0, NULL, oparg,
                                               oparg, NULL, 0), NULL
            };
            MATERIALIZE_INST();
         }
        new_frame = temp;
    }

    op(_SAVE_RETURN_OFFSET, (--)) {
        _Py_UopsPESlot frame = stack_pointer[-1];
        if (!sym_is_virtual(&frame)) {
            MATERIALIZE_INST();
        }
        ctx->frame->return_offset = oparg;
        // Else, it's a virtual frame on top.
    }

    op(_PY_FRAME_GENERAL, (callable[1], self_or_null[1], args[oparg] -- new_frame)) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        PyCodeObject *co = NULL;
        assert((this_instr + 2)->opcode == _PUSH_FRAME);
        co = get_code_with_logging((this_instr + 2));
        if (co == NULL) {
            ctx->done = true;
            break;
        }

        _Py_UopsPESlot temp = (_Py_UopsPESlot){(_Py_UopsPESymbol *)frame_new(ctx, co, 0, NULL, 0, oparg, NULL, 0), NULL};
        new_frame = temp;
    }

    op(_PY_FRAME_KW, (callable[1], self_or_null[1], args[oparg], kwnames -- new_frame)) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        new_frame = (_Py_UopsPESlot){NULL, NULL};
        ctx->done = true;
    }

    op(_CHECK_AND_ALLOCATE_OBJECT, (type_version/2, callable[1], null[1], args[oparg] -- init[1], self[1], args[oparg])) {
        (void)type_version;
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        self[0] = sym_new_not_null(ctx);
        init[0] = sym_new_not_null(ctx);
    }

    op(_CREATE_INIT_FRAME, (init[1], self[1], args[oparg] -- init_frame)) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        init_frame = (_Py_UopsPESlot){NULL, NULL};
        ctx->done = true;
    }

    op(_FOR_ITER_GEN_FRAME, (iter -- iter, gen_frame)) {
        MATERIALIZE_INST();
        gen_frame = (_Py_UopsPESlot){NULL, NULL};
        /* We are about to hit the end of the trace */
        ctx->done = true;
    }

    op(_SEND_GEN_FRAME, (receiver, v -- receiver, gen_frame)) {
        gen_frame = (_Py_UopsPESlot){NULL, NULL};
        MATERIALIZE_INST();
        // We are about to hit the end of the trace:
        ctx->done = true;
    }

    op(_PUSH_FRAME, (new_frame --)) {
        SYNC_SP();
        ctx->frame->stack_pointer = stack_pointer;
        _Py_UOpsPEAbstractFrame *old_frame = ctx->frame;
        ctx->frame = (_Py_UOpsPEAbstractFrame *)new_frame.sym;
        ctx->curr_frame_depth++;
        stack_pointer = ((_Py_UOpsPEAbstractFrame *)new_frame.sym)->stack_pointer;
        co = get_code(this_instr);
        if (co == NULL) {
            // should be about to _EXIT_TRACE anyway
            ctx->done = true;
            break;
        }
        ctx->frame->f_funcobj = get_func(this_instr);
        if (ctx->frame->f_funcobj != NULL) {
            ctx->frame->f_executable = ctx->frame->f_funcobj->func_code;
        }

        if (ctx->frame->init_frame_inst != NULL &&
            (ctx->frame->f_funcobj != NULL) &&
            sym_frame_body_is_inlineable(this_instr) &&
            ctx->frame->init_frame_inst->opcode == _INIT_CALL_PY_EXACT_ARGS) {
            DPRINTF(2, "Inlineable\n");
            // Shrink but don't decref --- the new "function" has stolen the
            // values.
            assert(ctx->frame->init_frame_inst != NULL);
            _PyUOpInstruction *initing_inst = ctx->frame->init_frame_inst;
            if (initing_inst->opcode == _INIT_CALL_PY_EXACT_ARGS) {
                // Grow the stack by the number of locals to NULL out
                // Locals to NULL out = frame->locals - locals already on stack.
                REPLACE_OP(initing_inst, _GROW_STACK, initing_inst->oparg, initing_inst->operand0);
                initing_inst->operand1 = ctx->frame->locals_len -
                    (ctx->frame->oparg - (ctx->frame->consumed_self ? 1 : 0));
                initing_inst->is_virtual = false;
            }
            else {
                Py_UNREACHABLE(); // For now.
            }
        }
        else {
#ifdef Py_DEBUG
            if ((ctx->frame->f_funcobj == NULL)) {
                DPRINTF(2, "Inline fail: no executable/func\n");
            }
            else {
                 DPRINTF(2, "Inline fail: escaping\n");
            }
#endif
            MATERIALIZE_INST();
            materialize_ctx(ctx);
            ctx->frame->init_frame_inst = NULL;
            ctx->frame->inline_localsplus_offset_from_caller = 0;
        };

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

    op(_RESUME_CHECK, (--)) {
        // Is NOT virtual
        bool is_virtual = frame_is_virtual(ctx);
        if (!is_virtual) {
            MATERIALIZE_INST();
        }
    }

    op(_RETURN_VALUE, (retval -- res)) {
        // Is NOT virtual.
        bool is_virtual = frame_is_virtual(ctx);
        if (!is_virtual) {
            MATERIALIZE_INST();
            materialize(&retval);
        }
        else {
            DPRINTF(2, "Virtualizing function\n");
            materialize(&retval);
            // The amount to shrink is the number of locals + 2 (callable and null/self).
            REPLACE_OP(this_instr, _SHRINK_STACK, 0, this_instr->operand0);
            this_instr->operand1 = ctx->frame->locals_len + (ctx->frame->consumed_self ? 0 : 1);
            MATERIALIZE_INST();
        }
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
        MATERIALIZE_INST();
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

    op(_YIELD_VALUE, (retval -- value)) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        value = sym_new_unknown(ctx);
    }

    op(_CHECK_FUNCTION, ( -- )) {
        materialize_ctx(ctx);
        MATERIALIZE_INST();
    }

    op(_JUMP_TO_TOP, (--)) {
        MATERIALIZE_INST();
        materialize_ctx(ctx);
        ctx->done = true;
    }

    op(_EXIT_TRACE, (exit_p/4 --)) {
        MATERIALIZE_INST();
        materialize_ctx(ctx);
        (void)exit_p;
        ctx->done = true;
    }

    op(_SET_IP, (instr_ptr/4 --)) {
        MATERIALIZE_INST();
        ctx->frame->instr_ptr = instr_ptr;
    }

    op(_UNPACK_SEQUENCE, (seq -- output[oparg])) {
        /* This has to be done manually */
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        for (int i = 0; i < oparg; i++) {
            output[i] = sym_new_unknown(ctx);
        }
    }

    op(_UNPACK_EX, (seq -- left[oparg & 0xFF], unused, right[oparg >> 8])) {
        /* This has to be done manually */
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        int totalargs = (oparg & 0xFF) + (oparg >> 8) + 1;
        for (int i = 0; i < totalargs; i++) {
            left[i] = sym_new_unknown(ctx);
        }
        (void)right;
    }

    op(_MAYBE_EXPAND_METHOD, (callable[1], self_or_null[1], args[oparg] -- func[1], maybe_self[1], args[oparg])) {
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        func[0] = sym_new_not_null(ctx);
        maybe_self[0] = sym_new_not_null(ctx);
    }

    op(_LOAD_GLOBAL_MODULE_FROM_KEYS, (index/1, globals_keys -- res, null if (oparg & 1))) {
        (void)index;
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        res = sym_new_not_null(ctx);
        null = sym_new_null(ctx);
    }

    op(_LOAD_GLOBAL_BUILTINS_FROM_KEYS, (index/1, builtins_keys -- res, null if (oparg & 1))) {
        (void)index;
        MATERIALIZE_INST();
        MATERIALIZE_INPUTS();
        res = sym_new_not_null(ctx);
        null = sym_new_null(ctx);
    }

    op(_RECONSTRUCT_FRAME, (frame_end_offset_in_localsplus/2, f_executable/4, prev_frame -- new_frame)) {
        new_frame = sym_new_not_null(ctx);
        Py_UNREACHABLE();
    }

    op(_REHYDRATE_FRAME, (f_funcobj/4, instr_ptr/4, new_frame -- new_frame)) {
        Py_UNREACHABLE();
    }

    op (_SET_TOPMOST_FRAME_AND_SHRINK_STACK, (prev_frame -- prev_frame)) {
        (void)(prev_frame);
        Py_UNREACHABLE();
    }

// END BYTECODES //

}
