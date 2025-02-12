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
    // Note that this is *not* a tail call:
#if Py_TAIL_CALL_INTERP
    PATCH_VALUE(jit_func_preserve_none, call, _JIT_CONTINUE);
    Py_MUSTTAIL return call(TAIL_CALL_ARGS);
#else
    PATCH_VALUE(jit_func_preserve_none, call, _JIT_CONTINUE);
    return call(frame, stack_pointer, tstate);
#endif
}
