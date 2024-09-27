
/*
 * This file contains the support code for CPython's uops optimizer.
 * It also performs some simple optimizations.
 * It performs a traditional data-flow analysis[1] over the trace of uops.
 * Using the information gained, it chooses to emit, or skip certain instructions
 * if possible.
 *
 * [1] For information on data-flow analysis, please see
 * https://clang.llvm.org/docs/DataFlowAnalysisIntro.html
 *
 * */
#include "Python.h"
#include "opcode.h"
#include "pycore_dict.h"
#include "pycore_interp.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_uop_metadata.h"
#include "pycore_dict.h"
#include "pycore_long.h"
#include "pycore_optimizer.h"
#include "pycore_object.h"
#include "pycore_dict.h"
#include "pycore_function.h"
#include "pycore_uop_metadata.h"
#include "pycore_uop_ids.h"
#include "pycore_range.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint-gcc.h>

#ifdef Py_DEBUG
    extern const char *_PyUOpName(int index);
    extern void _PyUOpPrint(const _PyUOpInstruction *uop);
    static const char *const DEBUG_ENV = "PYTHON_OPT_DEBUG";
    static inline int get_lltrace(void) {
        char *uop_debug = Py_GETENV(DEBUG_ENV);
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

static int
get_mutations(PyObject* dict) {
    assert(PyDict_CheckExact(dict));
    PyDictObject *d = (PyDictObject *)dict;
    return (d->ma_version_tag >> DICT_MAX_WATCHERS) & ((1 << DICT_WATCHED_MUTATION_BITS)-1);
}

static void
increment_mutations(PyObject* dict) {
    assert(PyDict_CheckExact(dict));
    PyDictObject *d = (PyDictObject *)dict;
    d->ma_version_tag += (1 << DICT_MAX_WATCHERS);
}

/* The first two dict watcher IDs are reserved for CPython,
 * so we don't need to check that they haven't been used */
#define BUILTINS_WATCHER_ID 0
#define GLOBALS_WATCHER_ID  1
#define TYPE_WATCHER_ID  0

static int
globals_watcher_callback(PyDict_WatchEvent event, PyObject* dict,
                         PyObject* key, PyObject* new_value)
{
    RARE_EVENT_STAT_INC(watched_globals_modification);
    assert(get_mutations(dict) < _Py_MAX_ALLOWED_GLOBALS_MODIFICATIONS);
    _Py_Executors_InvalidateDependency(_PyInterpreterState_GET(), dict, 1);
    increment_mutations(dict);
    PyDict_Unwatch(GLOBALS_WATCHER_ID, dict);
    return 0;
}

static int
type_watcher_callback(PyTypeObject* type)
{
    _Py_Executors_InvalidateDependency(_PyInterpreterState_GET(), type, 1);
    PyType_Unwatch(TYPE_WATCHER_ID, (PyObject *)type);
    return 0;
}

static PyObject *
convert_global_to_const(_PyUOpInstruction *inst, PyObject *obj)
{
    assert(inst->opcode == _LOAD_GLOBAL_MODULE || inst->opcode == _LOAD_GLOBAL_BUILTINS || inst->opcode == _LOAD_ATTR_MODULE);
    assert(PyDict_CheckExact(obj));
    PyDictObject *dict = (PyDictObject *)obj;
    assert(dict->ma_keys->dk_kind == DICT_KEYS_UNICODE);
    PyDictUnicodeEntry *entries = DK_UNICODE_ENTRIES(dict->ma_keys);
    assert(inst->operand <= UINT16_MAX);
    if ((int)inst->operand >= dict->ma_keys->dk_nentries) {
        return NULL;
    }
    PyObject *res = entries[inst->operand].me_value;
    if (res == NULL) {
        return NULL;
    }
    if (_Py_IsImmortal(res)) {
        inst->opcode = (inst->oparg & 1) ? _LOAD_CONST_INLINE_BORROW_WITH_NULL : _LOAD_CONST_INLINE_BORROW;
    }
    else {
        inst->opcode = (inst->oparg & 1) ? _LOAD_CONST_INLINE_WITH_NULL : _LOAD_CONST_INLINE;
    }
    inst->operand = (uint64_t)res;
    return res;
}

static int
incorrect_keys(_PyUOpInstruction *inst, PyObject *obj)
{
    if (!PyDict_CheckExact(obj)) {
        return 1;
    }
    PyDictObject *dict = (PyDictObject *)obj;
    if (dict->ma_keys->dk_version != inst->operand) {
        return 1;
    }
    return 0;
}

/* Returns 1 if successfully optimized
 *         0 if the trace is not suitable for optimization (yet)
 *        -1 if there was an error. */
static int
remove_globals(_PyInterpreterFrame *frame, _PyUOpInstruction *buffer,
               int buffer_size, _PyBloomFilter *dependencies)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    PyObject *builtins = frame->f_builtins;
    if (builtins != interp->builtins) {
        OPT_STAT_INC(remove_globals_builtins_changed);
        return 1;
    }
    PyObject *globals = frame->f_globals;
    PyFunctionObject *function = (PyFunctionObject *)frame->f_funcobj;
    assert(PyFunction_Check(function));
    assert(function->func_builtins == builtins);
    assert(function->func_globals == globals);
    uint32_t function_version = _PyFunction_GetVersionForCurrentState(function);
    /* In order to treat globals as constants, we need to
     * know that the globals dict is the one we expected, and
     * that it hasn't changed
     * In order to treat builtins as constants,  we need to
     * know that the builtins dict is the one we expected, and
     * that it hasn't changed and that the global dictionary's
     * keys have not changed */

    /* These values represent stacks of booleans (one bool per bit).
     * Pushing a frame shifts left, popping a frame shifts right. */
    uint32_t function_checked = 0;
    uint32_t builtins_watched = 0;
    uint32_t globals_watched = 0;
    uint32_t prechecked_function_version = 0;
    if (interp->dict_state.watchers[GLOBALS_WATCHER_ID] == NULL) {
        interp->dict_state.watchers[GLOBALS_WATCHER_ID] = globals_watcher_callback;
    }
    if (interp->type_watchers[TYPE_WATCHER_ID] == NULL) {
        interp->type_watchers[TYPE_WATCHER_ID] = type_watcher_callback;
    }
    for (int pc = 0; pc < buffer_size; pc++) {
        _PyUOpInstruction *inst = &buffer[pc];
        int opcode = inst->opcode;
        switch(opcode) {
            case _GUARD_BUILTINS_VERSION:
                if (incorrect_keys(inst, builtins)) {
                    OPT_STAT_INC(remove_globals_incorrect_keys);
                    return 0;
                }
                if (interp->rare_events.builtin_dict >= _Py_MAX_ALLOWED_BUILTINS_MODIFICATIONS) {
                    continue;
                }
                if ((builtins_watched & 1) == 0) {
                    PyDict_Watch(BUILTINS_WATCHER_ID, builtins);
                    builtins_watched |= 1;
                }
                if (function_checked & 1) {
                    buffer[pc].opcode = NOP;
                }
                else {
                    buffer[pc].opcode = _CHECK_FUNCTION;
                    buffer[pc].operand = function_version;
                    function_checked |= 1;
                }
                break;
            case _GUARD_GLOBALS_VERSION:
                if (incorrect_keys(inst, globals)) {
                    OPT_STAT_INC(remove_globals_incorrect_keys);
                    return 0;
                }
                uint64_t watched_mutations = get_mutations(globals);
                if (watched_mutations >= _Py_MAX_ALLOWED_GLOBALS_MODIFICATIONS) {
                    continue;
                }
                if ((globals_watched & 1) == 0) {
                    PyDict_Watch(GLOBALS_WATCHER_ID, globals);
                    _Py_BloomFilter_Add(dependencies, globals);
                    globals_watched |= 1;
                }
                if (function_checked & 1) {
                    buffer[pc].opcode = NOP;
                }
                else {
                    buffer[pc].opcode = _CHECK_FUNCTION;
                    buffer[pc].operand = function_version;
                    function_checked |= 1;
                }
                break;
            case _LOAD_GLOBAL_BUILTINS:
                if (function_checked & globals_watched & builtins_watched & 1) {
                    convert_global_to_const(inst, builtins);
                }
                break;
            case _LOAD_GLOBAL_MODULE:
                if (function_checked & globals_watched & 1) {
                    convert_global_to_const(inst, globals);
                }
                break;
            case _PUSH_FRAME:
            {
                builtins_watched <<= 1;
                globals_watched <<= 1;
                function_checked <<= 1;
                uint64_t operand = buffer[pc].operand;
                if (operand == 0 || (operand & 1)) {
                    // It's either a code object or NULL, so bail
                    return 1;
                }
                PyFunctionObject *func = (PyFunctionObject *)operand;
                if (func == NULL) {
                    return 1;
                }
                assert(PyFunction_Check(func));
                function_version = func->func_version;
                if (prechecked_function_version == function_version) {
                    function_checked |= 1;
                }
                prechecked_function_version = 0;
                globals = func->func_globals;
                builtins = func->func_builtins;
                if (builtins != interp->builtins) {
                    OPT_STAT_INC(remove_globals_builtins_changed);
                    return 1;
                }
                break;
            }
            case _RETURN_VALUE:
            {
                builtins_watched >>= 1;
                globals_watched >>= 1;
                function_checked >>= 1;
                uint64_t operand = buffer[pc].operand;
                if (operand == 0 || (operand & 1)) {
                    // It's either a code object or NULL, so bail
                    return 1;
                }
                PyFunctionObject *func = (PyFunctionObject *)operand;
                if (func == NULL) {
                    return 1;
                }
                assert(PyFunction_Check(func));
                function_version = func->func_version;
                globals = func->func_globals;
                builtins = func->func_builtins;
                break;
            }
            case _CHECK_FUNCTION_EXACT_ARGS:
                prechecked_function_version = (uint32_t)buffer[pc].operand;
                break;
            default:
                if (is_terminator(inst)) {
                    return 1;
                }
                break;
        }
    }
    return 0;
}



