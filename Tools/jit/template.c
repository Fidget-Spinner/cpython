#include "Python.h"

#include "pycore_backoff.h"
#include "pycore_call.h"
#include "pycore_ceval.h"
#include "pycore_cell.h"
#include "pycore_dict.h"
#include "pycore_emscripten_signal.h"
#include "pycore_intrinsics.h"
#include "pycore_jit.h"
#include "pycore_long.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_optimizer.h"
#include "pycore_pyatomic_ft_wrappers.h"
#include "pycore_range.h"
#include "pycore_setobject.h"
#include "pycore_sliceobject.h"
#include "pycore_descrobject.h"
#include "pycore_stackref.h"

#include "ceval_macros.h"

#include "jit.h"

#undef CURRENT_OPARG
#define CURRENT_OPARG(INST_N) (_oparg ## INST_N)

#undef CURRENT_OPERAND0
#define CURRENT_OPERAND0(INST_N) (_operand0_ ## INST_N)

#undef CURRENT_OPERAND1
#define CURRENT_OPERAND1(INST_N) (_operand1_ ## INST_N)

#undef DEOPT_IF
#define DEOPT_IF(COND, INSTNAME) \
    do {                         \
        if ((COND)) {            \
            goto deoptimize;     \
        }                        \
    } while (0)

#undef ENABLE_SPECIALIZATION
#define ENABLE_SPECIALIZATION (0)

#undef GOTO_ERROR
#define GOTO_ERROR(LABEL)        \
    do {                         \
        goto LABEL ## _tier_two; \
    } while (0)

#undef GOTO_TIER_TWO
#define GOTO_TIER_TWO(EXECUTOR) \
do {  \
    OPT_STAT_INC(traces_executed);                \
    __attribute__((musttail))                     \
    return ((jit_func_preserve_none)((EXECUTOR)->jit_side_entry))(frame, stack_pointer, tstate); \
} while (0)

#undef GOTO_TIER_ONE
#define GOTO_TIER_ONE(TARGET) \
do {  \
    _PyFrame_SetStackPointer(frame, stack_pointer); \
    return TARGET; \
} while (0)

#undef LOAD_IP
#define LOAD_IP(UNUSED) \
    do {                \
    } while (0)

#define PATCH_VALUE(TYPE, NAME, ALIAS)  \
    PyAPI_DATA(void) ALIAS;             \
    TYPE NAME = (TYPE)(uintptr_t)&ALIAS;

#define PATCH_JUMP(ALIAS)                                    \
do {                                                         \
    PyAPI_DATA(void) ALIAS;                                  \
    __attribute__((musttail))                                \
    return ((jit_func_preserve_none)&ALIAS)(frame, stack_pointer, tstate); \
} while (0)

#undef JUMP_TO_JUMP_TARGET
#define JUMP_TO_JUMP_TARGET(INST_N) PATCH_JUMP(_JIT_JUMP_TARGET ## INST_N)

#undef JUMP_TO_ERROR
#define JUMP_TO_ERROR(INST_N) PATCH_JUMP(_JIT_ERROR_TARGET ## INST_N)

#undef WITHIN_STACK_BOUNDS
#define WITHIN_STACK_BOUNDS() 1

#define TIER_TWO 2

