#include "Python.h"

#ifdef _Py_TIER2

#include "opcode.h"
#include "pycore_interp.h"
#include "pycore_backoff.h"
#include "pycore_bitutils.h"        // _Py_popcount32()
#include "pycore_code.h"            // _Py_GetBaseCodeUnit
#include "pycore_function.h"        // _PyFunction_LookupByVersion()
#include "pycore_interpframe.h"
#include "pycore_object.h"          // _PyObject_GC_UNTRACK()
#include "pycore_opcode_metadata.h" // _PyOpcode_OpName[]
#include "pycore_opcode_utils.h"  // MAX_REAL_OPCODE
#include "pycore_optimizer.h"     // _Py_uop_analyze_and_optimize()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_tuple.h" // _PyTuple_FromArraySteal
#include "pycore_unicodeobject.h" // _PyUnicode_FromASCII
#include "pycore_uop_ids.h"
#include "pycore_jit.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define NEED_OPCODE_METADATA
#include "pycore_uop_metadata.h" // Uop tables
#undef NEED_OPCODE_METADATA

#define MAX_EXECUTORS_SIZE 256

static bool
has_space_for_executor(PyCodeObject *code, _Py_CODEUNIT *instr)
{
    if (instr->op.code == ENTER_EXECUTOR) {
        return true;
    }
    if (code->co_executors == NULL) {
        return true;
    }
    return code->co_executors->size < MAX_EXECUTORS_SIZE;
}

static int32_t
get_index_for_executor(PyCodeObject *code, _Py_CODEUNIT *instr)
{
    if (instr->op.code == ENTER_EXECUTOR) {
        return instr->op.arg;
    }
    _PyExecutorArray *old = code->co_executors;
    int size = 0;
    int capacity = 0;
    if (old != NULL) {
        size = old->size;
        capacity = old->capacity;
        assert(size < MAX_EXECUTORS_SIZE);
    }
    assert(size <= capacity);
    if (size == capacity) {
        /* Array is full. Grow array */
        int new_capacity = capacity ? capacity * 2 : 4;
        _PyExecutorArray *new = PyMem_Realloc(
            old,
            offsetof(_PyExecutorArray, executors) +
            new_capacity * sizeof(_PyExecutorObject *));
        if (new == NULL) {
            return -1;
        }
        new->capacity = new_capacity;
        new->size = size;
        code->co_executors = new;
    }
    assert(size < code->co_executors->capacity);
    return size;
}

static void
insert_executor(PyCodeObject *code, _Py_CODEUNIT *instr, int index, _PyExecutorObject *executor)
{
    Py_INCREF(executor);
    if (instr->op.code == ENTER_EXECUTOR) {
        assert(index == instr->op.arg);
        _Py_ExecutorDetach(code->co_executors->executors[index]);
    }
    else {
        assert(code->co_executors->size == index);
        assert(code->co_executors->capacity > index);
        code->co_executors->size++;
    }
    executor->vm_data.opcode = instr->op.code;
    executor->vm_data.oparg = instr->op.arg;
    executor->vm_data.code = code;
    executor->vm_data.index = (int)(instr - _PyCode_CODE(code));
    code->co_executors->executors[index] = executor;
    assert(index < MAX_EXECUTORS_SIZE);
    instr->op.code = ENTER_EXECUTOR;
    instr->op.arg = index;
}

static _PyExecutorObject *
make_executor_from_uops(_PyUOpInstruction *buffer, int length, const _PyBloomFilter *dependencies);

static int
uop_optimize(_PyInterpreterFrame *frame, _Py_CODEUNIT *instr,
             _PyExecutorObject **exec_ptr, int curr_stackentries,
             bool progress_needed);

/* Returns 1 if optimized, 0 if not optimized, and -1 for an error.
 * If optimized, *executor_ptr contains a new reference to the executor
 */
int
_PyOptimizer_Optimize(
    _PyInterpreterFrame *frame, _Py_CODEUNIT *start,
    _PyExecutorObject **executor_ptr, int chain_depth)
{
    _PyStackRef *stack_pointer = frame->stackpointer;
    if (chain_depth != 0) {
        return 0;
    }
    assert(_PyInterpreterState_GET()->jit);
    // The first executor in a chain and the MAX_CHAIN_DEPTH'th executor *must*
    // make progress in order to avoid infinite loops or excessively-long
    // side-exit chains. We can only insert the executor into the bytecode if
    // this is true, since a deopt won't infinitely re-enter the executor:
    chain_depth %= MAX_CHAIN_DEPTH;
    bool progress_needed = chain_depth == 0;
    PyCodeObject *code = _PyFrame_GetCode(frame);
    assert(PyCode_Check(code));
    if (progress_needed && !has_space_for_executor(code, start)) {
        return 0;
    }
    int err = uop_optimize(frame, start, executor_ptr, (int)(stack_pointer - _PyFrame_Stackbase(frame)), progress_needed);
    if (err <= 0) {
        return err;
    }
    assert(*executor_ptr != NULL);
    if (progress_needed) {
        int index = get_index_for_executor(code, start);
        if (index < 0) {
            /* Out of memory. Don't raise and assume that the
             * error will show up elsewhere.
             *
             * If an optimizer has already produced an executor,
             * it might get confused by the executor disappearing,
             * but there is not much we can do about that here. */
            Py_DECREF(*executor_ptr);
            return 0;
        }
        insert_executor(code, start, index, *executor_ptr);
    }
    else {
        (*executor_ptr)->vm_data.code = NULL;
    }
    (*executor_ptr)->vm_data.chain_depth = chain_depth;
    assert((*executor_ptr)->vm_data.valid);
    return 1;
}

static _PyExecutorObject *
get_executor_lock_held(PyCodeObject *code, int offset)
{
    int code_len = (int)Py_SIZE(code);
    for (int i = 0 ; i < code_len;) {
        if (_PyCode_CODE(code)[i].op.code == ENTER_EXECUTOR && i*2 == offset) {
            int oparg = _PyCode_CODE(code)[i].op.arg;
            _PyExecutorObject *res = code->co_executors->executors[oparg];
            Py_INCREF(res);
            return res;
        }
        i += _PyInstruction_GetLength(code, i);
    }
    PyErr_SetString(PyExc_ValueError, "no executor at given byte offset");
    return NULL;
}

_PyExecutorObject *
_Py_GetExecutor(PyCodeObject *code, int offset)
{
    _PyExecutorObject *executor;
    Py_BEGIN_CRITICAL_SECTION(code);
    executor = get_executor_lock_held(code, offset);
    Py_END_CRITICAL_SECTION();
    return executor;
}

static PyObject *
is_valid(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return PyBool_FromLong(((_PyExecutorObject *)self)->vm_data.valid);
}

static PyObject *
get_opcode(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return PyLong_FromUnsignedLong(((_PyExecutorObject *)self)->vm_data.opcode);
}

static PyObject *
get_oparg(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return PyLong_FromUnsignedLong(((_PyExecutorObject *)self)->vm_data.oparg);
}

///////////////////// Experimental UOp Optimizer /////////////////////

static int executor_clear(_PyExecutorObject *executor);
static void unlink_executor(_PyExecutorObject *executor);

static void
uop_dealloc(_PyExecutorObject *self) {
    _PyObject_GC_UNTRACK(self);
    assert(self->vm_data.code == NULL);
    unlink_executor(self);
#ifdef _Py_JIT
    _PyJIT_Free(self);
#endif
    PyObject_GC_Del(self);
}

const char *
_PyUOpName(int index)
{
    if (index < 0 || index > MAX_UOP_ID) {
        return NULL;
    }
    return _PyOpcode_uop_name[index];
}

#ifdef Py_DEBUG
void
_PyUOpPrint(const _PyUOpInstruction *uop)
{
    const char *name = _PyUOpName(uop->opcode);
    if (name == NULL) {
        printf("<uop %d>", uop->opcode);
    }
    else {
        printf("%s", name);
    }
    switch(uop->format) {
        case UOP_FORMAT_TARGET:
            printf(" (%d, target=%d, operand0=%#" PRIx64 ", operand1=%#" PRIx64,
                uop->oparg,
                uop->target,
                (uint64_t)uop->operand0,
                (uint64_t)uop->operand1);
            break;
        case UOP_FORMAT_JUMP:
            printf(" (%d, jump_target=%d, operand0=%#" PRIx64 ", operand1=%#" PRIx64,
                uop->oparg,
                uop->jump_target,
                (uint64_t)uop->operand0,
                (uint64_t)uop->operand1);
            break;
        default:
            printf(" (%d, Unknown format)", uop->oparg);
    }
    if (_PyUop_Flags[uop->opcode] & HAS_ERROR_FLAG) {
        printf(", error_target=%d", uop->error_target);
    }

    printf(")");
}
#endif

static Py_ssize_t
uop_len(_PyExecutorObject *self)
{
    return self->code_size;
}

static PyObject *
uop_item(_PyExecutorObject *self, Py_ssize_t index)
{
    Py_ssize_t len = uop_len(self);
    if (index < 0 || index >= len) {
        PyErr_SetNone(PyExc_IndexError);
        return NULL;
    }
    const char *name = _PyUOpName(self->trace[index].opcode);
    if (name == NULL) {
        name = "<nil>";
    }
    PyObject *oname = _PyUnicode_FromASCII(name, strlen(name));
    if (oname == NULL) {
        return NULL;
    }
    PyObject *oparg = PyLong_FromUnsignedLong(self->trace[index].oparg);
    if (oparg == NULL) {
        Py_DECREF(oname);
        return NULL;
    }
    PyObject *target = PyLong_FromUnsignedLong(self->trace[index].target);
    if (oparg == NULL) {
        Py_DECREF(oparg);
        Py_DECREF(oname);
        return NULL;
    }
    PyObject *operand = PyLong_FromUnsignedLongLong(self->trace[index].operand0);
    if (operand == NULL) {
        Py_DECREF(target);
        Py_DECREF(oparg);
        Py_DECREF(oname);
        return NULL;
    }
    PyObject *args[4] = { oname, oparg, target, operand };
    return _PyTuple_FromArraySteal(args, 4);
}