#define STACK_LEVEL()     ((int)(stack_pointer - ctx->frame->stack))
#define STACK_SIZE()      ((int)(ctx->frame->stack_len))

#define WITHIN_STACK_BOUNDS() \
    (STACK_LEVEL() >= 0 && STACK_LEVEL() <= STACK_SIZE())


#define GETLOCAL(idx)          ((ctx->frame->locals[idx]))

#define REPLACE_OP(INST, OP, ARG, OPERAND)    \
    INST->opcode = OP;            \
    INST->oparg = ARG;            \
    INST->operand = OPERAND;

/* Shortened forms for convenience, used in optimizer_bytecodes.c */
#define sym_is_not_null _Py_uop_sym_is_not_null
#define sym_is_const _Py_uop_sym_is_const
#define sym_get_const _Py_uop_sym_get_const
#define sym_new_unknown _Py_uop_sym_new_unknown
#define sym_new_not_null _Py_uop_sym_new_not_null
#define sym_new_type _Py_uop_sym_new_type
#define sym_is_null _Py_uop_sym_is_null
#define sym_new_const _Py_uop_sym_new_const
#define sym_new_null _Py_uop_sym_new_null
#define sym_has_type _Py_uop_sym_has_type
#define sym_get_type _Py_uop_sym_get_type
#define sym_matches_type _Py_uop_sym_matches_type
#define sym_matches_type_version _Py_uop_sym_matches_type_version
#define sym_set_null(SYM) _Py_uop_sym_set_null(ctx, SYM)
#define sym_set_non_null(SYM) _Py_uop_sym_set_non_null(ctx, SYM)
#define sym_set_type(SYM, TYPE) _Py_uop_sym_set_type(ctx, SYM, TYPE)
#define sym_set_type_version(SYM, VERSION) _Py_uop_sym_set_type_version(ctx, SYM, VERSION)
#define sym_set_const(SYM, CNST) _Py_uop_sym_set_const(ctx, SYM, CNST)
#define sym_set_locals_idx _Py_uop_sym_set_locals_idx
#define sym_get_locals_idx _Py_uop_sym_get_locals_idx
#define sym_is_bottom _Py_uop_sym_is_bottom
#define sym_truthiness _Py_uop_sym_truthiness
#define frame_new _Py_uop_frame_new
#define frame_pop _Py_uop_frame_pop

