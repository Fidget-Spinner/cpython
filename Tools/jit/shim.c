#include "Python.h"

#include "pycore_ceval.h"
#include "pycore_frame.h"
#include "pycore_jit.h"

#include "jit.h"

__attribute__((preserve_none)) PyObject *
_JIT_ENTRY(_PyInterpreterFrame *frame, _PyStackRef *stack_pointer, PyThreadState *tstate, _Py_CODEUNIT *next_instr, int opcode, int oparg)
{
    PyAPI_DATA(void) _JIT_EXECUTOR;
    PyObject *executor = (PyObject *)(uintptr_t)&_JIT_EXECUTOR;
    Py_INCREF(executor);
    // Note that this is *not* a tail call:
    PyAPI_DATA(void) _JIT_CONTINUE;
    __attribute__((musttail))
    return ((jit_func_preserve_none)&_JIT_CONTINUE)(frame, stack_pointer, tstate, next_instr, opcode, oparg);
}
