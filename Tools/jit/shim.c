#include "Python.h"

#include "pycore_ceval.h"
#include "pycore_frame.h"
#include "pycore_jit.h"

#include "jit.h"

_Py_CODEUNIT *
_JIT_ENTRY(_PyInterpreterFrame *frame, _PyStackRef *stack_pointer, PyThreadState *tstate)
{
    // Note that this is *not* a tail call:
    PATCH_VALUE(jit_func_preserve_none, call, _JIT_CONTINUE);
    PATCH_VALUE(_PyExecutorObject *, current_executor, _JIT_EXECUTOR)
    call = (jit_func_preserve_none)(current_executor->jit_state.instruction_starts[current_executor->osr_entry_offset]);
    return call(frame, stack_pointer, tstate);
}