static int
optimize_to_bool(
    _PyUOpInstruction *this_instr,
    _Py_UOpsContext *ctx,
    _Py_UopsLocalsPlusSlot value,
    _Py_UopsLocalsPlusSlot *result_ptr)
{
    if (sym_matches_type(value, &PyBool_Type)) {
        REPLACE_OP(this_instr, _NOP, 0, 0);
        *result_ptr = value;
        return 1;
    }
    int truthiness = sym_truthiness(value);
    if (truthiness >= 0) {
        PyObject *load = truthiness ? Py_True : Py_False;
        REPLACE_OP(this_instr, _POP_TOP_LOAD_CONST_INLINE_BORROW, 0, (uintptr_t)load);
        *result_ptr = sym_new_const(ctx, load);
        return 1;
    }
    return 0;
}

static void
eliminate_pop_guard(_PyUOpInstruction *this_instr, bool exit)
{
    REPLACE_OP(this_instr, _POP_TOP, 0, 0);
    if (exit) {
        REPLACE_OP((this_instr+1), _EXIT_TRACE, 0, 0);
        this_instr[1].target = this_instr->target;
    }
}

/* _PUSH_FRAME/_RETURN_VALUE's operand can be 0, a PyFunctionObject *, or a
 * PyCodeObject *. Retrieve the code object if possible.
 */
static PyCodeObject *
get_code(_PyUOpInstruction *op)
{
    assert(op->opcode == _PUSH_FRAME || op->opcode == _RETURN_VALUE || op->opcode == _RETURN_GENERATOR);
    PyCodeObject *co = NULL;
    uint64_t operand = op->operand;
    if (operand == 0) {
        return NULL;
    }
    if (operand & 1) {
        co = (PyCodeObject *)(operand & ~1);
    }
    else {
        PyFunctionObject *func = (PyFunctionObject *)operand;
        assert(PyFunction_Check(func));
        co = (PyCodeObject *)func->func_code;
    }
    assert(PyCode_Check(co));
    return co;
}

static inline _Py_UopsLocalsPlusSlot
sym_to_slot(_Py_UopsSymbol *sym)
{
    return (_Py_UopsLocalsPlusSlot){sym, 0};
}

/* 1 for success, 0 for not ready, cannot error at the moment. */
static int
optimize_uops(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    int trace_len,
    int curr_stacklen,
    _PyBloomFilter *dependencies
)
{

    _Py_UOpsContext context;
    _Py_UOpsContext *ctx = &context;
    uint32_t opcode = UINT16_MAX;
    int curr_space = 0;
    int max_space = 0;
    _PyUOpInstruction *first_valid_check_stack = NULL;
    _PyUOpInstruction *corresponding_check_stack = NULL;

    _Py_uop_abstractcontext_init(ctx);
    _Py_UOpsAbstractFrame *frame = _Py_uop_frame_new(ctx, co, curr_stacklen, NULL, 0, true, false, 0);
    if (frame == NULL) {
        return -1;
    }
    ctx->curr_frame_depth++;
    ctx->frame = frame;
    ctx->done = false;
    ctx->out_of_space = false;
    ctx->contradiction = false;

    _PyUOpInstruction *this_instr = NULL;
    for (int i = 0; !ctx->done; i++) {
        assert(i < trace_len);
        this_instr = &trace[i];

        int oparg = this_instr->oparg;
        opcode = this_instr->opcode;
        _Py_UopsLocalsPlusSlot *stack_pointer = ctx->frame->stack_pointer;

#ifdef Py_DEBUG
        if (get_lltrace() >= 3) {
            printf("%4d abs: ", (int)(this_instr - trace));
            _PyUOpPrint(this_instr);
            printf(" ");
        }
#endif

        switch (opcode) {

#include "optimizer_cases.c.h"

            default:
                DPRINTF(1, "\nUnknown opcode in abstract interpreter\n");
                Py_UNREACHABLE();
        }
        assert(ctx->frame != NULL);
        DPRINTF(3, " stack_level %d\n", STACK_LEVEL());
        ctx->frame->stack_pointer = stack_pointer;
        assert(STACK_LEVEL() >= 0);
    }
    if (ctx->out_of_space) {
        DPRINTF(3, "\n");
        DPRINTF(1, "Out of space in abstract interpreter\n");
    }
    if (ctx->contradiction) {
        // Attempted to push a "bottom" (contradiction) symbol onto the stack.
        // This means that the abstract interpreter has hit unreachable code.
        // We *could* generate an _EXIT_TRACE or _FATAL_ERROR here, but hitting
        // bottom indicates type instability, so we are probably better off
        // retrying later.
        DPRINTF(3, "\n");
        DPRINTF(1, "Hit bottom in abstract interpreter\n");
        _Py_uop_abstractcontext_fini(ctx);
        return 0;
    }

    /* Either reached the end or cannot optimize further, but there
     * would be no benefit in retrying later */
    _Py_uop_abstractcontext_fini(ctx);
    if (first_valid_check_stack != NULL) {
        assert(first_valid_check_stack->opcode == _CHECK_STACK_SPACE);
        assert(max_space > 0);
        assert(max_space <= INT_MAX);
        assert(max_space <= INT32_MAX);
        first_valid_check_stack->opcode = _CHECK_STACK_SPACE_OPERAND;
        first_valid_check_stack->operand = max_space;
    }
    return trace_len;

error:
    DPRINTF(3, "\n");
    DPRINTF(1, "Encountered error in abstract interpreter\n");
    if (opcode <= MAX_UOP_ID) {
        OPT_ERROR_IN_OPCODE(opcode);
    }
    _Py_uop_abstractcontext_fini(ctx);
    return -1;

}

#define WRITE_OP(INST, OP, ARG, OPERAND)    \
    (INST)->opcode = OP;            \
    (INST)->oparg = ARG;            \
    (INST)->operand = OPERAND;

#define SET_STATIC_INST() instr_is_truly_static = true;

#define DONT_EMIT_N_INSTRUCTIONS(n) dont_emit_n_insts = n;