PySequenceMethods uop_as_sequence = {
    .sq_length = (lenfunc)uop_len,
    .sq_item = (ssizeargfunc)uop_item,
};

static int
executor_traverse(PyObject *o, visitproc visit, void *arg)
{
    _PyExecutorObject *executor = (_PyExecutorObject *)o;
    for (uint32_t i = 0; i < executor->exit_count; i++) {
        Py_VISIT(executor->exits[i].executor);
    }
    return 0;
}

static PyObject *
get_jit_code(PyObject *self, PyObject *Py_UNUSED(ignored))
{
#ifndef _Py_JIT
    PyErr_SetString(PyExc_RuntimeError, "JIT support not enabled.");
    return NULL;
#else
    _PyExecutorObject *executor = (_PyExecutorObject *)self;
    if (executor->jit_code == NULL || executor->jit_size == 0) {
        Py_RETURN_NONE;
    }
    return PyBytes_FromStringAndSize(executor->jit_code, executor->jit_size);
#endif
}

static PyMethodDef uop_executor_methods[] = {
    { "is_valid", is_valid, METH_NOARGS, NULL },
    { "get_jit_code", get_jit_code, METH_NOARGS, NULL},
    { "get_opcode", get_opcode, METH_NOARGS, NULL },
    { "get_oparg", get_oparg, METH_NOARGS, NULL },
    { NULL, NULL },
};

static int
executor_is_gc(PyObject *o)
{
    return !_Py_IsImmortal(o);
}

PyTypeObject _PyUOpExecutor_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uop_executor",
    .tp_basicsize = offsetof(_PyExecutorObject, exits),
    .tp_itemsize = 1,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC,
    .tp_dealloc = (destructor)uop_dealloc,
    .tp_as_sequence = &uop_as_sequence,
    .tp_methods = uop_executor_methods,
    .tp_traverse = executor_traverse,
    .tp_clear = (inquiry)executor_clear,
    .tp_is_gc = executor_is_gc,
};

/* TO DO -- Generate these tables */
static const uint16_t
_PyUOp_Replacements[MAX_UOP_ID + 1] = {
    [_ITER_JUMP_RANGE] = _TIER2_ITER_JUMP_RANGE,
    [_ITER_JUMP_LIST] = _TIER2_ITER_JUMP_LIST,
    [_ITER_JUMP_TUPLE] = _TIER2_ITER_JUMP_TUPLE,
    [_FOR_ITER] = _FOR_ITER_TIER_TWO,
    [_ITER_NEXT_LIST] = _ITER_NEXT_LIST_TIER_TWO,
    [_POP_JUMP_IF_TRUE] = _TIER2_POP_JUMP_IF_TRUE,
    [_POP_JUMP_IF_FALSE] = _TIER2_POP_JUMP_IF_FALSE,
};

static const uint8_t
is_for_iter_test[MAX_UOP_ID + 1] = {
    [_ITER_CHECK_RANGE] = 1,
    [_ITER_CHECK_LIST] = 1,
    [_ITER_CHECK_TUPLE] = 1,
    [_FOR_ITER_TIER_TWO] = 1,
};


#define CONFIDENCE_RANGE 1000
#define CONFIDENCE_CUTOFF 333

#ifdef Py_DEBUG
#define DPRINTF(level, ...) \
    if (lltrace >= (level)) { printf(__VA_ARGS__); }
#else
#define DPRINTF(level, ...)
#endif


static inline int
add_to_trace(
    _PyUOpInstruction *trace,
    int trace_length,
    uint16_t opcode,
    uint16_t oparg,
    uint64_t operand,
    uint32_t target)
{
    trace[trace_length].opcode = opcode;
    trace[trace_length].format = UOP_FORMAT_TARGET;
    trace[trace_length].target = target;
    trace[trace_length].oparg = oparg;
    trace[trace_length].operand0 = operand;
#ifdef Py_STATS
    trace[trace_length].execution_count = 0;
#endif
    return trace_length + 1;
}

#ifdef Py_DEBUG
#define ADD_TO_TRACE(OPCODE, OPARG, OPERAND, TARGET) \
    assert(trace_length < max_length); \
    trace_length = add_to_trace(trace, trace_length, (OPCODE), (OPARG), (OPERAND), (TARGET)); \
    if (lltrace >= 2) { \
        printf("%4d ADD_TO_TRACE: ", trace_length); \
        _PyUOpPrint(&trace[trace_length-1]); \
        printf("\n"); \
    }
#else
#define ADD_TO_TRACE(OPCODE, OPARG, OPERAND, TARGET) \
    assert(trace_length < max_length); \
    trace_length = add_to_trace(trace, trace_length, (OPCODE), (OPARG), (OPERAND), (TARGET));
#endif

#define INSTR_IP(INSTR, CODE) \
    ((uint32_t)((INSTR) - ((_Py_CODEUNIT *)(CODE)->co_code_adaptive)))

// Reserve space for n uops
#define RESERVE_RAW(n, opname) \
    if (trace_length + (n) > max_length) { \
        DPRINTF(2, "No room for %s (need %d, got %d)\n", \
                (opname), (n), max_length - trace_length); \
        OPT_STAT_INC(trace_too_long); \
        goto done; \
    }

// Reserve space for N uops, plus 3 for _SET_IP, _CHECK_VALIDITY and _EXIT_TRACE
#define RESERVE(needed) RESERVE_RAW((needed) + 3, _PyUOpName(opcode))

static _PyByteCodeTranslationCtx *
translation_ctx_init(
    _PyBloomFilter *dependencies,
    PyCodeObject *co, PyFunctionObject *func, int stackdepth)
{
#ifdef Py_DEBUG
    char *python_lltrace = Py_GETENV("PYTHON_LLTRACE");
    int lltrace = 0;
    if (python_lltrace != NULL && *python_lltrace >= '0') {
        lltrace = *python_lltrace - '0';  // TODO: Parse an int and all that
    }
#endif
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (interp->jit_translation_ctxs_used + 1 > MAX_JIT_TRANSLATION_CTXS) {
        DPRINTF(2, "Too many jit translation contexts.\n");
        return NULL;
    }
    if (co->co_flags & (CO_NESTED)) {
        DPRINTF(2, "Nested code.\n");
        return NULL;
    }
    _PyByteCodeTranslationCtx *ctx = &interp->jit_translation_ctx[interp->jit_translation_ctxs_used];
    ctx->stackdepth = stackdepth;
    _Py_BloomFilter_Add(dependencies, co);
    ctx->co = co;
    ctx->func = func;
    ctx->initial_instr = _PyCode_CODE(ctx->co);
    ctx->last_instr = _PyCode_CODE(ctx->co) + Py_SIZE(ctx->co);
    ctx->seen_entrypoints = 1;

    interp->jit_translation_ctxs_used++;
    ctx->dependencies = dependencies;
    ctx->return_to_this_bb = NULL;
    return ctx;
}

static int translate_bytecode_to_cfg(_PyByteCodeTranslationCtx *ctx);
static int translate_cfg_to_uops(_PyByteCodeTranslationCtx *ctx);
static void dump_bb(_PyByteCodeTranslationCtx *ctx, _PyByteCodeBB *bb);
static void dump_cfg(_PyByteCodeTranslationCtx *ctx);

static void
mark_entrypoint(_PyByteCodeTranslationCtx *ctx, _PyUOpInstruction *start, _Py_CODEUNIT *inst)
{
    if (ctx->stackdepth != 0) {
        return;
    }
    PyInterpreterState *interp = _PyInterpreterState_GET();
    int uop_offset = (int)(start - interp->buffer);
    int bc_offset = (int)(inst - ctx->initial_instr);
    assert(uop_offset >= 0);
    assert(bc_offset >= 0);
    interp->bc_offset_to_uop_offsets[bc_offset] = uop_offset;
}

