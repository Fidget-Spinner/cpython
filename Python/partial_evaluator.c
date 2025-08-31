#ifdef _Py_TIER2

/*
 * This file contains the support code for CPython's partial evaluator.
 * It performs an abstract interpretation[1] over the trace of uops.
 * Using the information gained, it chooses to emit, or skip certain instructions
 * if possible.
 *
 * [1] For information on abstract interpertation and partial evaluation, please see
 * https://en.wikipedia.org/wiki/Abstract_interpretation
 * https://en.wikipedia.org/wiki/Partial_evaluation
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
#include "pycore_interpframe.h"  // _PyFrame_GetCode

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

#define STACK_LEVEL()     ((int)(stack_pointer - ctx->frame->stack))
#define STACK_SIZE()      ((int)(ctx->frame->stack_len))

#define WITHIN_STACK_BOUNDS() \
    (STACK_LEVEL() >= 0 && STACK_LEVEL() <= STACK_SIZE())


#define GETLOCAL(idx)          ((ctx->frame->locals[idx]))


/* _PUSH_FRAME/_RETURN_VALUE's operand can be 0, a PyFunctionObject *, or a
 * PyCodeObject *. Retrieve the code object if possible.
 */
static PyCodeObject *
get_code(_PyUOpInstruction *op)
{
    assert(op->opcode == _PUSH_FRAME || op->opcode == _RETURN_VALUE || op->opcode == _RETURN_GENERATOR);
    PyCodeObject *co = NULL;
    uint64_t operand = op->operand0;
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

static PyCodeObject *
get_code_with_logging(_PyUOpInstruction *op)
{
    PyCodeObject *co = NULL;
    uint64_t push_operand = op->operand0;
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
            return NULL;
        }
        co = (PyCodeObject *)func->func_code;
        DPRINTF(3, "code=%p ", co);
    }
    return co;
}

// Try converting the left originating inst back to its unboxed version first.
// This is more efficient for "hot" instructions like _LOAD_TAGGED_INT -> _LOAD_CONST
// As it will avoid a trip through PyLong_FromVoidPtr.
// Note that to be safe, this MUST only operate on instructions that spawn temporary stack values
// such as LOAD_TAGGED_INT and not things that could affect state elsewhere.
// Furthermore, this is why we can't insert side exits in the same pass. A previously-inserted side exit
// may now be wrong.
static bool
convert_op_to_unboxed(_PyUOpInstruction *src)
{
    int opcode = src->opcode;
    switch (opcode) {
        case _LOAD_TAGGED_INT: {
            intptr_t i = PyStackRef_UntagInt(PyStackRef_Wrap((void *)src->operand0));
            if (i < _PY_NSMALLPOSINTS && i >= 0) {
                src->opcode = _LOAD_SMALL_INT;
                src->oparg = i;
            }
            else {
                src->opcode = src->oparg ? _LOAD_CONST_INLINE_BORROW : _LOAD_CONST_INLINE;
                src->operand0 = src->operand1;
            }
            return true;
        }
        default:
            return false;
    }
}

#define sym_is_not_null _Py_uop_pe_sym_is_not_null
#define sym_is_const _Py_uop_pe_sym_is_const
#define sym_get_const _Py_uop_pe_sym_get_const
#define sym_new_unknown _Py_uop_pe_sym_new_unknown
#define sym_new_not_null _Py_uop_pe_sym_new_not_null
#define sym_is_null _Py_uop_pe_sym_is_null
#define sym_new_const _Py_uop_pe_sym_new_const
#define sym_new_null _Py_uop_pe_sym_new_null
#define sym_new_tagged_int _Py_uop_pe_sym_new_tagged_int
#define sym_is_tagged_int _Py_uop_pe_sym_is_tagged_int
#define sym_set_null(SYM) _Py_uop_pe_sym_set_null(ctx, SYM)
#define sym_set_non_null(SYM) _Py_uop_pe_sym_set_non_null(ctx, SYM)
#define sym_set_const(SYM, CNST) _Py_uop_pe_sym_set_const(ctx, SYM, CNST)
#define sym_is_bottom _Py_uop_pe_sym_is_bottom
#define frame_new _Py_uop_pe_frame_new
#define frame_pop _Py_uop_pe_frame_pop

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
    trace[trace_length].is_pe_candidate = false;
    trace[trace_length].target = target;
    trace[trace_length].oparg = oparg;
    trace[trace_length].operand0 = operand;
#ifdef Py_STATS
    trace[trace_length].execution_count = 0;
#endif
    return trace_length + 1;
}

#define CURRENT_DEST_INST() (&trace_dest[trace_dest_length-1])

#ifdef Py_DEBUG
#define ADD_TO_TRACE(OPCODE, OPARG, OPERAND, TARGET) \
    assert(trace_dest_length < UOP_MAX_TRACE_LENGTH); \
    trace_dest_length = add_to_trace(trace_dest, trace_dest_length, (OPCODE), (OPARG), (OPERAND), (TARGET)); \
    if (get_lltrace() >= 2) { \
        printf("%4d ADD_TO_PE_TRACE: ", trace_dest_length); \
        _PyUOpPrint(&trace_dest[trace_dest_length-1]); \
        printf("\n"); \
    }