static const uint8_t is_for_iter_test[MAX_UOP_ID + 1] = {
    [_GUARD_NOT_EXHAUSTED_RANGE] = 1,
    [_GUARD_NOT_EXHAUSTED_LIST] = 1,
    [_GUARD_NOT_EXHAUSTED_TUPLE] = 1,
    [_FOR_ITER_TIER_TWO] = 1,
};

#define APPEND_OP(OPCODE, OPARG, OPERAND) \
    WRITE_OP(&trace_dest[ctx->n_trace_dest], OPCODE, OPARG, OPERAND); \
    trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET; \
    trace_dest[ctx->n_trace_dest].target = this_instr->target;  \
    ctx->n_trace_dest++;                        \
    if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH / 2) { \
        ctx->out_of_space = true; \
        ctx->done = true; \
    }

#define APPEND_SIDE_OP(OPCODE, OPARG, OPERAND) \
    WRITE_OP(&trace_dest[ctx->n_side_exit_dest], OPCODE, OPARG, OPERAND); \
    ctx->n_side_exit_dest++;                        \
    if (ctx->n_side_exit_dest >= UOP_MAX_TRACE_LENGTH) { \
        ctx->out_of_space = true; \
        ctx->done = true; \
    }

static void
restore_state_in_side_exit(_Py_UOpsContext *ctx, _Py_UOpsAbstractFrame *frame, _PyUOpInstruction *trace_dest, int original_target)
{
    // Write back the SET_IP
    APPEND_SIDE_OP(_SET_IP, 0, (uintptr_t)frame->instr_ptr);
}

static void
write_side_exit(_Py_UOpsContext *ctx, _Py_UOpsAbstractFrame *frame, _PyUOpInstruction *trace_dest, int opcode, _PyUOpInstruction *this_instr)
{
    assert((_PyUop_Flags[opcode] & (HAS_DEOPT_FLAG | HAS_EXIT_FLAG | HAS_ERROR_FLAG)));
    // Deopts or errors, need to write a side exit to reconstruct state.
    int32_t original_target = (int32_t)uop_get_target(this_instr);
    if (_PyUop_Flags[opcode] & (HAS_EXIT_FLAG | HAS_DEOPT_FLAG)) {
        uint16_t exit_op = (_PyUop_Flags[opcode] & HAS_EXIT_FLAG) ?
                           _EXIT_TRACE : _DEOPT;
        int32_t jump_target = original_target;
        if (is_for_iter_test[opcode]) {
            /* Target the POP_TOP immediately after the END_FOR,
             * leaving only the iterator on the stack. */
            int extended_arg = this_instr->oparg > 255;
            int32_t next_inst = original_target + 1 + INLINE_CACHE_ENTRIES_FOR_ITER + extended_arg;
            jump_target = next_inst + this_instr->oparg + 1;
        }
        int start_of_side_exit = ctx->n_side_exit_dest;
        // Write back the SET_IP
        restore_state_in_side_exit(ctx, frame, trace_dest, original_target);
        APPEND_SIDE_OP(exit_op, 0, 0);
        trace_dest[ctx->n_side_exit_dest - 1].target = jump_target;
        trace_dest[ctx->n_side_exit_dest - 1].format = UOP_FORMAT_TARGET;

        trace_dest[ctx->n_trace_dest - 1].jump_target = start_of_side_exit;
        trace_dest[ctx->n_trace_dest - 1].format = UOP_FORMAT_JUMP;
    }
    if (_PyUop_Flags[opcode] & HAS_ERROR_FLAG) {
        int popped = (_PyUop_Flags[opcode] & HAS_ERROR_NO_POP_FLAG) ?
                     0 : _PyUop_num_popped(opcode, this_instr->oparg);
        int start_of_side_exit = ctx->n_side_exit_dest;
        restore_state_in_side_exit(ctx, frame, trace_dest, original_target);
        APPEND_SIDE_OP(_ERROR_POP_N, popped, 0);
        trace_dest[ctx->n_side_exit_dest - 1].operand = original_target;
        trace_dest[ctx->n_side_exit_dest - 1].format = UOP_FORMAT_TARGET;

        trace_dest[ctx->n_trace_dest - 1].error_target = start_of_side_exit;

        if (trace_dest[ctx->n_trace_dest - 1].format == UOP_FORMAT_TARGET) {
            trace_dest[ctx->n_trace_dest - 1].format = UOP_FORMAT_JUMP;
            trace_dest[ctx->n_trace_dest - 1].jump_target = 0;
        }
    }
}

static bool
write_single_locals_or_const_or_tuple_or_null(
    _Py_UOpsContext *ctx,
    _Py_UOpsAbstractFrame *frame,
    _PyUOpInstruction *trace_dest,
    _Py_UopsLocalsPlusSlot *slot,
    int err_on_false,
    int reconstruct_tuples
    )
{
    if ((*slot).sym->locals_idx >= 0) {
        DPRINTF(3, "reifying LOAD_FAST %d\n", (*slot).sym->locals_idx);
        WRITE_OP(&trace_dest[ctx->n_trace_dest], _LOAD_FAST, (*slot).sym->locals_idx, 0);
        trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
        trace_dest[ctx->n_trace_dest].target = 0;
        ctx->n_trace_dest++;
        return true;
    }
    else if ((*slot).sym->const_val) {
        DPRINTF(3, "reifying LOAD_CONST_INLINE %p\n", (*slot).sym->const_val);
        WRITE_OP(&trace_dest[ctx->n_trace_dest], _Py_IsImmortal((*slot).sym->const_val) ?
            _LOAD_CONST_INLINE_BORROW : _LOAD_CONST_INLINE, 0, (uint64_t) (*slot).sym->const_val);
        trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
        trace_dest[ctx->n_trace_dest].target = 0;
        ctx->n_trace_dest++;
        return true;
    }
    else if (_Py_uop_sym_is_tuple(*slot)) {
        DPRINTF(3, "reifying BUILD_TUPLE start\n");
        if (slot->sym->tuple_size + frame->stack_pointer - 1 > frame->stack + frame->stack_len) {
            ctx->out_of_space = true;
            ctx->done = true;
            return true;
        }
        for (int i = 0; i < slot->sym->tuple_size; i++) {
            _Py_UopsLocalsPlusSlot temp_slot = {
                _Py_uop_sym_tuple_getitem(ctx, *slot, i), false};
            write_single_locals_or_const_or_tuple_or_null(ctx, frame, trace_dest, &temp_slot, true, true);
        }
        if (reconstruct_tuples) {
            // TODO we need a proper target for this eventually for errors.
            // But the chance of a small tuple erroring means OOM and freelist fail. Which is exceedingly rare.
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _BUILD_TUPLE,
                     slot->sym->tuple_size, 0);
            trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_JUMP;
            trace_dest[ctx->n_trace_dest].error_target =
                UOP_MAX_TRACE_LENGTH / 2 - 1;
            trace_dest[ctx->n_trace_dest].jump_target = 0;
            // trace_dest[ctx->n_trace_dest].jump_target = 0;
            ctx->n_trace_dest++;
            DPRINTF(3, "reifying BUILD_TUPLE end\n");
        }
        return true;
    }
    else if (sym_is_null(*slot)) {
        DPRINTF(3, "reifying %d PUSH_NULL\n", (int)(slot - frame->stack));
        WRITE_OP(&trace_dest[ctx->n_trace_dest], _PUSH_NULL, 0, 0);
        trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
        trace_dest[ctx->n_trace_dest].target = 0;
        ctx->n_trace_dest++;
        return true;
    }
    else if (err_on_false) {
        Py_UNREACHABLE();
    }
    return false;
}