static int
translate_bb_to_uops(_PyByteCodeTranslationCtx *ctx, _PyByteCodeBB *bb)
{
    _Py_BloomFilter_Add(ctx->dependencies, ctx->co);
    PyInterpreterState *interp = _PyInterpreterState_GET();
    PyCodeObject *code = ctx->co;
    PyFunctionObject *func = ctx->func;
    _PyUOpInstruction *trace = interp->buffer;
    int trace_length = interp->buffer_length;
    int max_length = interp->buffer_max_length;
    _Py_CODEUNIT *instr = bb->slice.start;
    _Py_CODEUNIT *end = bb->slice.end;

    bool first = true;
#ifdef Py_DEBUG
    char *python_lltrace = Py_GETENV("PYTHON_LLTRACE");
    int lltrace = 0;
    if (python_lltrace != NULL && *python_lltrace >= '0') {
        lltrace = *python_lltrace - '0';  // TODO: Parse an int and all that
    }
#endif

    DPRINTF(2,
            "Optimizing %s (%s:%d) at byte offset %d\n",
            PyUnicode_AsUTF8(code->co_qualname),
            PyUnicode_AsUTF8(code->co_filename),
            code->co_firstlineno,
            2 * INSTR_IP(ctx->initial_instr, code));
    uint32_t target = 0;

    if (instr == ctx->initial_instr) {
        mark_entrypoint(ctx, &trace[trace_length], instr);
        ADD_TO_TRACE(_START_EXECUTOR, 0, (uintptr_t) instr, target);
        ADD_TO_TRACE(_MAKE_WARM, 0, 0, 0);
    }

    for (;instr <= end;) {
        if (instr == interp->osr_entry_instr) {
            interp->osr_entry_uop = &trace[trace_length];
        }
        target = INSTR_IP(instr, code);
        // Need space for _DEOPT
        max_length--;

        uint32_t opcode = instr->op.code;
        uint32_t oparg = instr->op.arg;


        DPRINTF(2, "%d: %s(%d)\n", target, _PyOpcode_OpName[opcode], oparg);

        if (opcode == EXTENDED_ARG) {
            mark_entrypoint(ctx, &trace[trace_length], instr);
            instr++;
            opcode = instr->op.code;
            oparg = (oparg << 8) | instr->op.arg;
            if (opcode == EXTENDED_ARG) {
                instr--;
                goto done;
            }
        }
        assert(opcode != EXTENDED_ARG);
        if (interp->buffer != &trace[interp->buffer_length]) {
            if (OPCODE_HAS_NO_SAVE_IP(opcode)) {
                RESERVE_RAW(2, "_CHECK_VALIDITY");
                ADD_TO_TRACE(_CHECK_VALIDITY, 0, 0, target);
            }
            else {
                RESERVE_RAW(2, "_CHECK_VALIDITY_AND_SET_IP");
                ADD_TO_TRACE(_CHECK_VALIDITY_AND_SET_IP, 0, (uintptr_t) instr,
                             target);
            }
        }

        /* Special case the first instruction,
         * so that we can guarantee forward progress */
        if (bb->id == 0 && first) {
            if (OPCODE_HAS_EXIT(opcode) || OPCODE_HAS_DEOPT(opcode)) {
                opcode = _PyOpcode_Deopt[opcode];
            }
            assert(!OPCODE_HAS_EXIT(opcode));
            assert(!OPCODE_HAS_DEOPT(opcode));
        }

        if (OPCODE_HAS_EXIT(opcode)) {
            // Make space for side exit and final _EXIT_TRACE:
            RESERVE_RAW(2, "_EXIT_TRACE");
            max_length--;
        }
        if (OPCODE_HAS_ERROR(opcode)) {
            // Make space for error stub and final _EXIT_TRACE:
            RESERVE_RAW(2, "_ERROR_POP_N");
            max_length--;
        }
        if (opcode == ENTER_EXECUTOR) {
            opcode = ctx->co->co_executors->executors[oparg & 255]->vm_data.opcode;
            oparg = ctx->co->co_executors->executors[oparg & 255]->vm_data.oparg;
        }
        if (opcode == CALL_LIST_APPEND || opcode == BINARY_OP_INPLACE_ADD_UNICODE) {
            return 0;
        }
        switch (opcode) {
            case JUMP_BACKWARD_JIT:
            case JUMP_BACKWARD_NO_INTERRUPT:
            case JUMP_BACKWARD: {
                ADD_TO_TRACE(_CHECK_VALIDITY_AND_SET_IP, 0, (uintptr_t) instr, target);
                mark_entrypoint(ctx, &trace[trace_length], instr);
                ADD_TO_TRACE(_CHECK_PERIODIC, 0, 0, target);
                ADD_TO_TRACE(_TIER2_JUMP, 0, 0, 0);
                if (bb->terminator.kind != BB_JUMP) {
                    dump_bb(ctx, bb);
                    fprintf(stderr, "THING: %s, %d\n", _PyOpcode_OpName[instr->op.code], target);
                }
                assert(bb->terminator.kind == BB_JUMP);
                trace[trace_length-1].branch_or_jump_target_bb = bb->terminator.op.jump.jump_bb;
                break;
                }
            case RESUME:
            case RESUME_CHECK_JIT:
                mark_entrypoint(ctx, &trace[trace_length], instr);
                if (ctx->stackdepth == 0 && bb->id == 0 && instr != ctx->initial_instr) {
                    ADD_TO_TRACE(_START_EXECUTOR, 0, (uintptr_t) instr, target);
                    ADD_TO_TRACE(_MAKE_WARM, 0, 0, 0);
                }
                bb->is_entrypoint = true;
                /* Use a special tier 2 version of RESUME_CHECK to allow traces to
                 *  start with RESUME_CHECK */
                ADD_TO_TRACE(_TIER2_RESUME_CHECK, 0, 0, target);
                break;
            default:
            {
                const struct opcode_macro_expansion *expansion = &_PyOpcode_macro_expansion[opcode];
                if (expansion->nuops > 0) {
                    // Reserve space for nuops (+ _SET_IP + _EXIT_TRACE)
                    int nuops = expansion->nuops;
                    RESERVE(nuops + 1); /* One extra for exit */
                    uint32_t orig_oparg = oparg;  // For OPARG_TOP/BOTTOM
                    for (int i = 0; i < nuops; i++) {
                        oparg = orig_oparg;
                        uint32_t uop = expansion->uops[i].uop;
                        uint64_t operand = 0;
                        // Add one to account for the actual opcode/oparg pair:
                        int offset = expansion->uops[i].offset + 1;
                        switch (expansion->uops[i].size) {
                            case OPARG_SIMPLE:
                                assert(opcode != JUMP_BACKWARD_NO_INTERRUPT && opcode != JUMP_BACKWARD);
                                break;
                            case OPARG_CACHE_1:
                                operand = read_u16(&instr[offset].cache);
                                break;
                            case OPARG_CACHE_2:
                                operand = read_u32(&instr[offset].cache);
                                break;
                            case OPARG_CACHE_4:
                                operand = read_u64(&instr[offset].cache);
                                break;
                            case OPARG_TOP:  // First half of super-instr
                                oparg = orig_oparg >> 4;
                                break;
                            case OPARG_BOTTOM:  // Second half of super-instr
                                oparg = orig_oparg & 0xF;
                                break;
                            case OPARG_SAVE_RETURN_OFFSET:  // op=_SAVE_RETURN_OFFSET; oparg=return_offset
                                oparg = offset;
                                assert(uop == _SAVE_RETURN_OFFSET);
                                break;
                            case OPARG_REPLACED:
                                uop = _PyUOp_Replacements[uop];
                                assert(uop != 0);
#ifdef Py_DEBUG
                                if (_PyOpcode_Deopt[opcode] == FOR_ITER) {
                                    uint32_t next_inst = target + 1 + INLINE_CACHE_ENTRIES_FOR_ITER + (oparg > 255);
                                    uint32_t jump_target = next_inst + oparg;
                                    assert(_Py_GetBaseCodeUnit(code, jump_target).op.code == END_FOR);
                                    assert(_Py_GetBaseCodeUnit(code, jump_target+1).op.code == POP_ITER);
                                }
#endif
                                break;
                            case OPERAND1_1:
                                assert(trace[trace_length-1].opcode == uop);
                                operand = read_u16(&instr[offset].cache);
                                trace[trace_length-1].operand1 = operand;
                                continue;
                            case OPERAND1_2:
                                assert(trace[trace_length-1].opcode == uop);
                                operand = read_u32(&instr[offset].cache);
                                trace[trace_length-1].operand1 = operand;
                                continue;
                            case OPERAND1_4:
                                assert(trace[trace_length-1].opcode == uop);
                                operand = read_u64(&instr[offset].cache);
                                trace[trace_length-1].operand1 = operand;
                                continue;
                            default:
                                fprintf(stderr,
                                        "opcode=%d, oparg=%d; nuops=%d, i=%d; size=%d, offset=%d\n",
                                        opcode, oparg, nuops, i,
                                        expansion->uops[i].size,
                                        expansion->uops[i].offset);
                                Py_FatalError("garbled expansion");
                        }
                        if (uop == _TIER2_POP_JUMP_IF_FALSE ||
                            uop == _TIER2_POP_JUMP_IF_TRUE ||
                            uop == _TIER2_ITER_JUMP_TUPLE ||
                            uop == _TIER2_ITER_JUMP_RANGE ||
                            uop == _TIER2_ITER_JUMP_LIST ||
                            uop == _FOR_ITER_TIER_TWO) {
                            assert(bb->terminator.kind == BB_BRANCH);
                            assert(instr == end);
                            ADD_TO_TRACE(uop, oparg, operand, target);
                            trace[trace_length-1].branch_or_jump_target_bb = bb->terminator.op.branch.alternative_bb;
                            continue;
                        }
                        if (uop == _PUSH_FRAME) {
                            assert(i + 1 == nuops);
                            ADD_TO_TRACE(uop, oparg, operand, target);
                            if (opcode == FOR_ITER_GEN ||
                                opcode == LOAD_ATTR_PROPERTY ||
                                opcode == BINARY_OP_SUBSCR_GETITEM ||
                                opcode == SEND_GEN)
                            {
                                DPRINTF(2, "Bailing due to dynamic target\n");
                                OPT_STAT_INC(unknown_callee);
                                return 0;
                            }
                            // This should be an inlined call.
                            if (bb->terminator.kind == BB_INLINED_CALL) {
                                assert(instr == end);
                                _PyByteCodeTranslationCtx *inlinee = bb->terminator.op.jump.target_ctx;
//                                if (ctx->seen_entrypoints > 1) {
//                                    ADD_TO_TRACE(_TIER2_IP_TO_JUMP_TARGET, 0,
//                                                 (uintptr_t) inlinee->co, 0);
//                                }
                                interp->buffer_max_length = max_length;
                                interp->buffer_length = trace_length;
                                bb->slice.uop_end = &trace[trace_length];
                                if (!translate_cfg_to_uops(inlinee)) {
                                    return 0;
                                }
                                return 1;
                            }
                            else {
                                // BB Jump means recursion.
                                assert(bb->terminator.kind == BB_EXIT || bb->terminator.kind == BB_FALLTHROUGH || bb->terminator.kind == BB_JUMP);
                                return 0;
                            }
                            break;
                        }
                        if (uop == _RETURN_VALUE || uop == _RETURN_GENERATOR || uop == _YIELD_VALUE) {
                            assert(instr == end);
                            if (ctx->stackdepth == 0) {
                                ADD_TO_TRACE(_EXIT_TRACE, oparg, operand,
                                             target);
                            }
                            else {
                                ADD_TO_TRACE(uop, oparg, operand,
                                             target);
                                ADD_TO_TRACE(_TIER2_JUMP, 0, 0, 0);
                                assert(ctx->return_to_this_bb != NULL);
                                trace[trace_length-1].branch_or_jump_target_bb = ctx->return_to_this_bb;
                            }
                            break;
                        }

                        // All other instructions
                        ADD_TO_TRACE(uop, oparg, operand, target);
                    }
                    break;
                }
                DPRINTF(2, "Unsupported opcode %s\n", _PyOpcode_OpName[opcode]);
                OPT_UNSUPPORTED_OPCODE(opcode);
                goto done;
            }  // End default

        }  // End switch (opcode)

        instr++;
        // Add cache size for opcode
        instr += _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];

        first = false;
    }  // End for (;;)
    assert(instr >= end);
    DPRINTF(1,
            "Fully translated BB %d for %s (%s:%d) at byte offset %d -- length %d\n",
            bb->id,
            PyUnicode_AsUTF8(code->co_qualname),
            PyUnicode_AsUTF8(code->co_filename),
            code->co_firstlineno,
            2 * INSTR_IP(bb->slice.start, code),
            trace_length);
    interp->buffer_max_length = max_length;
    interp->buffer_length = trace_length;
    bb->slice.uop_end = &trace[trace_length];
    return 1;