#else
#define ADD_TO_TRACE(OPCODE, OPARG, OPERAND, TARGET) \
    assert(trace_dest_length < UOP_MAX_TRACE_LENGTH); \
    trace_dest_length = add_to_trace(trace_dest, trace_dest_length, (OPCODE), (OPARG), (OPERAND), (TARGET));
#endif

static inline int
copy_to_trace(
    _PyUOpInstruction *trace,
    int trace_length,
    _PyUOpInstruction *src)
{
    trace[trace_length] = *src;
    return trace_length + 1;
}

#ifdef Py_DEBUG
#define COPY_TO_TRACE(SRC) \
    assert(trace_dest_length < UOP_MAX_TRACE_LENGTH); \
    trace_dest_length = copy_to_trace(trace_dest, trace_dest_length, SRC); \
    if (get_lltrace() >= 2) { \
        printf("%4d ADD_TO_PE_TRACE: ", trace_dest_length); \
        _PyUOpPrint(&trace_dest[trace_dest_length-1]); \
        printf("\n"); \
    }
#else
#define COPY_TO_TRACE(SRC) \
    assert(trace_dest_length < UOP_MAX_TRACE_LENGTH); \
    trace_dest_length = copy_to_trace(trace_dest, trace_dest_length, SRC);
#endif


/* 1 for success, 0 for not ready, cannot error at the moment. */
static int
partial_evaluate_uops(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    int trace_length,
    int curr_stacklen,
    _PyBloomFilter *dependencies
)
{
    _PyUOpInstruction trace_dest[UOP_MAX_TRACE_LENGTH];
    int trace_dest_length = 0;
    JitOptPEContext context;
    JitOptPEContext *ctx = &context;
    uint32_t opcode = UINT16_MAX;
    int curr_space = 0;
    int max_space = 0;
    _PyUOpInstruction *first_valid_check_stack = NULL;
    _PyUOpInstruction *corresponding_check_stack = NULL;

    _Py_uop_pe_abstractcontext_init(ctx);
    _Py_UOpsPEAbstractFrame *frame = _Py_uop_pe_frame_new(ctx, co, curr_stacklen, NULL, 0);
    if (frame == NULL) {
        return -1;
    }
    ctx->curr_frame_depth++;
    ctx->frame = frame;

    _PyUOpInstruction *this_instr = NULL;
    int i = 0;
    for (; !ctx->done; i++) {
        assert(i < trace_length);
        this_instr = &trace[i];

        int oparg = this_instr->oparg;
        opcode = this_instr->opcode;
        int is_pe_candidate = this_instr->is_pe_candidate;
        JitOptPESymbol **stack_pointer = ctx->frame->stack_pointer;

#ifdef Py_DEBUG
        if (get_lltrace() >= 3) {
            printf("%4d pe: ", (int)(this_instr - trace));
            _PyUOpPrint(this_instr);
            printf("\n");
        }
#endif

        switch (opcode) {

#include "partial_evaluator_cases.c.h"

            default:
                DPRINTF(1, "\nUnknown opcode in pe's abstract interpreter\n");
                Py_UNREACHABLE();
        }
        assert(ctx->frame != NULL);
        ctx->frame->stack_pointer = stack_pointer;
        assert(STACK_LEVEL() >= 0);
    }
    if (ctx->contradiction) {
        // Attempted to push a "bottom" (contradiction) symbol onto the stack.
        // This means that the abstract interpreter has hit unreachable code.
        // We *could* generate an _EXIT_TRACE or _FATAL_ERROR here, but hitting
        // bottom indicates type instability, so we are probably better off
        // retrying later.
        DPRINTF(3, "\n");
        DPRINTF(1, "Hit bottom in pe's abstract interpreter\n");
        _Py_uop_pe_abstractcontext_fini(ctx);
        return 0;
    }
    if (ctx->out_of_space) {
        DPRINTF(3, "\n");
        DPRINTF(1, "Out of space in pe's abstract interpreter\n");
    }
    // Copy the rest in, if we did not read everything
    for (; i < trace_length; i++) {
        COPY_TO_TRACE(this_instr);
        this_instr++;
    }
    assert(trace_dest_length < UOP_MAX_TRACE_LENGTH);
    mempcpy(trace, trace_dest, sizeof(_PyUOpInstruction) * trace_dest_length);
    _Py_uop_pe_abstractcontext_fini(ctx);
    return trace_dest_length;

error:
    DPRINTF(3, "\n");
    DPRINTF(1, "Encountered error in pe's abstract interpreter\n");
    if (opcode <= MAX_UOP_ID) {
        OPT_ERROR_IN_OPCODE(opcode);
    }
    _Py_uop_pe_abstractcontext_fini(ctx);
    return -1;

}


//  0 - failure, no error raised, just fall back to Tier 1
// -1 - failure, and raise error
//  > 0 - length of optimized trace
int
_Py_uop_partial_evaluate(
    _PyInterpreterFrame *frame,
    _PyUOpInstruction *buffer,
    int length,
    int curr_stacklen,
    _PyBloomFilter *dependencies
)
{
    if (sizeof(intptr_t) != 8) {
        DPRINTF(1, "JIT PE pass disabled for 32-bit systems\n");
        return length;
    }
    length = partial_evaluate_uops(
        _PyFrame_GetCode(frame), buffer,
        length, curr_stacklen, dependencies);

    if (length <= 0) {
        return length;
    }

    return length;
}

#endif /* _Py_TIER2 */