static void
reify_single_frame(_Py_UOpsContext *ctx, _Py_UOpsAbstractFrame *frame, int reconstruct_topmost_tuple)
{
    _PyUOpInstruction *trace_dest = ctx->trace_dest;
    int curr_real_stackentries = 0;
    for (_Py_UopsLocalsPlusSlot *sp = frame->stack; sp < frame->stack_pointer; sp++) {
        if (!sp->is_virtual) {
            curr_real_stackentries++;
        }
    }
    int locals_loaded = 0;
    // Restore locals from bottom to top
    for (_Py_UopsLocalsPlusSlot *local = frame->locals; local < frame->locals + frame->locals_len; local++) {
        // Locals did not change. No need to update it.
        if (local->sym->locals_idx == (local - frame->locals)) {
            continue;
        }
        if (sym_is_null(*local)) {
            continue;
        }
        locals_loaded++;
        assert(local->sym->locals_idx != -1 || sym_is_const(*local));
        // Locals DID change.
        DPRINTF(3, "reifying locals LOAD_FAST %d\n", (int)(local->sym->locals_idx));
        if (local->sym->locals_idx != -1) {
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _LOAD_FAST,
                     local->sym->locals_idx, 0);
        }
        else {
            assert(sym_is_const(*local));
            PyObject *const_val = sym_get_const(*local);
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _Py_IsImmortal(const_val) ? _LOAD_CONST_INLINE_BORROW : _LOAD_CONST_INLINE,
                     0, (uintptr_t)const_val);
        }
        trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
        trace_dest[ctx->n_trace_dest].target = 0;
        ctx->n_trace_dest++;
        if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
            ctx->out_of_space = true;
            ctx->done = true;
            return;
        }
    }
    if (locals_loaded + curr_real_stackentries + frame->stack > frame->stack + frame->stack_len) {
        ctx->out_of_space = true;
        ctx->done = true;
        return;
    }
    assert(locals_loaded + curr_real_stackentries +  frame->stack <= frame->stack + frame->stack_len);
    // Store in REVERSE order (due to nature of stack)
    for (_Py_UopsLocalsPlusSlot *local = frame->locals + frame->locals_len - 1; local >= frame->locals ; local--) {
        // Locals did not change. No need to update it.
        if (local->sym->locals_idx == (local - frame->locals)) {
            continue;
        }
        if (sym_is_null(*local)) {
            continue;
        }
        locals_loaded--;
        // Locals DID change.
        DPRINTF(3, "reifying locals STORE_FAST %d\n", (int)(local - frame->locals));
        WRITE_OP(&trace_dest[ctx->n_trace_dest], _STORE_FAST, (local - frame->locals), 0);
        trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
        trace_dest[ctx->n_trace_dest].target = 0;
        ctx->n_trace_dest++;
        if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
            ctx->out_of_space = true;
            ctx->done = true;
            return;
        }
    }
    assert(locals_loaded == 0);
    for (int i = 0; i <  frame->locals_len; i++) {
        if (sym_is_null(frame->locals[i])) {
            continue;
        }
        if (sym_is_const(frame->locals[i])) {
            frame->locals[i] = sym_new_const(ctx, sym_get_const(frame->locals[i]));
        }
        else {
            frame->locals[i] = sym_new_not_null(ctx);
        }
        frame->locals[i].sym->locals_idx = i;
    }
    for (_Py_UopsLocalsPlusSlot *sp = frame->stack; sp < frame->stack_pointer; sp++) {
        _Py_UopsLocalsPlusSlot slot = *sp;
        assert(slot.sym != NULL);
        // Need reifying.
        if (slot.is_virtual) {
            sp->is_virtual = false;
            if (write_single_locals_or_const_or_tuple_or_null(ctx, frame, trace_dest, &slot, false, reconstruct_topmost_tuple)) {
            }
            else {
                // Is static but not a constant value of locals or NULL.
                // How is that possible?
                Py_UNREACHABLE();
            }
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
                return;
            }
        }
    }

    for (int i = 0; i < (int)(frame->stack_pointer - frame->stack); i++) {
        if (sym_is_null(frame->stack[i])) {
            continue;
        }
        frame->stack[i] = sym_new_not_null(ctx);
    }
}


