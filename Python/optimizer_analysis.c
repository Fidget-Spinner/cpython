#ifdef _Py_TIER2

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

static PyFunctionObject *
get_func(_PyUOpInstruction *op)
{
    if (op == NULL) {
        return NULL;
    }
    assert(op->opcode == _PUSH_FRAME || op->opcode == _RETURN_VALUE || op->opcode == _RETURN_GENERATOR
           || op->opcode == _PUSH_SKELETON_FRAME_CANDIDATE);
    uint64_t operand = op->operand;
    if (operand == 0) {
        return NULL;
    }
    if (operand & 1) {
        return NULL;
    }
    else {
        PyFunctionObject *func = (PyFunctionObject *)operand;
        return func;
    }
}

static int
function_decide_inlineable(PyFunctionObject *func)
{
    if (func == NULL) {
        return 0;
    }
    PyCodeObject *co = (PyCodeObject *)func->func_code;
    if (co == NULL) {
        return 0;
    }
    // Ban closures
    if (co->co_ncellvars > 0 || co->co_nfreevars > 0) {
        DPRINTF(2, "inline_fail: closure\n");
        return 0;
    }
    // Ban generators, async, etc.
    int flags = co->co_flags;
    if ((flags & CO_COROUTINE) ||
        (flags & CO_GENERATOR) ||
        (flags & CO_ITERABLE_COROUTINE) ||
        (flags & CO_ASYNC_GENERATOR) ||
        // TODO we can support these in the future.
        (flags & CO_VARKEYWORDS) ||
        (flags & CO_VARARGS)) {
        DPRINTF(2, "inline_fail: generator/coroutine/varargs/varkeywords\n");
        return 0;
    }

    if (co->co_nlocalsplus >= 512) {
        DPRINTF(2, "inline_fail: too many local vars\n");
        return 0;
    }
    return 1;
}

/* Returns 1 if successfully optimized
 *         0 if the trace is not suitable for optimization (yet)
 *        -1 if there was an error. */
