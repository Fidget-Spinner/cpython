#ifndef Py_INTERNAL_JIT_H
#define Py_INTERNAL_JIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pycore_interp.h"
#include "pycore_optimizer.h"
#include "pycore_stackref.h"

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#ifdef _Py_JIT

#define JIT_PARAMS _PyInterpreterFrame *frame, _PyStackRef *stack_pointer, PyThreadState *tstate
#define JIT_ARGS frame, stack_pointer, tstate

typedef struct _JITOutlinedReturnVal {
    _PyInterpreterFrame *frame;
    _PyStackRef *stack_pointer;
    PyThreadState *tstate;
} _JITOutlinedReturnVal;

typedef _Py_CODEUNIT *(*jit_func)(JIT_PARAMS);

int _PyJIT_Compile(_PyExecutorObject *executor, const _PyUOpInstruction *trace, size_t length);
void _PyJIT_Free(_PyExecutorObject *executor);

#endif  // _Py_JIT

#ifdef __cplusplus
}
#endif

#endif // !Py_INTERNAL_JIT_H