static void
reify_shadow_ctx(_Py_UOpsContext *ctx, int reconstruct_topmost_tuple)
{
    _PyUOpInstruction *trace_dest = ctx->trace_dest;
    (void)reconstruct_topmost_tuple;
    DPRINTF(3, "reifying shadow ctx\n");
    for (int frame_i = 0; frame_i < ctx->curr_frame_depth; frame_i++) {
        _Py_UOpsAbstractFrame *frame = &ctx->frames[frame_i];
        // Reify the frame first.
        if (frame->is_virtual) {
            frame->is_virtual = false;
            // 0-th frame can't be virtual.
            assert(frame_i != 0);
            DPRINTF(3, "reifying frame %d\n", frame_i);
            _Py_UOpsAbstractFrame *prev_frame = frame - 1;
            // Push the stuff we need to init the frame first.
            for (int i = 0; i < frame->host_frame_stackentries; i++) {
                if (frame->args_stack_state[i].is_virtual) {
                    write_single_locals_or_const_or_tuple_or_null(ctx,
                                                                  prev_frame,
                                                                  trace_dest,
                                                                  &frame->args_stack_state[i],
                                                                  true, true);
                }
                frame->args_stack_state[i].is_virtual = false;
            }
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
                return;
            }
            assert((frame->resume_check_inst-7)->opcode == _CHECK_FUNCTION_VERSION_INLINE);
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _CHECK_FUNCTION_VERSION_INLINE, (frame->resume_check_inst-7)->oparg, (frame->resume_check_inst-7)->operand);
            trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
            trace_dest[ctx->n_trace_dest].target = 0;
            ctx->n_trace_dest++;
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
                return;
            }
            write_side_exit(ctx, frame, trace_dest, _CHECK_FUNCTION_VERSION_INLINE, (frame->resume_check_inst-7));
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _INIT_CALL_PY_EXACT_ARGS, frame->init_frame_oparg, 0);
            trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
            trace_dest[ctx->n_trace_dest].target = 0;
            ctx->n_trace_dest++;
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
                return;
            }
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _SAVE_RETURN_OFFSET, prev_frame->return_offset, 0);
            trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
            trace_dest[ctx->n_trace_dest].target = 0;
            ctx->n_trace_dest++;
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
                return;
            }
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _PUSH_FRAME, 0, 0);
            trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
            trace_dest[ctx->n_trace_dest].target = 0;
            ctx->n_trace_dest++;
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
                return;
            }
            WRITE_OP(&trace_dest[ctx->n_trace_dest], _RESUME_CHECK, 0, 0);
            trace_dest[ctx->n_trace_dest].format = UOP_FORMAT_TARGET;
            trace_dest[ctx->n_trace_dest].target = 0;
            ctx->n_trace_dest++;
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
                return;
            }
            write_side_exit(ctx, frame, trace_dest, _RESUME_CHECK, frame->resume_check_inst);
        }
        reify_single_frame(ctx, frame, reconstruct_topmost_tuple);
    }
}


/* 1 for success, 0 for not ready, cannot error at the moment. */
static int
partial_evaluate_uops(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    int trace_len,
    int curr_stacklen,
    _PyBloomFilter *dependencies,
    int *main_trace_len,
    bool *partial_eval_success
)
{

    _PyUOpInstruction trace_dest[UOP_MAX_TRACE_LENGTH];
    for (int i = 0; i < UOP_MAX_TRACE_LENGTH; i++) {
        trace_dest[i].opcode = _NOP;
    }
    _Py_UOpsContext context;
    context.trace_dest = trace_dest;
    context.n_trace_dest = 0;
    context.n_side_exit_dest = UOP_MAX_TRACE_LENGTH / 2;
    _Py_UOpsContext *ctx = &context;
    uint32_t opcode = UINT16_MAX;
    int curr_space = 0;
    int max_space = 0;
    _PyUOpInstruction *first_valid_check_stack = NULL;
    _PyUOpInstruction *corresponding_check_stack = NULL;

    _Py_uop_abstractcontext_init(ctx);
    _Py_UOpsAbstractFrame *frame = _Py_uop_frame_new(ctx, co, curr_stacklen, NULL, 0, false, false, 0);
    if (frame == NULL) {
        return -1;
    }
    ctx->curr_frame_depth++;
    ctx->frame = frame;
    ctx->done = false;
    ctx->out_of_space = false;
    ctx->contradiction = false;

    _PyUOpInstruction *this_instr = NULL;
    int i = 0;
    int dont_emit_n_insts = 0;
    for (; !ctx->done; i++) {
        assert(i < trace_len);
        this_instr = &trace[i];

        int oparg = this_instr->oparg;
        opcode = this_instr->opcode;
        _Py_UopsLocalsPlusSlot *stack_pointer = ctx->frame->stack_pointer;

        // An instruction is candidate static if it has no escapes, and all its inputs
        // are static.
        // If so, whether it can be eliminated is up to whether it has an implementation.
        bool instr_is_truly_static = false;
        if (!(_PyUop_Flags[opcode] & HAS_STATIC_FLAG) &&
            // _INIT_CALL_PY_EXACT_ARGS pushes a non-symbol on the stack, so just don't reify it
            // because the stack is inconsistent.
            (opcode != _SAVE_RETURN_OFFSET && opcode !=_PUSH_FRAME)) {
            reify_shadow_ctx(ctx, true);
        }

#ifdef Py_DEBUG
        if (get_lltrace() >= 3) {
            printf("%4d pe: ", (int)(this_instr - trace));
            _PyUOpPrint(this_instr);
            printf(" ");
        }
#endif

        switch (opcode) {

#include "partial_evaluator_cases.c.h"

            default:
                DPRINTF(1, "\nUnknown opcode in pe's abstract interpreter\n");
                Py_UNREACHABLE();
        }
        assert(ctx->frame != NULL);
        DPRINTF(3, " stack_level %d\n", STACK_LEVEL());
        ctx->frame->stack_pointer = stack_pointer;
        assert(STACK_LEVEL() >= 0);
        if (!instr_is_truly_static && dont_emit_n_insts == 0) {
            trace_dest[ctx->n_trace_dest] = *this_instr;
            ctx->n_trace_dest++;
            if (ctx->n_trace_dest >= UOP_MAX_TRACE_LENGTH) {
                ctx->out_of_space = true;
                ctx->done = true;
            }
            if ((_PyUop_Flags[opcode] & (HAS_DEOPT_FLAG | HAS_EXIT_FLAG | HAS_ERROR_FLAG))) {
                write_side_exit(ctx, ctx->frame, trace_dest, opcode, this_instr);
            }

        }
        else {
            // Inst is static. Nothing written :)!
            assert((_PyUop_Flags[opcode] & HAS_STATIC_FLAG) || dont_emit_n_insts > 0);
            if (dont_emit_n_insts > 0) {
                dont_emit_n_insts--;
            }
#ifdef Py_DEBUG
            if (get_lltrace() >= 3) {
                printf("%4d pe -STATIC-\n", (int) (this_instr - trace));
            }
#endif
        }
        if (ctx->done) {
            break;
        }
    }
    if (ctx->out_of_space) {
        DPRINTF(3, "\n");
        DPRINTF(1, "Out of space in pe's abstract interpreter\n");
    }
    if (ctx->contradiction) {
        // Attempted to push a "bottom" (contradiction) symbol onto the stack.
        // This means that the abstract interpreter has hit unreachable code.
        // We *could* generate an _EXIT_TRACE or _FATAL_ERROR here, but hitting
        // bottom indicates type instability, so we are probably better off
        // retrying later.
        DPRINTF(3, "\n");
        DPRINTF(1, "Hit bottom in pe's abstract interpreter\n");
        _Py_uop_abstractcontext_fini(ctx);
        return trace_len;
    }

    if (ctx->out_of_space || !is_terminator(this_instr)) {
        _Py_uop_abstractcontext_fini(ctx);
        return trace_len;
    }
    else {
        // We MUST not have bailed early here.
        // That's the only time the PE's residual is valid.
        assert(ctx->n_trace_dest < UOP_MAX_TRACE_LENGTH);
        assert(is_terminator(this_instr));

        // Fix up _JUMP_TO_TOP
        for (_PyUOpInstruction *inst = trace_dest; inst < trace_dest + UOP_MAX_TRACE_LENGTH; inst++) {
            if (inst->opcode == _JUMP_TO_TOP) {
                assert(trace_dest[0].opcode == _START_EXECUTOR);
                inst->format = UOP_FORMAT_JUMP;
                inst->jump_target = 1;
            }
        }
        // Copy trace_dest into trace.
        memcpy(trace, trace_dest, UOP_MAX_TRACE_LENGTH * sizeof(_PyUOpInstruction));
        int main_trace_length = ctx->n_trace_dest;
        assert(ctx->n_side_exit_dest >= UOP_MAX_TRACE_LENGTH / 2);
        int trace_dest_len = ctx->n_side_exit_dest;
        _Py_uop_abstractcontext_fini(ctx);
        *main_trace_len = main_trace_length;
        assert(trace_dest_len >= 1);
        *partial_eval_success = true;
        return trace_dest_len;
    }

error:
    DPRINTF(3, "\n");
    DPRINTF(1, "Encountered error in pe's abstract interpreter\n");
    if (opcode <= MAX_UOP_ID) {
        OPT_ERROR_IN_OPCODE(opcode);
    }
    _Py_uop_abstractcontext_fini(ctx);
    return -1;

}