done:
    ADD_TO_TRACE(_EXIT_TRACE, 0, 0, target);
    DPRINTF(1,
            "Partially translated BB %d for %s (%s:%d) at byte offset %d -- length %d\n",
            bb->id,
            PyUnicode_AsUTF8(code->co_qualname),
            PyUnicode_AsUTF8(code->co_filename),
            code->co_firstlineno,
            2 * INSTR_IP(bb->slice.start, code),
            trace_length);
    OPT_HIST(trace_length, trace_length_hist);
    interp->buffer_max_length = max_length;
    interp->buffer_length = trace_length;
    bb->slice.uop_end = &trace[trace_length];
    return 1;
}

static int
translate_cfg_to_uops(_PyByteCodeTranslationCtx *ctx)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    _Py_BloomFilter_Add(ctx->dependencies, ctx->co);
    for (int i = 0; i < ctx->max_seen_bb_count; i++) {
        ctx->bbs[i].slice.uop_start = &interp->buffer[interp->buffer_length];
        if (translate_bb_to_uops(ctx, &ctx->bbs[i]) == 0) {
            return 0;
        }
    }
    // Fix up the real jump targets.
    for (int i = 0; i < interp->buffer_length; i++) {
        switch (interp->buffer[i].opcode) {
            case _TIER2_POP_JUMP_IF_FALSE:
            case _TIER2_POP_JUMP_IF_TRUE:
            case _FOR_ITER_TIER_TWO:
            case _TIER2_ITER_JUMP_LIST:
            case _TIER2_ITER_JUMP_RANGE:
            case _TIER2_ITER_JUMP_TUPLE:
            case _TIER2_JUMP:
//                fprintf(stderr, "OP: %s\n", _PyOpcode_uop_name[interp->buffer[i].opcode]);
                assert(interp->buffer[i].branch_or_jump_target_bb);
                interp->buffer[i].jump_target = (int)(interp->buffer[i].branch_or_jump_target_bb->slice.uop_start - interp->buffer);
                interp->buffer[i].format = UOP_FORMAT_JUMP;
                break;
            default:
                break;
        }
    }
#ifdef Py_DEBUG
    char *python_lltrace = Py_GETENV("PYTHON_LLTRACE");
    int lltrace = 0;
    if (python_lltrace != NULL && *python_lltrace >= '0') {
        lltrace = *python_lltrace - '0';  // TODO: Parse an int and all that
    }
    if (lltrace >= 2) {
        printf("Unoptimized uops (length %d):\n", interp->buffer_length);
        for (int i = 0; i < interp->buffer_length; i++) {
            printf("%4d UNOPTIMIZED: ", i);
            _PyUOpPrint(&interp->buffer[i]);
            printf("\n");
        }
    }
#endif
    return 1;
}

// Credits to Max Bernstein for this
// algorithm/data structure
// https://bernsteinbear.com/blog/discovering-basic-blocks/

#define INSTR_OFFSET(instr) (instr - ctx->initial_instr)

static int
inline_possible_push_frame(_PyByteCodeTranslationCtx *ctx, _Py_CODEUNIT *instr, int opcode, int oparg, int starting_bb_id)
{
#if Py_DEBUG
    char *python_lltrace = Py_GETENV("PYTHON_LLTRACE");
    int lltrace = 0;
    if (python_lltrace != NULL && *python_lltrace >= '0') {
        lltrace = *python_lltrace - '0';  // TODO: Parse an int and all that
    }
#endif
    PyInterpreterState *interp = _PyInterpreterState_GET();
    assert(_PyOpcode_Deopt[opcode] == CALL || _PyOpcode_Deopt[opcode] == CALL_KW);
    const struct opcode_macro_expansion *expansion = &_PyOpcode_macro_expansion[opcode];
    // Reserve space for nuops (+ _SET_IP + _EXIT_TRACE)
    int nuops = expansion->nuops;
    uint32_t orig_oparg = oparg;  // For OPARG_TOP/BOTTOM
    for (int i = 0; i < nuops; i++) {
        uint32_t uop = expansion->uops[i].uop;
        if (uop == _PUSH_FRAME) {
            assert(i + 1 == nuops);
            assert(_PyOpcode_Deopt[opcode] == CALL || _PyOpcode_Deopt[opcode] == CALL_KW);
            int func_version_offset =
                offsetof(_PyCallCache, func_version)/sizeof(_Py_CODEUNIT)
                // Add one to account for the actual opcode/oparg pair:
                + 1;
            uint32_t func_version = read_u32(&instr[func_version_offset].cache);
            PyCodeObject *new_code = NULL;
            PyFunctionObject *new_func =
                _PyFunction_LookupByVersion(func_version, (PyObject **) &new_code);
            if (new_code != NULL) {
                if (new_code == ctx->co) {
                    // Recursive call, bail (we could be here forever).
                    DPRINTF(2, "Bailing on recursive call to %s (%s:%d)\n",
                            PyUnicode_AsUTF8(new_code->co_qualname),
                            PyUnicode_AsUTF8(new_code->co_filename),
                            new_code->co_firstlineno);
                    OPT_STAT_INC(recursive_call);
                    // Point the current BB to the recursive one.
                    ctx->bbs[starting_bb_id].terminator.kind = BB_JUMP;
                    ctx->bbs[starting_bb_id].terminator.op.jump.jump_target = ctx->bbs[0].slice.start;
                    ctx->bbs[starting_bb_id].terminator.op.jump.target_ctx = ctx;
                    return 0;
                }
                if (new_code->co_version != func_version) {
                    ctx->bbs[starting_bb_id].terminator.kind = BB_EXIT;
                    return 0;
                }
                _PyByteCodeTranslationCtx *inlinee_ctx = translation_ctx_init(ctx->dependencies, new_code, new_func, ctx->stackdepth + 1);
                if (inlinee_ctx == NULL) {
                    return 0;
                }
                inlinee_ctx->return_to_this_bb = &ctx->bbs[starting_bb_id + 1];
                DPRINTF(2,
                        "Inlining %s (%s:%d)\n",
                        PyUnicode_AsUTF8(new_code->co_qualname),
                        PyUnicode_AsUTF8(new_code->co_filename),
                        new_code->co_firstlineno);
                // Cannot inline
                if (translate_bytecode_to_cfg(inlinee_ctx) == 0) {
                    DPRINTF(2, "Failed to inline\n");
                    return 0;
                }
                // Insert the new BBs.

                ctx->bbs[starting_bb_id].terminator.kind = BB_INLINED_CALL;
                ctx->bbs[starting_bb_id].terminator.op.jump.target_ctx = inlinee_ctx;
                ctx->bbs[starting_bb_id].terminator.op.jump.jump_bb = &inlinee_ctx->bbs[0];
                ctx->bbs[starting_bb_id].terminator.op.jump.jump_target = _PyCode_CODE(new_code);
                return 1;
            }
            DPRINTF(2, "Bail, new_code == NULL\n");
            OPT_STAT_INC(unknown_callee);
            ctx->bbs[starting_bb_id].terminator.kind = BB_EXIT;
            return 0;
        }
    }
    ctx->bbs[starting_bb_id].terminator.kind = BB_FALLTHROUGH;
    return 1;
}

static int
compare( const void *arg1, const void *arg2 )
{
    return (uintptr_t)arg1 > (uintptr_t)arg2;
}

static void dump_cfg(_PyByteCodeTranslationCtx *ctx);

static void
dump_bb(_PyByteCodeTranslationCtx *ctx, _PyByteCodeBB *bb)
{
    fprintf(stdout, "BB %d {\n", bb->id);
    for (_Py_CODEUNIT *curr = ctx->bbs[bb->id].slice.start; curr <= ctx->bbs[bb->id].slice.end;) {
        int opcode = curr->op.code;
        int oparg = curr->op.arg;
        if (opcode == ENTER_EXECUTOR) {
            opcode = ctx->co->co_executors->executors[oparg & 255]->vm_data.opcode;
            oparg = ctx->co->co_executors->executors[oparg & 255]->vm_data.oparg;
        }
        fprintf(stdout,  "    %d: %s %d\n", (int)(curr - ctx->initial_instr), _PyOpcode_OpName[opcode], oparg);
        curr += _PyOpcode_Caches[_PyOpcode_Deopt[opcode]] + 1;
    }
    switch(ctx->bbs[bb->id].terminator.kind) {
        case BB_BRANCH:
            fprintf(stdout, "Consequent: -> BB %d\n",ctx->bbs[bb->id].terminator.op.branch.consequent_bb->id);
            fprintf(stdout, "Alternative: -> BB %d\n",ctx->bbs[bb->id].terminator.op.branch.alternative_bb->id);
            break;
        case BB_JUMP:
            fprintf(stdout, "Jump: -> BB %d (CTX, %p)\n",ctx->bbs[bb->id].terminator.op.jump.jump_bb->id, ctx->bbs[bb->id].terminator.op.jump.target_ctx);
            break;
        case BB_INLINED_CALL: {
            _PyByteCodeTranslationCtx *target_ctx = ctx->bbs[bb->id].terminator.op.jump.target_ctx;
            fprintf(stdout, "Inlined Call: -> BB %d\n",
                    (int) (ctx->bbs[bb->id].terminator.op.jump.jump_bb - target_ctx->bbs));
            fprintf(stdout, "Start Inlined Call\n");
            dump_cfg(target_ctx);
            fprintf(stdout, "End Inlined Call\n");
            break;
        }
        case BB_FALLTHROUGH:
            fprintf(stdout, "Fallthrough: -> BB %d\n", bb->id + 1);
            break;
        case BB_EXIT:
            fprintf(stdout, "Exit\n");
            break;
    }
    fprintf(stdout, "} \n");
}