static int
remove_globals(_PyInterpreterFrame *frame, _PyUOpInstruction *buffer,
               int buffer_size, _PyBloomFilter *dependencies)
{
    _PyUOpInstruction *push_frames[MAX_ABSTRACT_FRAME_DEPTH];
    int frame_depth = 1;
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
                push_frames[frame_depth] = &buffer[pc];
                frame_depth++;
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
                PyObject *old_globals = globals;
                globals = func->func_globals;
                builtins = func->func_builtins;
                frame_depth--;
                // We need the globals to match, otherwise this will just result in a DEOPT
                // due to the global to constant promotion.
                if (old_globals == globals && function_decide_inlineable(get_func(push_frames[frame_depth]))) {
                    push_frames[frame_depth]->opcode = _PUSH_SKELETON_FRAME_CANDIDATE;
                    // For now, ban non-leaf calls. We need a proper algorithm that works
                    // from each nested call-outwards if we are to support non-leaf calls.
                    for (int i = 0; i < frame_depth - 2; i++) {
                        push_frames[i]->opcode = push_frames[i]->opcode == _PUSH_SKELETON_FRAME_CANDIDATE ? _PUSH_FRAME : push_frames[i]->opcode;
                    }
                }
                else {
                    // Cannot be inlined. Make sure we don't inline previous frames for simpler reconstruction.
                    // In theory, this can be removed in a future version when we get better.
                    for (int i = 0; i < frame_depth; i++) {
                        push_frames[i]->opcode = push_frames[i]->opcode == _PUSH_SKELETON_FRAME_CANDIDATE ? _PUSH_FRAME : push_frames[i]->opcode;
                    }
                }

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
    (INST)->opcode = OP;            \
    (INST)->oparg = ARG;            \
    (INST)->operand = OPERAND;


static inline _Py_UOpsAbstractFrame *
prev_frame(_Py_UOpsContext *ctx)
{
    assert(ctx->curr_frame_depth >= 2);
    return &ctx->frames[ctx->curr_frame_depth - 2];
}

// Note: for this, current frame must be the frame that was just inlined.
// <0 for error
static inline int
add_reconstruction_data(_Py_UOpsContext *ctx, PyCodeObject *f_executable)
{
    // Construct in order:
    // 1. Host frame
    // 2. ... inlined frames (from bottom of stack to top)
    int res = ctx->reconstruction_count;
    if (ctx->reconstruction_count >= TRACE_MAX_FRAME_RECONSTRUCTIONS) {
        return -1;
    }
    // Find host frame
    _Py_UOpsAbstractFrame *curr_frame = NULL;
    int i = ctx->curr_frame_depth - 1;
    bool found = false;
    for (; i >= 0; i--) {
        if (!ctx->frames[i].is_inlined) {;
            found = true;
            break;
        }
    }
    if (!found) {
        DPRINTF(2, "inline_fail: inconsistent frame state in optimizer\n");
        ctx->done = true;
        return -1;
    }
    // Do once for the host frame.
    curr_frame = &ctx->frames[i];
    _PyInterpFrameReconstructor *host_frame = &ctx->reconstruction_buffer[ctx->reconstruction_count];
    _PyInterpFrameReconstructor *cons = host_frame;
    cons->instr_ptr = curr_frame->instr_ptr;
    cons->n_stackentries = (int)(curr_frame->stack_pointer - curr_frame->stack);
    cons->f_executable = (PyObject *)f_executable;
    ctx->reconstruction_count++;
    i++;
    _PyInterpFrameReconstructor *prev = cons;
    // Do for remaining frames.
    for (; i < ctx->curr_frame_depth; i++) {
        curr_frame = &ctx->frames[i];
        cons = &ctx->reconstruction_buffer[ctx->reconstruction_count];
        cons->instr_ptr = curr_frame->instr_ptr;
        cons->return_offset = curr_frame->return_offset;
        cons->n_stackentries = (int)(curr_frame->stack_pointer - curr_frame->stack);
        PyFunctionObject *func = (PyFunctionObject *)get_func(curr_frame->push_frame);
        if (func == NULL) {
            DPRINTF(2, "inline_fail: no func_obj %d, D:%d\n", i, ctx->curr_frame_depth);
            return -1;
        }
        cons->f_funcobj = Py_NewRef(func);
        cons->f_executable = Py_NewRef(func->func_code);

        prev->next_frame_cons = cons;
        prev = cons;
        ctx->reconstruction_count++;
    }

    cons->next_frame_cons = NULL;

    return res;

}

static inline int
inline_call_py_exact_args(_Py_UOpsContext *ctx, _PyUOpInstruction *this_instr, PyCodeObject *f_executable)
{
    // Inline calls
    assert((this_instr - 2)->opcode == _INIT_CALL_PY_EXACT_ARGS);
    assert((this_instr - 1)->opcode == _SAVE_RETURN_OFFSET);
    assert((this_instr + 1)->opcode == _CHECK_VALIDITY_AND_SET_IP ||
           (this_instr + 1)->opcode == _CHECK_VALIDITY);
    assert((this_instr + 2)->opcode == _RESUME_CHECK);
    // Skip over the CHECK_VALIDITY when deciding,
    // as those can be optimized away later.
    PyFunctionObject *func = get_func(this_instr);
    if (func == NULL) {
        DPRINTF(2, "inline_fail: no func\n");
        return -1;
    }
    PyCodeObject *co = (PyCodeObject *)func->func_code;

    int argcount = ctx->frame->argcount;
    if (argcount < 0) {
        DPRINTF(2, "inline_fail: undeterministic argcount\n");
        return -1;
    }

    assert((this_instr + 2)->opcode == _RESUME_CHECK || (this_instr + 2)->opcode == _NOP);
    int first_reconstructor = add_reconstruction_data(ctx, f_executable);
    if (first_reconstructor < 0) {
        DPRINTF(2, "inline_fail: no reconstruction data\n");
        return -1;
    }

    DPRINTF(2, "inline_success\n");

    REPLACE_OP((this_instr - 2), _NOP, 0, 0);
    REPLACE_OP((this_instr - 1), _PUSH_SKELETON_FRAME, co->co_nlocalsplus, argcount);
    REPLACE_OP((this_instr - 0), _SET_RECONSTRUCTION, f_executable->co_framesize, first_reconstructor);
    ctx->frame->first_reconstructor = first_reconstructor;
    // Note: Leave the _CHECK_VALIDITY and +1
    // Remove RESUME_CHECK
    REPLACE_OP((this_instr + 2), _NOP, 0, 0);
}

static inline void
inline_frame_push(_Py_UOpsContext *ctx, _PyUOpInstruction *this_instr, PyCodeObject *f_executable)
{
    ctx->frame->is_inlined = true;
    assert(this_instr->opcode == _PUSH_SKELETON_FRAME_CANDIDATE);
    // So far we only support CALL_PY_EXACT_ARGS form
    // TODO: CALL_PY_GENERAL
    if ((this_instr - 2)->opcode == _INIT_CALL_PY_EXACT_ARGS) {
        if (inline_call_py_exact_args(ctx, this_instr, f_executable) < 0) {
            goto err;
        }
        return;
    }
    DPRINTF(2, "inline_fail: Unknown call form\n");

err:
    this_instr->opcode = _PUSH_FRAME;
    for (int i = 0; i < ctx->curr_frame_depth; i++) {
        ctx->frames[i].is_inlined = false;
    }
    ctx->frame->is_inlined = false;
    return -1;

}

static inline void
inline_frame_pop(_Py_UOpsContext *ctx, _PyUOpInstruction *this_instr, PyCodeObject *inlinee_co, int old_frame_argcount)
{
    assert(this_instr->opcode == _RETURN_VALUE);
    // Inline pop
    assert((this_instr + 1)->opcode == _NOP || (this_instr + 1)->opcode == _CHECK_VALIDITY_AND_SET_IP);
    assert(old_frame_argcount >= 0);

    REPLACE_OP(this_instr, _POP_SKELETON_FRAME, old_frame_argcount, inlinee_co->co_nlocalsplus);
    // REPLACE_OP((this_instr + 1), _SET_RECONSTRUCTION, 0, ctx->frame->first_inlined_frame_offset);
}

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
#define sym_is_bottom _Py_uop_sym_is_bottom
#define sym_truthiness _Py_uop_sym_truthiness
#define frame_new _Py_uop_frame_new
#define frame_pop _Py_uop_frame_pop

static int
optimize_to_bool(
    _PyUOpInstruction *this_instr,
    _Py_UOpsContext *ctx,
    _Py_UopsSymbol *value,
    _Py_UopsSymbol **result_ptr)
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
    assert(op->opcode == _PUSH_FRAME || op->opcode == _RETURN_VALUE || op->opcode == _RETURN_GENERATOR
        || op->opcode == _PUSH_SKELETON_FRAME_CANDIDATE);
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

/* 1 for success, 0 for not ready, cannot error at the moment. */
static int
optimize_uops(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    int trace_len,
    int curr_stacklen,
    _PyBloomFilter *dependencies,
    _PyInterpFrameReconstructor *reconstruction_buffer,
    int *reconstruction_count_p
)
{

    _Py_UOpsContext context;
    _Py_UOpsContext *ctx = &context;
    uint32_t opcode = UINT16_MAX;
    int curr_space = 0;
    int max_space = 0;
    _PyUOpInstruction *first_valid_check_stack = NULL;
    _PyUOpInstruction *corresponding_check_stack = NULL;
    PyCodeObject *f_executable = co;

    _Py_uop_abstractcontext_init(ctx);
    _Py_UOpsAbstractFrame *frame = _Py_uop_frame_new(ctx, co, curr_stacklen, NULL, 0);
    if (frame == NULL) {
        return -1;
    }
    ctx->curr_frame_depth++;
    ctx->frame = frame;
    ctx->done = false;
    ctx->out_of_space = false;
    ctx->contradiction = false;
    ctx->reconstruction_buffer = reconstruction_buffer;

    _PyUOpInstruction *this_instr = NULL;
    for (int i = 0; !ctx->done; i++) {
        assert(i < trace_len);
        this_instr = &trace[i];

        int oparg = this_instr->oparg;
        opcode = this_instr->opcode;
        // TODO implement pseudo or something in optimizer_bytecodes.c
        if (opcode == _PUSH_SKELETON_FRAME_CANDIDATE) {
            opcode = _PUSH_FRAME;
        }
        _Py_UopsSymbol **stack_pointer = ctx->frame->stack_pointer;

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

        if (this_instr->opcode == _PUSH_SKELETON_FRAME_CANDIDATE) {
            inline_frame_push(ctx, this_instr, f_executable);
        }
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
    *reconstruction_count_p = ctx->reconstruction_count;
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
                bool needs_ip = (opcode == _PUSH_FRAME) || (opcode == _PUSH_SKELETON_FRAME);
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

void
cleanup(_PyUOpInstruction *buffer, int buffer_size)
{
    for (int pc = 0; pc < buffer_size; pc++) {
        _PyUOpInstruction *inst = &buffer[pc];
        int opcode = inst->opcode;
        switch(opcode) {
            case _PUSH_SKELETON_FRAME_CANDIDATE:
                inst->opcode = _PUSH_FRAME;
                break;
            default:
                if (is_terminator(inst)) {
                    return;
                }
                break;
        }
    }
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
    _PyBloomFilter *dependencies,
    _PyInterpFrameReconstructor *reconstruction_buffer,
    int *recon_count
)
{
    OPT_STAT_INC(optimizer_attempts);

    int err = remove_globals(frame, buffer, length, dependencies);
    if (err <= 0) {
        return err;
    }

    length = optimize_uops(
        _PyFrame_GetCode(frame), buffer,
        length, curr_stacklen, dependencies, reconstruction_buffer, recon_count);

    if (length <= 0) {
        return length;
    }

    cleanup(buffer, length);

    length = remove_unneeded_uops(buffer, length);
    assert(length > 0);

    OPT_STAT_INC(optimizer_successes);
    return length;
}

#endif /* _Py_TIER2 */