static int
remove_unneeded_uops(_PyUOpInstruction *buffer, int buffer_size)
{
    /* Remove _SET_IP and _CHECK_VALIDITY where possible.
     * _SET_IP is needed if the following instruction escapes or
     * could error. _CHECK_VALIDITY is needed if the previous
     * instruction could have escaped. */
    int last_set_ip = -1;
    bool may_have_escaped = true;
    for (int pc = 0; pc < buffer_size; pc++) {
        int opcode = buffer[pc].opcode;
        switch (opcode) {
            case _START_EXECUTOR:
                may_have_escaped = false;
                break;
            case _SET_IP:
                buffer[pc].opcode = _NOP;
                last_set_ip = pc;
                break;
            case _CHECK_VALIDITY:
                if (may_have_escaped) {
                    may_have_escaped = false;
                }
                else {
                    buffer[pc].opcode = _NOP;
                }
                break;
            case _CHECK_VALIDITY_AND_SET_IP:
                if (may_have_escaped) {
                    may_have_escaped = false;
                    buffer[pc].opcode = _CHECK_VALIDITY;
                }
                else {
                    buffer[pc].opcode = _NOP;
                }
                last_set_ip = pc;
                break;
            case _POP_TOP:
            {
                _PyUOpInstruction *last = &buffer[pc-1];
                while (last->opcode == _NOP) {
                    last--;
                }
                if (last->opcode == _LOAD_CONST_INLINE  ||
                    last->opcode == _LOAD_CONST_INLINE_BORROW ||
                    last->opcode == _LOAD_FAST ||
                    last->opcode == _COPY
                ) {
                    last->opcode = _NOP;
                    buffer[pc].opcode = _NOP;
                }
                if (last->opcode == _REPLACE_WITH_TRUE) {
                    last->opcode = _NOP;
                }
                break;
            }
            case _JUMP_TO_TOP:
            case _EXIT_TRACE:
            case _DYNAMIC_EXIT:
                return pc + 1;
            default:
            {
                /* _PUSH_FRAME doesn't escape or error, but it
                 * does need the IP for the return address */
                bool needs_ip = opcode == _PUSH_FRAME;
                if (_PyUop_Flags[opcode] & HAS_ESCAPES_FLAG) {
                    needs_ip = true;
                    may_have_escaped = true;
                }
                if (needs_ip && last_set_ip >= 0) {
                    if (buffer[last_set_ip].opcode == _CHECK_VALIDITY) {
                        buffer[last_set_ip].opcode = _CHECK_VALIDITY_AND_SET_IP;
                    }
                    else {
                        assert(buffer[last_set_ip].opcode == _NOP);
                        buffer[last_set_ip].opcode = _SET_IP;
                    }
                    last_set_ip = -1;
                }
            }
        }
    }
    Py_UNREACHABLE();
}