static void
dump_cfg(_PyByteCodeTranslationCtx *ctx)
{
    fprintf(stdout, "BBs of %s:%s\n", PyUnicode_AsUTF8(ctx->co->co_filename), PyUnicode_AsUTF8(ctx->co->co_qualname));
    for (int i = 0; i < ctx->max_seen_bb_count; i++) {
        dump_bb(ctx, &ctx->bbs[i]);
    }
}

static int
translate_bytecode_to_cfg(_PyByteCodeTranslationCtx *ctx)
{
#if Py_DEBUG
    char *python_lltrace = Py_GETENV("PYTHON_LLTRACE");
    int lltrace = 0;
    if (python_lltrace != NULL && *python_lltrace >= '0') {
        lltrace = *python_lltrace - '0';  // TODO: Parse an int and all that
    }
#endif
    if (INSTR_OFFSET(ctx->last_instr) > MAX_BYTECODE_SIZE) {
        DPRINTF(2, "Too big code object\n");
        return 0;
    }
    // Step 1. Mark all entrypoints
    for (int i = 0; i < MAX_BYTECODE_SIZE; i++) {
        ctx->instr_is_bb_start[i] = false;
    }
    int opcode;
    int oparg;
    _Py_CODEUNIT *curr = ctx->initial_instr;
    while (curr < ctx->last_instr) {
        assert(curr->op.code != CACHE && curr->op.code != RESERVED);
        oparg = curr->op.arg;
    top:
        opcode = curr->op.code;
        if (opcode == ENTER_EXECUTOR) {
            opcode = ctx->co->co_executors->executors[oparg & 255]->vm_data.opcode;
            assert(opcode == JUMP_BACKWARD_JIT || opcode == RESUME_CHECK_JIT);
            oparg = ctx->co->co_executors->executors[oparg & 255]->vm_data.oparg;
        }
        switch (_PyOpcode_Deopt[opcode]) {
            // Don't support exception code for now.
            case RAISE_VARARGS:
            case CLEANUP_THROW:
            case PUSH_EXC_INFO:
            case CHECK_EXC_MATCH:
            case CHECK_EG_MATCH:
            case LOAD_DEREF:
            case STORE_DEREF:
            case RERAISE:
//            case RERAISE: // TODO move this down later.
                DPRINTF(2, "unsupported opcode %s\n", _PyOpcode_OpName[opcode]);
                return 0;
            case EXTENDED_ARG:
                oparg = oparg << 8 | (curr+1)->op.arg;
                curr++;
                goto top;
            case POP_JUMP_IF_FALSE:
            case POP_JUMP_IF_TRUE:
            case POP_JUMP_IF_NONE:
            case POP_JUMP_IF_NOT_NONE:
            case SEND:
            {
                _Py_CODEUNIT *next_instr = curr + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
                _Py_CODEUNIT *target_instr = next_instr + oparg;
                ctx->instr_is_bb_start[INSTR_OFFSET(next_instr)] = true;
                ctx->instr_is_bb_start[INSTR_OFFSET(target_instr)] = true;
                break;
            }
            case JUMP_BACKWARD:
            case JUMP_BACKWARD_JIT:
            case JUMP_BACKWARD_NO_INTERRUPT:
            {
                _Py_CODEUNIT *next_instr =  curr + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
                ctx->instr_is_bb_start[INSTR_OFFSET(next_instr)] = true;
                _Py_CODEUNIT *target_instr = next_instr - (int)oparg;
                ctx->instr_is_bb_start[INSTR_OFFSET(target_instr)] = true;
                break;
            }
            case JUMP_FORWARD:
            {
                _Py_CODEUNIT *next_instr =  curr + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
                ctx->instr_is_bb_start[INSTR_OFFSET(next_instr)] = true;
                _Py_CODEUNIT *target_instr = next_instr + (int)oparg;
                ctx->instr_is_bb_start[INSTR_OFFSET(target_instr)] = true;
                break;
            }
            case FOR_ITER:
            {
                _Py_CODEUNIT *next_instr = curr + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
                _Py_CODEUNIT *target_instr = next_instr + oparg + 1;
                ctx->instr_is_bb_start[INSTR_OFFSET(next_instr)] = true;
                ctx->instr_is_bb_start[INSTR_OFFSET(target_instr)] = true;
                break;
            }
            case RESUME:
            case RESUME_CHECK_JIT:
                if (curr != ctx->initial_instr) {
                    ctx->seen_entrypoints++;
                }
                ctx->instr_is_bb_start[INSTR_OFFSET(curr)] = true;
                break;
            case RETURN_VALUE:
            case RETURN_GENERATOR:
            case YIELD_VALUE:
            {
                ctx->instr_is_bb_start[INSTR_OFFSET(curr)] = true;
                _Py_CODEUNIT *after = curr+1+_PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
                if (after < ctx->last_instr) {
                    ctx->instr_is_bb_start[INSTR_OFFSET(after)] = true;
                }
                break;
            }
            case END_FOR:
            {
                ctx->instr_is_bb_start[INSTR_OFFSET(curr)] = true;
                break;
            }
            case CALL: // Every Call could be an inline site.
            case CALL_KW:
            {
                ctx->instr_is_bb_start[INSTR_OFFSET(curr)] = true;
                _Py_CODEUNIT *target_instr = curr + 1 + _PyOpcode_Caches[CALL];
                ctx->instr_is_bb_start[INSTR_OFFSET(target_instr)] = true;
                break;
            }
            default:
                assert(opcode < MIN_INSTRUMENTED_OPCODE);
                break;
        }
        // The last instruction is an artificial end.
        if (curr + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]] >= ctx->last_instr) {
            ctx->instr_is_bb_start[INSTR_OFFSET(curr)] = true;
        }
        curr += 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
    }
    // Step 2. Build the CFG
    ctx->max_seen_bb_count = 0;
    for (int i = 0; i < MAX_BYTECODE_SIZE; i++) {
        if (ctx->instr_is_bb_start[i]) {
            ctx->indices[ctx->max_seen_bb_count] = i;
            ctx->max_seen_bb_count++;
            if (ctx->max_seen_bb_count > MAX_BBS_ALLOWED) {
                return 0;
            }
        }
    }
    qsort((void *)ctx->indices, ctx->max_seen_bb_count, sizeof(int), compare);
    for (int i = 0; i < MAX_BYTECODE_SIZE; i++) {
        ctx->instr_to_bb_id[i] = -1;
    }
    // First BB is an entrypoint always.
    ctx->bbs[0].is_entrypoint = true;
    for (int i = 0; i < ctx->max_seen_bb_count; i++) {
        int start_indice = ctx->indices[i];
        int end_indice = start_indice;
        // For the end indice, we need to "rewind" to the last instruction
        // due to the CACHE entries.
        int real_end = i == ctx->max_seen_bb_count - 1 ? (int)INSTR_OFFSET(ctx->last_instr) : ctx->indices[i+1];
        oparg = ctx->initial_instr[end_indice].op.arg;
        while (end_indice < real_end) {
            int end_opcode = ctx->initial_instr[end_indice].op.code;
            if (end_opcode == ENTER_EXECUTOR)  {
                end_opcode = ctx->co->co_executors->executors[oparg & 255]->vm_data.opcode;
            }
            else if (end_opcode == EXTENDED_ARG) {
                while (end_opcode == EXTENDED_ARG) {
                    oparg = oparg << 8 | ctx->initial_instr[end_indice + 1].op.arg;
                    end_indice++;
                    end_opcode = ctx->initial_instr[end_indice].op.code;
                }
            }
            if (end_indice + _PyOpcode_Caches[_PyOpcode_Deopt[end_opcode]] + 1 >= real_end) {
                break;
            }
            end_indice += _PyOpcode_Caches[_PyOpcode_Deopt[end_opcode]] + 1;
            oparg = ctx->initial_instr[end_indice].op.arg;
        }
        ctx->bbs[i].id = i;
        ctx->bbs[i].is_entrypoint = false;
        ctx->bbs[i].slice.start = ctx->initial_instr + start_indice;
        ctx->bbs[i].slice.end = ctx->initial_instr + end_indice;
        ctx->instr_to_bb_id[start_indice] = i;
        int opcode = ctx->initial_instr[end_indice].op.code;
        if (opcode == ENTER_EXECUTOR) {
            opcode = ctx->co->co_executors->executors[oparg & 255]->vm_data.opcode;
            oparg = ctx->co->co_executors->executors[oparg & 255]->vm_data.oparg;
        }
        switch (_PyOpcode_Deopt[opcode]) {
            case POP_JUMP_IF_FALSE:
            case POP_JUMP_IF_TRUE:
            case POP_JUMP_IF_NONE:
            case POP_JUMP_IF_NOT_NONE:
            case SEND:
            {
                int next_instr = end_indice + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
                int target_instr = next_instr + oparg;
                assert(ctx->instr_is_bb_start[next_instr]);
                assert(ctx->instr_is_bb_start[target_instr]);
                ctx->bbs[i].terminator.kind = BB_BRANCH;
                ctx->bbs[i].terminator.op.branch.consequent_target = ctx->initial_instr + next_instr;
                ctx->bbs[i].terminator.op.branch.alternative_target = ctx->initial_instr + target_instr;
                break;
            }
            case JUMP_BACKWARD_NO_INTERRUPT:
            case JUMP_BACKWARD:
            case JUMP_BACKWARD_JIT: {
                int jump_target = end_indice + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]] - (int)oparg;;
                if (!ctx->instr_is_bb_start[jump_target]) {
                    dump_cfg(ctx);
                    fprintf(stderr, "OP: %s:%s\n", _PyOpcode_OpName[(ctx->initial_instr + jump_target)->op.code], _PyOpcode_OpName[ctx->initial_instr[end_indice].op.code]);
                }
                assert(ctx->instr_is_bb_start[jump_target]);
                ctx->bbs[i].terminator.kind = BB_JUMP;
                ctx->bbs[i].terminator.op.jump.jump_target = ctx->initial_instr + jump_target;
                ctx->bbs[i].terminator.op.jump.target_ctx = ctx;
                break;
            }
            case JUMP_FORWARD: {
                int jump_target = end_indice + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]] + (int)oparg;;
                if (!ctx->instr_is_bb_start[jump_target]) {
                    dump_cfg(ctx);
                    fprintf(stderr, "OP: %s:%s\n", _PyOpcode_OpName[(ctx->initial_instr + jump_target)->op.code], _PyOpcode_OpName[ctx->initial_instr[end_indice].op.code]);
                }
                assert(ctx->instr_is_bb_start[jump_target]);
                ctx->bbs[i].terminator.kind = BB_JUMP;
                ctx->bbs[i].terminator.op.jump.jump_target = ctx->initial_instr + jump_target;
                ctx->bbs[i].terminator.op.jump.target_ctx = ctx;
                break;
            }
            case FOR_ITER:
            {
                int next_instr = end_indice + 1 + _PyOpcode_Caches[_PyOpcode_Deopt[opcode]];
                int target_instr = next_instr + oparg + 1;
                if (!ctx->instr_is_bb_start[next_instr]) {
                    dump_cfg(ctx);
                    fprintf(stderr, "OP: %s\n", _PyOpcode_OpName[(ctx->initial_instr + next_instr)->op.code]);
                }
                assert(ctx->instr_is_bb_start[next_instr]);
                assert(ctx->instr_is_bb_start[target_instr]);

                ctx->bbs[i].terminator.kind = BB_BRANCH;
                ctx->bbs[i].terminator.op.branch.consequent_target = ctx->initial_instr + next_instr;
                ctx->bbs[i].terminator.op.branch.alternative_target = ctx->initial_instr + target_instr;
                break;
            }
            case CALL:
            case CALL_KW:
                if (!inline_possible_push_frame(ctx, &ctx->initial_instr[end_indice],
                                           opcode, oparg, i)) {
                    return 0;
                }
                break;
            case RETURN_VALUE:
            case RETURN_GENERATOR:
            case YIELD_VALUE:
            case RERAISE:
                ctx->bbs[i].terminator.kind = BB_EXIT;
                break;
            case ENTER_EXECUTOR:
                Py_UNREACHABLE();
            default:
                ctx->bbs[i].terminator.kind = BB_FALLTHROUGH;
                break;
        }
    }

    // Step 3. Link up the BBs
    for (int i = 0; i < ctx->max_seen_bb_count; i++) {
        switch(ctx->bbs[i].terminator.kind) {
            case BB_BRANCH: {
                int alt_bb_id = ctx->instr_to_bb_id[ctx->bbs[i].terminator.op.branch.alternative_target - ctx->initial_instr];
                assert(alt_bb_id >= 0);
                ctx->bbs[i].terminator.op.branch.alternative_bb = &ctx->bbs[alt_bb_id];
                int cons_bb_id = ctx->instr_to_bb_id[ctx->bbs[i].terminator.op.branch.consequent_target - ctx->initial_instr];
                assert(cons_bb_id >= 0);
                ctx->bbs[i].terminator.op.branch.consequent_bb = &ctx->bbs[cons_bb_id];
                break;
            }
            case BB_JUMP:
            {
                _PyByteCodeTranslationCtx *target_ctx = ctx->bbs[i].terminator.op.jump.target_ctx;
                int jump_bb_id = target_ctx->instr_to_bb_id[ctx->bbs[i].terminator.op.jump.jump_target - ctx->initial_instr];
                assert(jump_bb_id >= 0);
                ctx->bbs[i].terminator.op.jump.jump_bb = &target_ctx->bbs[jump_bb_id];
                break;
            }
            case BB_FALLTHROUGH:
            {
                assert(i < ctx->max_seen_bb_count);
                ctx->bbs[i].terminator.op.jump.jump_bb = &ctx->bbs[i + 1];
                break;
            }
            case BB_INLINED_CALL:
            case BB_EXIT:
                break;
        }
    }

