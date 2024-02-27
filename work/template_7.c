
#include "Python.h"

#include "pycore_call.h"
#include "pycore_ceval.h"
#include "pycore_dict.h"
#include "pycore_emscripten_signal.h"
#include "pycore_intrinsics.h"
#include "pycore_jit.h"
#include "pycore_long.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_range.h"
#include "pycore_setobject.h"
#include "pycore_sliceobject.h"

#include "ceval_macros.h"

#undef CURRENT_OPARG
#define CURRENT_OPARG() (_oparg)

#undef CURRENT_OPERAND
#define CURRENT_OPERAND() (_operand)

#undef DEOPT_IF
#define DEOPT_IF(COND, INSTNAME)     do {                                 if ((COND)) {                        goto deoptimize;             }                            } while (0)

#undef ENABLE_SPECIALIZATION
#define ENABLE_SPECIALIZATION (0)

#undef GOTO_ERROR
#define GOTO_ERROR(LABEL)            do {                                 goto LABEL ## _tier_two;     } while (0)

#undef GOTO_TIER_TWO
#define GOTO_TIER_TWO(EXECUTOR) do {      __attribute__((musttail))                         return ((jit_func)((EXECUTOR)->jit_code))(frame, stack_pointer, tstate); } while (0)

#undef GOTO_TIER_ONE
#define GOTO_TIER_ONE(TARGET) do {      _PyFrame_SetStackPointer(frame, stack_pointer);     return TARGET; } while (0)

#undef LOAD_IP
#define LOAD_IP(UNUSED)     do {                    } while (0)

#define PATCH_VALUE(TYPE, NAME, ALIAS)  \
    extern void ALIAS;                  \
    TYPE NAME = (TYPE)(uint64_t)&ALIAS;

#define REASSIGN_VALUE(TYPE, NAME, ALIAS)  \
    extern void ALIAS;                    \
    NAME = (TYPE)(uint64_t)&ALIAS;

#define PATCH_JUMP(ALIAS)                                    \
    extern void ALIAS;                                       \
    __attribute__((musttail))                                \
    return ((jit_func)&ALIAS)(frame, stack_pointer, tstate);