int
remove_nop_sled(_PyUOpInstruction *buffer, int whole_trace_len, int main_trace_len) {
    int side_exit_lens = whole_trace_len - UOP_MAX_TRACE_LENGTH / 2;
    int offset = (UOP_MAX_TRACE_LENGTH / 2) - main_trace_len;
    _PyUOpInstruction *side_exit_dest_start = &buffer[main_trace_len];
    _PyUOpInstruction *side_exit_curr_start = &buffer[UOP_MAX_TRACE_LENGTH / 2];
    for (_PyUOpInstruction *start = side_exit_dest_start; start < (side_exit_dest_start + side_exit_lens); start ++) {
        *start = *side_exit_curr_start;
        side_exit_curr_start++;
    }

    // _NOP out the rest
    for (_PyUOpInstruction *start = side_exit_curr_start; start < buffer + UOP_MAX_TRACE_LENGTH; start++) {
        start->opcode = _NOP;
    }

    for (_PyUOpInstruction *inst = buffer; inst < buffer + (main_trace_len); inst++) {
        int opcode = inst->opcode;
        if (inst->format == UOP_FORMAT_JUMP) {
            if (opcode == _JUMP_TO_TOP) {
                break;
            }
            if (is_for_iter_test[opcode]) {
                inst->jump_target -= offset;
                inst->error_target -= offset;
                continue;
            }
            int flags = _PyUop_Flags[opcode];
            if (flags & HAS_ERROR_FLAG) {
                inst->error_target -= offset;
            }
            if (flags & (HAS_DEOPT_FLAG | HAS_EXIT_FLAG)) {
                inst->jump_target -= offset;
            }
        }
    }

    return side_exit_lens + main_trace_len;
}

static void make_exit(_PyUOpInstruction *inst, int opcode, int target)
{
    inst->opcode = opcode;
    inst->oparg = 0;
    inst->operand = 0;
    inst->format = UOP_FORMAT_TARGET;
    inst->target = target;
}

/* Convert implicit exits, errors and deopts
 * into explicit ones. */
static int
prepare_for_execution(_PyUOpInstruction *buffer, int length)
{
    int32_t current_jump = -1;
    int32_t current_jump_target = -1;
    int32_t current_error = -1;
    int32_t current_error_target = -1;
    int32_t current_popped = -1;
    int32_t current_exit_op = -1;
    /* Leaving in NOPs slows down the interpreter and messes up the stats */
    _PyUOpInstruction *copy_to = &buffer[0];
    for (int i = 0; i < length; i++) {
        _PyUOpInstruction *inst = &buffer[i];
        if (inst->opcode != _NOP) {
            if (copy_to != inst) {
                *copy_to = *inst;
            }
            copy_to++;
        }
    }
    length = (int)(copy_to - buffer);
    int next_spare = length;
    for (int i = 0; i < length; i++) {
        _PyUOpInstruction *inst = &buffer[i];
        int opcode = inst->opcode;
        int32_t target = (int32_t)uop_get_target(inst);
        if (_PyUop_Flags[opcode] & (HAS_EXIT_FLAG | HAS_DEOPT_FLAG)) {
            uint16_t exit_op = (_PyUop_Flags[opcode] & HAS_EXIT_FLAG) ?
                               _EXIT_TRACE : _DEOPT;
            int32_t jump_target = target;
            if (is_for_iter_test[opcode]) {
                /* Target the POP_TOP immediately after the END_FOR,
                 * leaving only the iterator on the stack. */
                int extended_arg = inst->oparg > 255;
                int32_t next_inst = target + 1 + INLINE_CACHE_ENTRIES_FOR_ITER + extended_arg;
                jump_target = next_inst + inst->oparg + 1;
            }
            if (jump_target != current_jump_target || current_exit_op != exit_op) {
                make_exit(&buffer[next_spare], exit_op, jump_target);
                current_exit_op = exit_op;
                current_jump_target = jump_target;
                current_jump = next_spare;
                next_spare++;
            }
            buffer[i].jump_target = current_jump;
            buffer[i].format = UOP_FORMAT_JUMP;
        }
        if (_PyUop_Flags[opcode] & HAS_ERROR_FLAG) {
            int popped = (_PyUop_Flags[opcode] & HAS_ERROR_NO_POP_FLAG) ?
                         0 : _PyUop_num_popped(opcode, inst->oparg);
            if (target != current_error_target || popped != current_popped) {
                current_popped = popped;
                current_error = next_spare;
                current_error_target = target;
                make_exit(&buffer[next_spare], _ERROR_POP_N, 0);
                buffer[next_spare].oparg = popped;
                buffer[next_spare].operand = target;
                next_spare++;
            }
            buffer[i].error_target = current_error;
            if (buffer[i].format == UOP_FORMAT_TARGET) {
                buffer[i].format = UOP_FORMAT_JUMP;
                buffer[i].jump_target = 0;
            }
        }
        if (opcode == _JUMP_TO_TOP) {
            assert(buffer[0].opcode == _START_EXECUTOR);
            buffer[i].format = UOP_FORMAT_JUMP;
            buffer[i].jump_target = 1;
        }
    }
    return next_spare;
}

//  0 - failure, no error raised, just fall back to Tier 1
// -1 - failure, and raise error
//  > 0 - length of optimized trace
int
_Py_uop_analyze_and_optimize(
    _PyInterpreterFrame *frame,
    _PyUOpInstruction *buffer,
    int length,
    int curr_stacklen,
    _PyBloomFilter *dependencies
)
{
    OPT_STAT_INC(optimizer_attempts);

    int err = remove_globals(frame, buffer, length, dependencies);
    if (err <= 0) {
        return err;
    }

    length = optimize_uops(
        _PyFrame_GetCode(frame), buffer,
        length, curr_stacklen, dependencies);

    if (length <= 0) {
        return length;
    }

    // Help the PE by removing as many _CHECK_VALIDITY as possible,
    // Since PE treats that as non-static since it can deopt arbitrarily.
    length = remove_unneeded_uops(buffer, length);
    assert(length > 0);

    int main_trace_length;
    bool success = false;
    length = partial_evaluate_uops(
        _PyFrame_GetCode(frame), buffer,
        length, curr_stacklen, dependencies, &main_trace_length, &success);

    if (length <= 0) {
        return length;
    }

    if (success) {
        length = remove_nop_sled(buffer, length, main_trace_length);
    }
    else {
        // Partial eval failed, we have to prepare the exits ourselves.
        length = prepare_for_execution(buffer, length);
    }

    OPT_STAT_INC(optimizer_successes);
    return length;
}


