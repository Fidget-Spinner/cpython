#include "Python.h"

#include "pycore_backoff.h"
#include "pycore_call.h"
#include "pycore_cell.h"
#include "pycore_ceval.h"
#include "pycore_code.h"
#include "pycore_descrobject.h"
#include "pycore_dict.h"
#include "pycore_emscripten_signal.h"
#include "pycore_floatobject.h"
#include "pycore_frame.h"
#include "pycore_function.h"
#include "pycore_genobject.h"
#include "pycore_interpframe.h"
#include "pycore_interpolation.h"
#include "pycore_intrinsics.h"
#include "pycore_jit.h"
#include "pycore_list.h"
#include "pycore_long.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_optimizer.h"
#include "pycore_pyatomic_ft_wrappers.h"
#include "pycore_range.h"
#include "pycore_setobject.h"
#include "pycore_sliceobject.h"
#include "pycore_stackref.h"
#include "pycore_template.h"
#include "pycore_tuple.h"
#include "pycore_unicodeobject.h"
#include "pycore_uop_metadata.h"

#include "ceval_macros.h"

#include "jit.h"

#undef CURRENT_OPARG
#define CURRENT_OPARG() (_oparg)

#undef CURRENT_OPERAND0
#define CURRENT_OPERAND0() (_operand0)

#undef CURRENT_OPERAND1
#define CURRENT_OPERAND1() (_operand1)

#undef CURRENT_TARGET
#define CURRENT_TARGET() (_target)

#undef GOTO_TIER_TWO
#define GOTO_TIER_TWO(EXECUTOR)                                            \
do {                                                                       \
    OPT_STAT_INC(traces_executed);                                         \
    _PyExecutorObject *_executor = (EXECUTOR);                             \
    tstate->current_executor = (PyObject *)_executor;                      \
    jit_func_preserve_none jitted = _executor->jit_side_entry;             \
    __attribute__((musttail)) return jitted(frame, stack_pointer, tstate); \
} while (0)

#undef GOTO_TIER_ONE
#define GOTO_TIER_ONE(TARGET)                       \
do {                                                \
    tstate->current_executor = NULL;                \
    _PyFrame_SetStackPointer(frame, stack_pointer); \
    return TARGET;                                  \
} while (0)

#undef LOAD_IP
#define LOAD_IP(UNUSED) \
    do {                \
    } while (0)

#undef LLTRACE_RESUME_FRAME
#define LLTRACE_RESUME_FRAME() \
    do {                       \
    } while (0)

#define PATCH_JUMP(ALIAS)                                                \
do {                                                                     \
    PATCH_VALUE(jit_func_preserve_none, jump, ALIAS);                    \
    __attribute__((musttail)) return jump(frame, stack_pointer, tstate); \
} while (0)

#undef JUMP_TO_JUMP_TARGET
#define JUMP_TO_JUMP_TARGET() dump_stack(frame, stack_pointer); PATCH_JUMP(_JIT_JUMP_TARGET)

#undef JUMP_TO_ERROR
#define JUMP_TO_ERROR() PATCH_JUMP(_JIT_ERROR_TARGET)

#define TIER_TWO 2

#ifdef Py_DEBUG
static inline void
_PyUOpPrint(const _PyUOpInstruction *uop)
{
    const char *name = _PyOpcode_uop_name[uop->opcode];
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

static inline void
dump_item(_PyStackRef item)
{
    if (PyStackRef_IsNull(item)) {
        printf("<NULL>");
        return;
    }
    if (PyStackRef_IsTaggedInt(item)) {
        printf("%" PRId64, (int64_t)PyStackRef_UntagInt(item));
        return;
    }
    PyObject *obj = PyStackRef_AsPyObjectBorrow(item);
    if (obj == NULL) {
        printf("<nil>");
        return;
    }
    // Don't call __repr__(), it might recurse into the interpreter.
    printf("<%s at %p>", Py_TYPE(obj)->tp_name, (void *)obj);
}

static inline void
dump_stack(_PyInterpreterFrame *frame, _PyStackRef *stack_pointer)
{
    _PyFrame_SetStackPointer(frame, stack_pointer);
    _PyStackRef *locals_base = _PyFrame_GetLocalsArray(frame);
    _PyStackRef *stack_base = _PyFrame_Stackbase(frame);
    PyObject *exc = PyErr_GetRaisedException();
    printf("    locals=[");
    for (_PyStackRef *ptr = locals_base; ptr < stack_base; ptr++) {
        if (ptr != locals_base) {
            printf(", ");
        }
        dump_item(*ptr);
    }
    printf("]\n");
    if (stack_pointer < stack_base) {
        printf("    stack=%d\n", (int)(stack_pointer-stack_base));
    }
    else {
        printf("    stack=[");
        for (_PyStackRef *ptr = stack_base; ptr < stack_pointer; ptr++) {
            if (ptr != stack_base) {
                printf(", ");
            }
            dump_item(*ptr);
        }
        printf("]\n");
    }
    fflush(stdout);
    PyErr_SetRaisedException(exc);
    _PyFrame_GetStackPointer(frame);
}
#endif

__attribute__((preserve_none)) _Py_CODEUNIT *
_JIT_ENTRY(_PyInterpreterFrame *frame, _PyStackRef *stack_pointer, PyThreadState *tstate)
{
    // Locals that the instruction implementations expect to exist:
    PATCH_VALUE(_PyExecutorObject *, current_executor, _JIT_EXECUTOR)
    int oparg;
    int uopcode = _JIT_OPCODE;
    _Py_CODEUNIT *next_instr;
    REGISTER_BANK_DEF
    // Other stuff we need handy:
    PATCH_VALUE(uint16_t, _oparg, _JIT_OPARG)
#if SIZEOF_VOID_P == 8
    PATCH_VALUE(uint64_t, _operand0, _JIT_OPERAND0)
    PATCH_VALUE(uint64_t, _operand1, _JIT_OPERAND1)
#else
    assert(SIZEOF_VOID_P == 4);
    PATCH_VALUE(uint32_t, _operand0_hi, _JIT_OPERAND0_HI)
    PATCH_VALUE(uint32_t, _operand0_lo, _JIT_OPERAND0_LO)
    uint64_t _operand0 = ((uint64_t)_operand0_hi << 32) | _operand0_lo;
    PATCH_VALUE(uint32_t, _operand1_hi, _JIT_OPERAND1_HI)
    PATCH_VALUE(uint32_t, _operand1_lo, _JIT_OPERAND1_LO)
    uint64_t _operand1 = ((uint64_t)_operand1_hi << 32) | _operand1_lo;
#endif
//    _PyUOpInstruction temp;
//    temp.opcode = uopcode;
//    temp.oparg = CURRENT_OPARG();
//    _PyUOpPrint(&temp),
//    printf("\n");
//    dump_stack(frame, stack_pointer);
    PATCH_VALUE(uint32_t, _target, _JIT_TARGET)
    OPT_STAT_INC(uops_executed);
    UOP_STAT_INC(uopcode, execution_count);
    switch (uopcode) {
        // The actual instruction definition gets inserted here:
        CASE
        default:
            Py_UNREACHABLE();
    }
    PATCH_JUMP(_JIT_CONTINUE);
}