__attribute__((preserve_none)) _Py_CODEUNIT *
_JIT_ENTRY(_PyInterpreterFrame *frame, _PyStackRef *stack_pointer, PyThreadState *tstate)
{
    // Locals that the instruction implementations expect to exist:
    PATCH_VALUE(_PyExecutorObject *, current_executor, _JIT_EXECUTOR)
    int opcode = _JIT_OPCODE;
    int oparg;
    _Py_CODEUNIT *next_instr;

    // Other stuff we need handy:
    PATCH_VALUE(uint16_t, _oparg0, _JIT_OPARG0)
    PATCH_VALUE(uint16_t, _oparg1, _JIT_OPARG1)
    PATCH_VALUE(uint16_t, _oparg2, _JIT_OPARG2)
    PATCH_VALUE(uint16_t, _oparg3, _JIT_OPARG3)
    PATCH_VALUE(uint16_t, _oparg4, _JIT_OPARG4)
    PATCH_VALUE(uint16_t, _oparg5, _JIT_OPARG5)
    PATCH_VALUE(uint16_t, _oparg6, _JIT_OPARG6)
    PATCH_VALUE(uint16_t, _oparg7, _JIT_OPARG7)
    PATCH_VALUE(uint16_t, _oparg8, _JIT_OPARG8)

    uint16_t super_opargs[] = {
        _oparg0,
        _oparg1,
        _oparg2,
        _oparg3,
        _oparg4,
        _oparg5,
        _oparg6,
        _oparg7,
        _oparg8,
    };

#if SIZEOF_VOID_P == 8
    PATCH_VALUE(uint64_t, _operand0_0, _JIT_OPERAND0_0)
    PATCH_VALUE(uint64_t, _operand1_0, _JIT_OPERAND1_0)

    PATCH_VALUE(uint64_t, _operand0_1, _JIT_OPERAND0_1)
    PATCH_VALUE(uint64_t, _operand1_1, _JIT_OPERAND1_1)

    PATCH_VALUE(uint64_t, _operand0_2, _JIT_OPERAND0_2)
    PATCH_VALUE(uint64_t, _operand1_2, _JIT_OPERAND1_2)

    PATCH_VALUE(uint64_t, _operand0_3, _JIT_OPERAND0_3)
    PATCH_VALUE(uint64_t, _operand1_3, _JIT_OPERAND1_3)

    PATCH_VALUE(uint64_t, _operand0_4, _JIT_OPERAND0_4)
    PATCH_VALUE(uint64_t, _operand1_4, _JIT_OPERAND1_4)

    PATCH_VALUE(uint64_t, _operand0_5, _JIT_OPERAND0_5)
    PATCH_VALUE(uint64_t, _operand1_5, _JIT_OPERAND1_5)

    PATCH_VALUE(uint64_t, _operand0_6, _JIT_OPERAND0_6)
    PATCH_VALUE(uint64_t, _operand1_6, _JIT_OPERAND1_6)

    PATCH_VALUE(uint64_t, _operand0_7, _JIT_OPERAND0_7)
    PATCH_VALUE(uint64_t, _operand1_7, _JIT_OPERAND1_7)

    PATCH_VALUE(uint64_t, _operand0_8, _JIT_OPERAND0_8)
    PATCH_VALUE(uint64_t, _operand1_8, _JIT_OPERAND1_8)

#else
    // Super instructions not supported on 32-bit for now.
    assert(SIZEOF_VOID_P == 4);
    PATCH_VALUE(uint32_t, _operand0_hi, _JIT_OPERAND0_HI)
    PATCH_VALUE(uint32_t, _operand0_lo, _JIT_OPERAND0_LO)
    uint64_t _operand0 = ((uint64_t)_operand0_hi << 32) | _operand0_lo;

    PATCH_VALUE(uint32_t, _operand1_hi, _JIT_OPERAND1_HI)
    PATCH_VALUE(uint32_t, _operand1_lo, _JIT_OPERAND1_LO)
    uint64_t _operand1 = ((uint64_t)_operand1_hi << 32) | _operand1_lo;
#endif
    PATCH_VALUE(uint32_t, _target0, _JIT_TARGET0)
    PATCH_VALUE(uint32_t, _target1, _JIT_TARGET1)
    PATCH_VALUE(uint32_t, _target2, _JIT_TARGET2)
    PATCH_VALUE(uint32_t, _target3, _JIT_TARGET3)
    PATCH_VALUE(uint32_t, _target4, _JIT_TARGET4)
    PATCH_VALUE(uint32_t, _target5, _JIT_TARGET5)
    PATCH_VALUE(uint32_t, _target6, _JIT_TARGET6)
    PATCH_VALUE(uint32_t, _target7, _JIT_TARGET7)
    PATCH_VALUE(uint32_t, _target8, _JIT_TARGET8)
    PATCH_VALUE(uint32_t, _target9, _JIT_TARGET9)

    uint16_t super_targets[] = {
        _target0,
        _target1,
        _target2,
        _target3,
        _target4,
        _target5,
        _target6,
        _target7,
        _target8,
    };

    OPT_STAT_INC(uops_executed);

    UOP_STAT_INC(opcode, execution_count);
    switch (opcode) {
        // The actual instruction definition gets inserted here:
        CASE
        default:
            Py_UNREACHABLE();
    }

    error_tier_two:
        tstate->previous_executor = (PyObject *)current_executor;
        GOTO_TIER_ONE(NULL);
    exit_to_tier1:
        tstate->previous_executor = (PyObject *)current_executor;
        GOTO_TIER_ONE(_PyCode_CODE(_PyFrame_GetCode(frame)) + _target0);
    exit_to_tier1_dynamic:
        tstate->previous_executor = (PyObject *)current_executor;
        GOTO_TIER_ONE(frame->instr_ptr);
    PATCH_JUMP(_JIT_CONTINUE);
    // Labels that the instruction implementations expect to exist:

}
