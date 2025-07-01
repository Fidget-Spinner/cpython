#include "Python.h"

#include "pycore_ceval.h"
#include "pycore_frame.h"
#include "pycore_jit.h"

#include "jit.h"

_Py_CODEUNIT *
_JIT_ENTRY(_PyInterpreterFrame *frame, _PyStackRef *stack_pointer, PyThreadState *tstate, PyObject **PYOBJECT_SCRATCH)
{
    // Note that this is *not* a tail call:
    PATCH_VALUE(jit_func_preserve_none, call, _JIT_CONTINUE);
    return call(frame, stack_pointer, tstate, PYOBJECT_SCRATCH);
}