//     dump_cfg(ctx);
    return 1;
}

#undef RESERVE
#undef RESERVE_RAW
#undef INSTR_IP
#undef ADD_TO_TRACE
#undef DPRINTF

#define UNSET_BIT(array, bit) (array[(bit)>>5] &= ~(1<<((bit)&31)))
#define SET_BIT(array, bit) (array[(bit)>>5] |= (1<<((bit)&31)))
#define BIT_IS_SET(array, bit) (array[(bit)>>5] & (1<<((bit)&31)))

/* Count the number of unused uops and exits
*/
static int
count_exits(_PyUOpInstruction *buffer, int length)
{
    int exit_count = 0;
    for (int i = 0; i < length; i++) {
        int opcode = buffer[i].opcode;
        if (opcode == _EXIT_TRACE) {
            exit_count++;
        }
    }
    return exit_count;
}

static void make_exit(_PyUOpInstruction *inst, int opcode, int target)
{
    inst->opcode = opcode;
    inst->oparg = 0;
    inst->operand0 = 0;
    inst->format = UOP_FORMAT_TARGET;
    inst->target = target;
#ifdef Py_STATS
    inst->execution_count = 0;
#endif
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

    int next_spare = length;
    for (int i = 0; i < length; i++) {
        _PyUOpInstruction *inst = &buffer[i];
        if (inst->format == UOP_FORMAT_JUMP) {
            continue;
        }
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
                buffer[next_spare].operand0 = target;
                next_spare++;
            }
            buffer[i].error_target = current_error;
            if (buffer[i].format == UOP_FORMAT_TARGET) {
                buffer[i].format = UOP_FORMAT_JUMP;
                buffer[i].jump_target = 0;
            }
        }
        assert(opcode != _JUMP_TO_TOP);
    }
    return next_spare;
}

/* Executor side exits */

static _PyExecutorObject *
allocate_executor(int exit_count, int length)
{
    int size = exit_count*sizeof(_PyExitData) + length*sizeof(_PyUOpInstruction);
    _PyExecutorObject *res = PyObject_GC_NewVar(_PyExecutorObject, &_PyUOpExecutor_Type, size);
    if (res == NULL) {
        return NULL;
    }
    res->trace = (_PyUOpInstruction *)(res->exits + exit_count);
    res->code_size = length;
    res->exit_count = exit_count;
    return res;
}

#ifdef Py_DEBUG

