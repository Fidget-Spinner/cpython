#include "Python.h"

#include "pycore_ceval.h"
#include "pycore_frame.h"
#include "pycore_jit.h"

#include "ceval_macros.h"

#include "jit.h"


Py_PRESERVE_NONE_CC PyObject *
_JIT_ENTRY(TAIL_CALL_PARAMS)
{
    // This is subtle. The actual trace will return to us once it exits, so we
    // need to make sure that we stay alive until then. If our trace side-exits
    // into another trace, and this trace is then invalidated, the code for
    // *this function* will be freed and we'll crash upon return:
    PyAPI_DATA(void) _JIT_EXECUTOR;
    PyAPI_DATA(void) _JIT_CONTINUE;
    Py_MUSTTAIL return ((jit_func_preserve_none)&_JIT_CONTINUE)(TAIL_CALL_ARGS);
}