_Py_CODEUNIT *
_JIT_ENTRY(_PyInterpreterFrame *frame, PyObject **stack_pointer, PyThreadState *tstate)
{
    // Locals that the instruction implementations expect to exist:
    PATCH_VALUE(_PyExecutorObject *, current_executor, _JIT_EXECUTOR)
    int oparg;
    _PyUOpInstruction *next_uop;
    PATCH_VALUE(uint32_t, _target, _JIT_TARGET0)
    int opcode;

    #undef CURRENT_OPARG
    #define CURRENT_OPARG() (_oparg0)
    #undef CURRENT_OPERAND
    #define CURRENT_OPERAND() (_operand0)
    
    int opcode0 = _JIT_OPCODE0;
    opcode = opcode0;
    PATCH_VALUE(uint16_t, _oparg0, _JIT_OPARG0)
    PATCH_VALUE(uint64_t, _operand0, _JIT_OPERAND0)
    
    if (opcode0 == _JUMP_TO_TOP) {
        CHECK_EVAL_BREAKER();
        PATCH_JUMP(_JIT_TOP);
    }
    switch (opcode0) {
#include "executor_cases.c.h"
        default:
            Py_UNREACHABLE();
    }

    #undef CURRENT_OPARG
    #define CURRENT_OPARG() (_oparg1)
    #undef CURRENT_OPERAND
    #define CURRENT_OPERAND() (_operand1)
    
    int opcode1 = _JIT_OPCODE1;
    opcode = opcode1;
    PATCH_VALUE(uint16_t, _oparg1, _JIT_OPARG1)
    PATCH_VALUE(uint64_t, _operand1, _JIT_OPERAND1)
    REASSIGN_VALUE(uint32_t, _target, _JIT_TARGET1);
    
    switch (opcode1) {
#include "executor_cases.c.h"
        default:
            Py_UNREACHABLE();
    }

    #undef CURRENT_OPARG
    #define CURRENT_OPARG() (_oparg2)
    #undef CURRENT_OPERAND
    #define CURRENT_OPERAND() (_operand2)
    
    int opcode2 = _JIT_OPCODE2;
    opcode = opcode2;
    PATCH_VALUE(uint16_t, _oparg2, _JIT_OPARG2)
    PATCH_VALUE(uint64_t, _operand2, _JIT_OPERAND2)
    REASSIGN_VALUE(uint32_t, _target, _JIT_TARGET2);
    
    switch (opcode2) {
#include "executor_cases.c.h"
        default:
            Py_UNREACHABLE();
    }

    #undef CURRENT_OPARG
    #define CURRENT_OPARG() (_oparg3)
    #undef CURRENT_OPERAND
    #define CURRENT_OPERAND() (_operand3)
    
    int opcode3 = _JIT_OPCODE3;
    opcode = opcode3;
    PATCH_VALUE(uint16_t, _oparg3, _JIT_OPARG3)
    PATCH_VALUE(uint64_t, _operand3, _JIT_OPERAND3)
    REASSIGN_VALUE(uint32_t, _target, _JIT_TARGET3);
    
    switch (opcode3) {
#include "executor_cases.c.h"
        default:
            Py_UNREACHABLE();
    }

    #undef CURRENT_OPARG
    #define CURRENT_OPARG() (_oparg4)
    #undef CURRENT_OPERAND
    #define CURRENT_OPERAND() (_operand4)
    
    int opcode4 = _JIT_OPCODE4;
    opcode = opcode4;
    PATCH_VALUE(uint16_t, _oparg4, _JIT_OPARG4)
    PATCH_VALUE(uint64_t, _operand4, _JIT_OPERAND4)
    REASSIGN_VALUE(uint32_t, _target, _JIT_TARGET4);
    
    switch (opcode4) {
#include "executor_cases.c.h"
        default:
            Py_UNREACHABLE();
    }

    #undef CURRENT_OPARG
    #define CURRENT_OPARG() (_oparg5)
    #undef CURRENT_OPERAND
    #define CURRENT_OPERAND() (_operand5)
    
    int opcode5 = _JIT_OPCODE5;
    opcode = opcode5;
    PATCH_VALUE(uint16_t, _oparg5, _JIT_OPARG5)
    PATCH_VALUE(uint64_t, _operand5, _JIT_OPERAND5)
    REASSIGN_VALUE(uint32_t, _target, _JIT_TARGET5);
    
    switch (opcode5) {
#include "executor_cases.c.h"
        default:
            Py_UNREACHABLE();
    }

    #undef CURRENT_OPARG
    #define CURRENT_OPARG() (_oparg6)
    #undef CURRENT_OPERAND
    #define CURRENT_OPERAND() (_operand6)
    
    int opcode6 = _JIT_OPCODE6;
    opcode = opcode6;
    PATCH_VALUE(uint16_t, _oparg6, _JIT_OPARG6)
    PATCH_VALUE(uint64_t, _operand6, _JIT_OPERAND6)
    REASSIGN_VALUE(uint32_t, _target, _JIT_TARGET6);
    
    switch (opcode6) {
#include "executor_cases.c.h"
        default:
            Py_UNREACHABLE();
    }

    PATCH_JUMP(_JIT_CONTINUE);
    // Labels that the instruction implementations expect to exist:
unbound_local_error_tier_two:
    _PyEval_FormatExcCheckArg(
        tstate, PyExc_UnboundLocalError, UNBOUNDLOCAL_ERROR_MSG,
        PyTuple_GetItem(_PyFrame_GetCode(frame)->co_localsplusnames, oparg));
    goto error_tier_two;
pop_4_error_tier_two:
    STACK_SHRINK(1);
pop_3_error_tier_two:
    STACK_SHRINK(1);
pop_2_error_tier_two:
    STACK_SHRINK(1);
pop_1_error_tier_two:
    STACK_SHRINK(1);
error_tier_two:
    tstate->previous_executor = (PyObject *)current_executor;
    GOTO_TIER_ONE(NULL);
deoptimize:
    tstate->previous_executor = (PyObject *)current_executor;
    GOTO_TIER_ONE(_PyCode_CODE(_PyFrame_GetCode(frame)) + _target);
side_exit:
    {
        _PyExitData *exit = &current_executor->exits[_target];
        Py_INCREF(exit->executor);
        tstate->previous_executor = (PyObject *)current_executor;
        GOTO_TIER_TWO(exit->executor);
    }
}