#define CHECK(PRED) \
if (!(PRED)) { \
    printf(#PRED " at %d\n", i); \
    assert(0); \
}

static int
target_unused(int opcode)
{
    return (_PyUop_Flags[opcode] & (HAS_ERROR_FLAG | HAS_EXIT_FLAG | HAS_DEOPT_FLAG)) == 0;
}

static void
sanity_check(_PyExecutorObject *executor)
{
    for (uint32_t i = 0; i < executor->exit_count; i++) {
        _PyExitData *exit = &executor->exits[i];
        CHECK(exit->target < (1 << 25));
    }
    bool ended = false;
    uint32_t i = 0;
    CHECK(executor->trace[0].opcode == _START_EXECUTOR);
    for (; i < executor->code_size; i++) {
        const _PyUOpInstruction *inst = &executor->trace[i];
        uint16_t opcode = inst->opcode;
        CHECK(opcode <= MAX_UOP_ID);
        CHECK(_PyOpcode_uop_name[opcode] != NULL);
        switch(inst->format) {
            case UOP_FORMAT_TARGET:
                CHECK(target_unused(opcode));
                break;
            case UOP_FORMAT_JUMP:
                CHECK(inst->jump_target < executor->code_size);
                break;
        }
        if (_PyUop_Flags[opcode] & HAS_ERROR_FLAG) {
            CHECK(inst->format == UOP_FORMAT_JUMP);
            CHECK(inst->error_target < executor->code_size);
        }
        if (is_terminator(inst)) {
            ended = true;
            i++;
        }
    }
    CHECK(ended);
//    for (; i < executor->code_size; i++) {
//        const _PyUOpInstruction *inst = &executor->trace[i];
//        uint16_t opcode = inst->opcode;
//        CHECK(
//            opcode == _DEOPT ||
//            opcode == _EXIT_TRACE ||
//            opcode == _ERROR_POP_N);
//    }
}

#undef CHECK
#endif

/* Makes an executor from a buffer of uops.
 * Account for the buffer having gaps and NOPs by computing a "used"
 * bit vector and only copying the used uops. Here "used" means reachable
 * and not a NOP.
 */
static _PyExecutorObject *
make_executor_from_uops(_PyUOpInstruction *buffer, int length, const _PyBloomFilter *dependencies)
{
    int exit_count = count_exits(buffer, length);
    _PyExecutorObject *executor = allocate_executor(exit_count, length);
    if (executor == NULL) {
        return NULL;
    }

    /* Initialize exits */
    for (int i = 0; i < exit_count; i++) {
        executor->exits[i].executor = NULL;
        executor->exits[i].temperature = initial_temperature_backoff_counter();
    }
    int next_exit = exit_count-1;
    _PyUOpInstruction *dest = (_PyUOpInstruction *)&executor->trace[length];
    assert(buffer[0].opcode == _START_EXECUTOR);
    buffer[0].operand0 = (uint64_t)executor;
    for (int i = 0; i < length; i++) {
        if (buffer[i].opcode == _START_EXECUTOR) {
            buffer[i].operand0 = (uint64_t)executor;
        }
    }
    for (int i = length-1; i >= 0; i--) {
        int opcode = buffer[i].opcode;
        dest--;
        *dest = buffer[i];
        assert(opcode != _POP_JUMP_IF_FALSE && opcode != _POP_JUMP_IF_TRUE);
        if (opcode == _EXIT_TRACE) {
            _PyExitData *exit = &executor->exits[next_exit];
            exit->target = buffer[i].target;
            dest->operand0 = (uint64_t)exit;
            next_exit--;
        }
    }
    assert(next_exit == -1);
    assert(dest == executor->trace);
    assert(dest->opcode == _START_EXECUTOR);
    _Py_ExecutorInit(executor, dependencies);
#ifdef Py_DEBUG
    char *python_lltrace = Py_GETENV("PYTHON_LLTRACE");
    int lltrace = 0;
    if (python_lltrace != NULL && *python_lltrace >= '0') {
        lltrace = *python_lltrace - '0';  // TODO: Parse an int and all that
    }
    if (lltrace >= 2) {
        printf("Optimized trace (length %d):\n", length);
        for (int i = 0; i < length; i++) {
            printf("%4d OPTIMIZED: ", i);
            _PyUOpPrint(&executor->trace[i]);
            printf("\n");
        }
    }
    sanity_check(executor);
#endif
#ifdef _Py_JIT
    executor->jit_code = NULL;
    executor->jit_side_entry = NULL;
    executor->jit_size = 0;
    // This is initialized to true so we can prevent the executor
    // from being immediately detected as cold and invalidated.
    executor->vm_data.warm = true;
    if (_PyJIT_Compile(executor, executor->trace, length)) {
        Py_DECREF(executor);
        return NULL;
    }
#endif
    _PyObject_GC_TRACK(executor);
    return executor;
}

#ifdef Py_STATS
/* Returns the effective trace length.
 * Ignores NOPs and trailing exit and error handling.*/
int effective_trace_length(_PyUOpInstruction *buffer, int length)
{
    int nop_count = 0;
    for (int i = 0; i < length; i++) {
        int opcode = buffer[i].opcode;
        if (opcode == _NOP) {
            nop_count++;
        }
        if (is_terminator(&buffer[i])) {
            return i+1-nop_count;
        }
    }
    Py_FatalError("No terminating instruction");
    Py_UNREACHABLE();
}
#endif

static int
uop_optimize(
    _PyInterpreterFrame *frame,
    _Py_CODEUNIT *instr,
    _PyExecutorObject **exec_ptr,
    int curr_stackentries,
    bool progress_needed)
{
    _PyBloomFilter dependencies;
    _Py_BloomFilter_Init(&dependencies);
    OPT_STAT_INC(attempts);
    PyInterpreterState *interp = _PyInterpreterState_GET();
    interp->jit_translation_ctxs_used = 0;
    interp->osr_entry_instr = instr;
    interp->osr_entry_uop = NULL;
//    char *second = Py_GETENV("PYTHON_UOPS_CFG");
//    if (second == NULL) {
//        return 0;
//    }
    _PyByteCodeTranslationCtx *ctx = translation_ctx_init(&dependencies,
                                                          _PyFrame_GetCode(frame),
                                                          _PyFrame_GetFunction(frame), 0);
    if (ctx == NULL) {
        return 0;
    }
    if (!translate_bytecode_to_cfg(ctx)) {
        return 0;
    }
    interp->buffer_length = 0;
    interp->buffer_max_length = UOP_MAX_METHOD_LENGTH - 2;
    for (int i = 0; i < MAX_BYTECODE_SIZE; i++) {
        interp->bc_offset_to_uop_offsets[i] = -1;
    }
    if (!translate_cfg_to_uops(ctx)) {
        return 0;
    }
    assert(interp->osr_entry_uop != NULL);
    int length = interp->buffer_length;

    assert(length < UOP_MAX_METHOD_LENGTH);

    OPT_STAT_INC(traces_created);
    char *env_var = Py_GETENV("PYTHON_UOPS_OPTIMIZE");
    if (env_var == NULL || *env_var == '\0' || *env_var > '0') {
        length = _Py_uop_analyze_and_optimize(ctx, interp->buffer,
                                           length,
                                           curr_stackentries, &dependencies);
        if (length <= 0) {
            return length;
        }
    }
    assert(length < UOP_MAX_METHOD_LENGTH);
    assert(length >= 1);
    /* Fix up */
    for (int pc = 0; pc < interp->buffer_length; pc++) {
        int opcode = interp->buffer[pc].opcode;
        int oparg = interp->buffer[pc].oparg;
        if (oparg < _PyUop_Replication[opcode]) {
            interp->buffer[pc].opcode = opcode + oparg + 1;
            assert(strncmp(_PyOpcode_uop_name[interp->buffer[pc].opcode], _PyOpcode_uop_name[opcode], strlen(_PyOpcode_uop_name[opcode])) == 0);
        }
        assert(_PyOpcode_uop_name[interp->buffer[pc].opcode]);
    }
    OPT_HIST(effective_trace_length(buffer, length), optimized_trace_length_hist);
    length = prepare_for_execution(interp->buffer, length);
    assert(length <= UOP_MAX_METHOD_LENGTH);
    _PyExecutorObject *executor = make_executor_from_uops(interp->buffer, length,  &dependencies);
    if (executor == NULL) {
        return -1;
    }
    assert(length <= UOP_MAX_METHOD_LENGTH);
    *exec_ptr = executor;
    executor->osr_entry_offset = (int)(interp->osr_entry_uop - interp->buffer);
    assert(executor->osr_entry_offset <= interp->buffer_length);

    for (int i = 0; i < MAX_BYTECODE_SIZE; i++) {
        executor->bc_offset_to_trace_offset[i] = interp->bc_offset_to_uop_offsets[i];
    }

    return 1;
}


/*****************************************
 *        Executor management
 ****************************************/

/* We use a bloomfilter with k = 6, m = 256
 * The choice of k and the following constants
 * could do with a more rigorous analysis,
 * but here is a simple analysis:
 *
 * We want to keep the false positive rate low.
 * For n = 5 (a trace depends on 5 objects),
 * we expect 30 bits set, giving a false positive
 * rate of (30/256)**6 == 2.5e-6 which is plenty
 * good enough.
 *
 * However with n = 10 we expect 60 bits set (worst case),
 * giving a false positive of (60/256)**6 == 0.0001
 *
 * We choose k = 6, rather than a higher number as
 * it means the false positive rate grows slower for high n.
 *
 * n = 5, k = 6 => fp = 2.6e-6
 * n = 5, k = 8 => fp = 3.5e-7
 * n = 10, k = 6 => fp = 1.6e-4
 * n = 10, k = 8 => fp = 0.9e-4
 * n = 15, k = 6 => fp = 0.18%
 * n = 15, k = 8 => fp = 0.23%
 * n = 20, k = 6 => fp = 1.1%
 * n = 20, k = 8 => fp = 2.3%
 *
 * The above analysis assumes perfect hash functions,
 * but those don't exist, so the real false positive
 * rates may be worse.
 */

#define K 6

#define SEED 20221211

/* TO DO -- Use more modern hash functions with better distribution of bits */
static uint64_t
address_to_hash(void *ptr) {
    assert(ptr != NULL);
    uint64_t uhash = SEED;
    uintptr_t addr = (uintptr_t)ptr;
    for (int i = 0; i < SIZEOF_VOID_P; i++) {
        uhash ^= addr & 255;
        uhash *= (uint64_t)PyHASH_MULTIPLIER;
        addr >>= 8;
    }
    return uhash;
}

void
_Py_BloomFilter_Init(_PyBloomFilter *bloom)
{
    for (int i = 0; i < _Py_BLOOM_FILTER_WORDS; i++) {
        bloom->bits[i] = 0;
    }
}

/* We want K hash functions that each set 1 bit.
 * A hash function that sets 1 bit in M bits can be trivially
 * derived from a log2(M) bit hash function.
 * So we extract 8 (log2(256)) bits at a time from
 * the 64bit hash. */
void
_Py_BloomFilter_Add(_PyBloomFilter *bloom, void *ptr)
{
    uint64_t hash = address_to_hash(ptr);
    assert(K <= 8);
    for (int i = 0; i < K; i++) {
        uint8_t bits = hash & 255;
        bloom->bits[bits >> 5] |= (1 << (bits&31));
        hash >>= 8;
    }
}

static bool
bloom_filter_may_contain(_PyBloomFilter *bloom, _PyBloomFilter *hashes)
{
    for (int i = 0; i < _Py_BLOOM_FILTER_WORDS; i++) {
        if ((bloom->bits[i] & hashes->bits[i]) != hashes->bits[i]) {
            return false;
        }
    }
    return true;
}

static void
link_executor(_PyExecutorObject *executor)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    _PyExecutorLinkListNode *links = &executor->vm_data.links;
    _PyExecutorObject *head = interp->executor_list_head;
    if (head == NULL) {
        interp->executor_list_head = executor;
        links->previous = NULL;
        links->next = NULL;
    }
    else {
        assert(head->vm_data.links.previous == NULL);
        links->previous = NULL;
        links->next = head;
        head->vm_data.links.previous = executor;
        interp->executor_list_head = executor;
    }
    executor->vm_data.linked = true;
    /* executor_list_head must be first in list */
    assert(interp->executor_list_head->vm_data.links.previous == NULL);
}

static void
unlink_executor(_PyExecutorObject *executor)
{
    if (!executor->vm_data.linked) {
        return;
    }
    _PyExecutorLinkListNode *links = &executor->vm_data.links;
    assert(executor->vm_data.valid);
    _PyExecutorObject *next = links->next;
    _PyExecutorObject *prev = links->previous;
    if (next != NULL) {
        next->vm_data.links.previous = prev;
    }
    if (prev != NULL) {
        prev->vm_data.links.next = next;
    }
    else {
        // prev == NULL implies that executor is the list head
        PyInterpreterState *interp = PyInterpreterState_Get();
        assert(interp->executor_list_head == executor);
        interp->executor_list_head = next;
    }
    executor->vm_data.linked = false;
}

/* This must be called by optimizers before using the executor */
void
_Py_ExecutorInit(_PyExecutorObject *executor, const _PyBloomFilter *dependency_set)
{
    executor->vm_data.valid = true;
    for (int i = 0; i < _Py_BLOOM_FILTER_WORDS; i++) {
        executor->vm_data.bloom.bits[i] = dependency_set->bits[i];
    }
    link_executor(executor);
}

/* Detaches the executor from the code object (if any) that
 * holds a reference to it */
void
_Py_ExecutorDetach(_PyExecutorObject *executor)
{
    PyCodeObject *code = executor->vm_data.code;
    if (code == NULL) {
        return;
    }
    _Py_CODEUNIT *instruction = &_PyCode_CODE(code)[executor->vm_data.index];
    assert(instruction->op.code == ENTER_EXECUTOR);
    int index = instruction->op.arg;
    assert(code->co_executors->executors[index] == executor);
    instruction->op.code = executor->vm_data.opcode;
    instruction->op.arg = executor->vm_data.oparg;
    executor->vm_data.code = NULL;
    code->co_executors->executors[index] = NULL;
    Py_DECREF(executor);
}

static int
executor_clear(_PyExecutorObject *executor)
{
    if (!executor->vm_data.valid) {
        return 0;
    }
    assert(executor->vm_data.valid == 1);
    unlink_executor(executor);
    executor->vm_data.valid = 0;
    /* It is possible for an executor to form a reference
     * cycle with itself, so decref'ing a side exit could
     * free the executor unless we hold a strong reference to it
     */
    Py_INCREF(executor);
    for (uint32_t i = 0; i < executor->exit_count; i++) {
        executor->exits[i].temperature = initial_unreachable_backoff_counter();
        Py_CLEAR(executor->exits[i].executor);
    }
    _Py_ExecutorDetach(executor);
    Py_DECREF(executor);
    return 0;
}

void
_Py_Executor_DependsOn(_PyExecutorObject *executor, void *obj)
{
    assert(executor->vm_data.valid);
    _Py_BloomFilter_Add(&executor->vm_data.bloom, obj);
}

/* Invalidate all executors that depend on `obj`
 * May cause other executors to be invalidated as well
 */
void
_Py_Executors_InvalidateDependency(PyInterpreterState *interp, void *obj, int is_invalidation)
{
    _PyBloomFilter obj_filter;
    _Py_BloomFilter_Init(&obj_filter);
    _Py_BloomFilter_Add(&obj_filter, obj);
    /* Walk the list of executors */
    /* TO DO -- Use a tree to avoid traversing as many objects */
    PyObject *invalidate = PyList_New(0);
    if (invalidate == NULL) {
        goto error;
    }
    /* Clearing an executor can deallocate others, so we need to make a list of
     * executors to invalidate first */
    for (_PyExecutorObject *exec = interp->executor_list_head; exec != NULL;) {
        assert(exec->vm_data.valid);
        _PyExecutorObject *next = exec->vm_data.links.next;
        if (bloom_filter_may_contain(&exec->vm_data.bloom, &obj_filter) &&
            PyList_Append(invalidate, (PyObject *)exec))
        {
            goto error;
        }
        exec = next;
    }
    for (Py_ssize_t i = 0; i < PyList_GET_SIZE(invalidate); i++) {
        _PyExecutorObject *exec = (_PyExecutorObject *)PyList_GET_ITEM(invalidate, i);
        executor_clear(exec);
        if (is_invalidation) {
            OPT_STAT_INC(executors_invalidated);
        }
    }
    Py_DECREF(invalidate);
    return;
error:
    PyErr_Clear();
    Py_XDECREF(invalidate);
    // If we're truly out of memory, wiping out everything is a fine fallback:
    _Py_Executors_InvalidateAll(interp, is_invalidation);
}

/* Invalidate all executors */
void
_Py_Executors_InvalidateAll(PyInterpreterState *interp, int is_invalidation)
{
    while (interp->executor_list_head) {
        _PyExecutorObject *executor = interp->executor_list_head;
        assert(executor->vm_data.valid == 1 && executor->vm_data.linked == 1);
        if (executor->vm_data.code) {
            // Clear the entire code object so its co_executors array be freed:
            _PyCode_Clear_Executors(executor->vm_data.code);
        }
        else {
            executor_clear(executor);
        }
        if (is_invalidation) {
            OPT_STAT_INC(executors_invalidated);
        }
    }
}

void
_Py_Executors_InvalidateCold(PyInterpreterState *interp)
{
    /* Walk the list of executors */
    /* TO DO -- Use a tree to avoid traversing as many objects */
    PyObject *invalidate = PyList_New(0);
    if (invalidate == NULL) {
        goto error;
    }

    /* Clearing an executor can deallocate others, so we need to make a list of
     * executors to invalidate first */
    for (_PyExecutorObject *exec = interp->executor_list_head; exec != NULL;) {
        assert(exec->vm_data.valid);
        _PyExecutorObject *next = exec->vm_data.links.next;

        if (!exec->vm_data.warm && PyList_Append(invalidate, (PyObject *)exec) < 0) {
            goto error;
        }
        else {
            exec->vm_data.warm = false;
        }

        exec = next;
    }
    for (Py_ssize_t i = 0; i < PyList_GET_SIZE(invalidate); i++) {
        _PyExecutorObject *exec = (_PyExecutorObject *)PyList_GET_ITEM(invalidate, i);
        executor_clear(exec);
    }
    Py_DECREF(invalidate);
    return;
error:
    PyErr_Clear();
    Py_XDECREF(invalidate);
    // If we're truly out of memory, wiping out everything is a fine fallback
    _Py_Executors_InvalidateAll(interp, 0);
}

static void
write_str(PyObject *str, FILE *out)
{
    // Encode the Unicode object to the specified encoding
    PyObject *encoded_obj = PyUnicode_AsEncodedString(str, "utf8", "strict");
    if (encoded_obj == NULL) {
        PyErr_Clear();
        return;
    }
    const char *encoded_str = PyBytes_AsString(encoded_obj);
    Py_ssize_t encoded_size = PyBytes_Size(encoded_obj);
    fwrite(encoded_str, 1, encoded_size, out);
    Py_DECREF(encoded_obj);
}

static int
find_line_number(PyCodeObject *code, _PyExecutorObject *executor)
{
    int code_len = (int)Py_SIZE(code);
    for (int i = 0; i < code_len; i++) {
        _Py_CODEUNIT *instr = &_PyCode_CODE(code)[i];
        int opcode = instr->op.code;
        if (opcode == ENTER_EXECUTOR) {
            _PyExecutorObject *exec = code->co_executors->executors[instr->op.arg];
            if (exec == executor) {
                return PyCode_Addr2Line(code, i*2);
            }
        }
        i += _PyOpcode_Caches[_Py_GetBaseCodeUnit(code, i).op.code];
    }
    return -1;
}

/* Writes the node and outgoing edges for a single tracelet in graphviz format.
 * Each tracelet is presented as a table of the uops it contains.
 * If Py_STATS is enabled, execution counts are included.
 *
 * https://graphviz.readthedocs.io/en/stable/manual.html
 * https://graphviz.org/gallery/
 */
static void
executor_to_gv(_PyExecutorObject *executor, FILE *out)
{
    PyCodeObject *code = executor->vm_data.code;
    fprintf(out, "executor_%p [\n", executor);
    fprintf(out, "    shape = none\n");

    /* Write the HTML table for the uops */
    fprintf(out, "    label = <<table border=\"0\" cellspacing=\"0\">\n");
    fprintf(out, "        <tr><td port=\"start\" border=\"1\" ><b>Executor</b></td></tr>\n");
    if (code == NULL) {
        fprintf(out, "        <tr><td border=\"1\" >No code object</td></tr>\n");
    }
    else {
        fprintf(out, "        <tr><td  border=\"1\" >");
        write_str(code->co_qualname, out);
        int line = find_line_number(code, executor);
        fprintf(out, ": %d</td></tr>\n", line);
    }
    for (uint32_t i = 0; i < executor->code_size; i++) {
        /* Write row for uop.
         * The `port` is a marker so that outgoing edges can
         * be placed correctly. If a row is marked `port=17`,
         * then the outgoing edge is `{EXEC_NAME}:17 -> {TARGET}`
         * https://graphviz.readthedocs.io/en/stable/manual.html#node-ports-compass
         */
        _PyUOpInstruction const *inst = &executor->trace[i];
        const char *opname = _PyOpcode_uop_name[inst->opcode];
#ifdef Py_STATS
        fprintf(out, "        <tr><td port=\"i%d\" border=\"1\" >%s -- %" PRIu64 "</td></tr>\n", i, opname, inst->execution_count);
#else
        fprintf(out, "        <tr><td port=\"i%d\" border=\"1\" >%s</td></tr>\n", i, opname);
#endif
//        if (inst->opcode == _EXIT_TRACE || inst->opcode == _JUMP_TO_TOP) {
//            break;
//        }
    }
    fprintf(out, "    </table>>\n");
    fprintf(out, "]\n\n");

    /* Write all the outgoing edges */
    for (uint32_t i = 0; i < executor->code_size; i++) {
        _PyUOpInstruction const *inst = &executor->trace[i];
        uint16_t flags = _PyUop_Flags[inst->opcode];
        _PyExitData *exit = NULL;
        if (inst->opcode == _EXIT_TRACE) {
            exit = (_PyExitData *)inst->operand0;
        }
        else if (flags & HAS_EXIT_FLAG) {
            assert(inst->format == UOP_FORMAT_JUMP);
            _PyUOpInstruction const *exit_inst = &executor->trace[inst->jump_target];
            assert(exit_inst->opcode == _EXIT_TRACE);
            exit = (_PyExitData *)exit_inst->operand0;
        }
        if (exit != NULL && exit->executor != NULL) {
            fprintf(out, "executor_%p:i%d -> executor_%p:start\n", executor, i, exit->executor);
        }
//        if (inst->opcode == _EXIT_TRACE || inst->opcode == _JUMP_TO_TOP) {
//            break;
//        }
    }
}

/* Write the graph of all the live tracelets in graphviz format. */
int
_PyDumpExecutors(FILE *out)
{
    fprintf(out, "digraph ideal {\n\n");
    fprintf(out, "    rankdir = \"LR\"\n\n");
    PyInterpreterState *interp = PyInterpreterState_Get();
    for (_PyExecutorObject *exec = interp->executor_list_head; exec != NULL;) {
        executor_to_gv(exec, out);
        exec = exec->vm_data.links.next;
    }
    fprintf(out, "}\n\n");
    return 0;
}

#else

int
_PyDumpExecutors(FILE *out)
{
    PyErr_SetString(PyExc_NotImplementedError, "No JIT available");
    return -1;
}

#endif /* _Py_TIER2 */
