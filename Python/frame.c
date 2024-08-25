
#define _PY_INTERPRETER

#include "Python.h"
#include "frameobject.h"
#include "pycore_code.h"          // stats
#include "pycore_frame.h"
#include "pycore_object.h"        // _PyObject_GC_UNTRACK()
#include "opcode.h"

// Reconstructs inlined frames.
_PyInterpreterFrame *
_PyFrame_Reconstruct(_PyInterpreterFrame *frame, _PyStackRef *stack_pointer)
{
    printf("RECONSTRUCTING\n");
    assert(frame->has_inlinee);
    _PyStackRef *old_sp = stack_pointer != NULL ? stack_pointer : frame->stackpointer;
    _Py_CODEUNIT *old_ip = frame->instr_ptr;
    assert(frame->real_localsplus > frame->localsplus);
    _PyInterpreterFrame *first_inlined = (_PyInterpreterFrame *)((PyObject **)frame->real_localsplus - FRAME_SPECIALS_SIZE);
    fprintf(stderr, "FIRST_INLINED_OFFSET(STACK): %ld\n", (_PyStackRef *)first_inlined - _PyFrame_Stackbase(frame));
    _PyInterpFrameReconstructor *reconstruction = (_PyInterpFrameReconstructor  *)first_inlined->previous;
    _PyInterpreterFrame *prev = frame;
    _PyInterpreterFrame *next = first_inlined;

    // Restore the host frame's reconstruction info (instr ptr and stackpointer)
    assert(PyCode_Check(reconstruction->f_executable));
    frame->instr_ptr = (reconstruction->instr_ptr);
    frame->stackpointer = frame->localsplus
        + ((PyCodeObject *)frame->f_executable)->co_nlocalsplus
        + reconstruction->n_stackentries;
    frame->return_offset = (reconstruction->return_offset);
    frame->f_names = ((PyCodeObject *)frame->f_executable)->co_names;
    frame->has_inlinee = false;
    frame->real_localsplus = frame->localsplus;

    reconstruction++;

    next->f_globals = ((PyFunctionObject *)(reconstruction->f_funcobj))->func_globals;
    next->f_builtins = ((PyFunctionObject *)(reconstruction->f_funcobj))->func_builtins;
    assert(PyCode_Check(reconstruction->f_executable));
    next->f_executable = Py_NewRef(reconstruction->f_executable);
    next->f_funcobj = Py_NewRef(reconstruction->f_funcobj);
    next->instr_ptr = reconstruction->instr_ptr;
    next->stackpointer = next->localsplus + reconstruction->n_stackentries;
    next->return_offset = reconstruction->return_offset;
    next->has_inlinee = false;
    next->owner = FRAME_OWNED_BY_THREAD;
    next->previous = prev;
    next->f_names = ((PyCodeObject *)reconstruction->f_executable)->co_names;
    next->real_localsplus = next->localsplus;

    // If the frame is the topmost frame, set the stack pointer and instr ptr to the current one.
    next->stackpointer = NULL;
    next->instr_ptr = old_ip;
    // fprintf(stderr, "HI: %d, %d\n", old_sp - _PyFrame_Stackbase(next), _PyFrame_GetCode(frame)->co_stacksize);
    assert(old_sp - _PyFrame_Stackbase(next) <= _PyFrame_GetCode(frame)->co_stacksize);
    PyThreadState *tstate = _PyThreadState_GET();
    tstate->datastack_top = ((PyObject **)next) + ((PyCodeObject *)(next->f_executable))->co_framesize;
    return next;
}

int
_PyFrame_Traverse(_PyInterpreterFrame *frame, visitproc visit, void *arg)
{
    Py_VISIT(frame->frame_obj);
    Py_VISIT(frame->f_locals);
    Py_VISIT(frame->f_funcobj);
    Py_VISIT(_PyFrame_GetCode(frame));
    return _PyGC_VisitFrameStack(frame, visit, arg);
}

PyFrameObject *
_PyFrame_MakeAndSetFrameObject(_PyInterpreterFrame *frame)
{
    assert(frame->frame_obj == NULL);
    PyObject *exc = PyErr_GetRaisedException();

    PyFrameObject *f = _PyFrame_New_NoTrack(_PyFrame_GetCode(frame));
    if (f == NULL) {
        Py_XDECREF(exc);
        return NULL;
    }
    PyErr_SetRaisedException(exc);

    // GH-97002: There was a time when a frame object could be created when we
    // are allocating the new frame object f above, so frame->frame_obj would
    // be assigned already. That path does not exist anymore. We won't call any
    // Python code in this function and garbage collection will not run.
    // Notice that _PyFrame_New_NoTrack() can potentially raise a MemoryError,
    // but it won't allocate a traceback until the frame unwinds, so we are safe
    // here.
    assert(frame->frame_obj == NULL);
    assert(frame->owner != FRAME_OWNED_BY_FRAME_OBJECT);
    assert(frame->owner != FRAME_CLEARED);
    f->f_frame = frame;
    frame->frame_obj = f;
    return f;
}

static void
take_ownership(PyFrameObject *f, _PyInterpreterFrame *frame)
{
    assert(frame->owner != FRAME_OWNED_BY_CSTACK);
    assert(frame->owner != FRAME_OWNED_BY_FRAME_OBJECT);
    assert(frame->owner != FRAME_CLEARED);
    Py_ssize_t size = ((char*)frame->stackpointer) - (char *)frame;
    Py_INCREF(_PyFrame_GetCode(frame));
    memcpy((_PyInterpreterFrame *)f->_f_frame_data, frame, size);
    frame = (_PyInterpreterFrame *)f->_f_frame_data;
    frame->stackpointer = (_PyStackRef *)(((char *)frame) + size);
    f->f_frame = frame;
    frame->owner = FRAME_OWNED_BY_FRAME_OBJECT;
    if (_PyFrame_IsIncomplete(frame)) {
        // This may be a newly-created generator or coroutine frame. Since it's
        // dead anyways, just pretend that the first RESUME ran:
        PyCodeObject *code = _PyFrame_GetCode(frame);
        frame->instr_ptr = _PyCode_CODE(code) + code->_co_firsttraceable + 1;
    }
    assert(!_PyFrame_IsIncomplete(frame));
    assert(f->f_back == NULL);
    _PyInterpreterFrame *prev = _PyFrame_GetFirstComplete(frame->previous);
    frame->previous = NULL;
    if (prev) {
        assert(prev->owner != FRAME_OWNED_BY_CSTACK);
        /* Link PyFrameObjects.f_back and remove link through _PyInterpreterFrame.previous */
        PyFrameObject *back = _PyFrame_GetFrameObject(prev);
        if (back == NULL) {
            /* Memory error here. */
            assert(PyErr_ExceptionMatches(PyExc_MemoryError));
            /* Nothing we can do about it */
            PyErr_Clear();
        }
        else {
            f->f_back = (PyFrameObject *)Py_NewRef(back);
        }
    }
    if (!_PyObject_GC_IS_TRACKED((PyObject *)f)) {
        _PyObject_GC_TRACK((PyObject *)f);
    }
}

void
_PyFrame_ClearLocals(_PyInterpreterFrame *frame)
{
    assert(frame->stackpointer != NULL);
    _PyStackRef *sp = frame->stackpointer;
    _PyStackRef *locals = frame->localsplus;
    frame->stackpointer = locals;
    while (sp > locals) {
        sp--;
        PyStackRef_XCLOSE(*sp);
    }
    Py_CLEAR(frame->f_locals);
}

void
_PyFrame_ClearExceptCode(_PyInterpreterFrame *frame)
{
    /* It is the responsibility of the owning generator/coroutine
     * to have cleared the enclosing generator, if any. */
    assert(frame->owner != FRAME_OWNED_BY_GENERATOR ||
        _PyGen_GetGeneratorFromFrame(frame)->gi_frame_state == FRAME_CLEARED);
    // GH-99729: Clearing this frame can expose the stack (via finalizers). It's
    // crucial that this frame has been unlinked, and is no longer visible:
    assert(_PyThreadState_GET()->current_frame != frame);
    if (frame->frame_obj) {
        PyFrameObject *f = frame->frame_obj;
        frame->frame_obj = NULL;
        if (Py_REFCNT(f) > 1) {
            take_ownership(f, frame);
            Py_DECREF(f);
            return;
        }
        Py_DECREF(f);
    }
    _PyFrame_ClearLocals(frame);
    Py_DECREF(frame->f_funcobj);
}

/* Unstable API functions */

PyObject *
PyUnstable_InterpreterFrame_GetCode(struct _PyInterpreterFrame *frame)
{
    PyObject *code = frame->f_executable;
    Py_INCREF(code);
    return code;
}

int
PyUnstable_InterpreterFrame_GetLasti(struct _PyInterpreterFrame *frame)
{
    return _PyInterpreterFrame_LASTI(frame) * sizeof(_Py_CODEUNIT);
}

int
PyUnstable_InterpreterFrame_GetLine(_PyInterpreterFrame *frame)
{
    int addr = _PyInterpreterFrame_LASTI(frame) * sizeof(_Py_CODEUNIT);
    return PyCode_Addr2Line(_PyFrame_GetCode(frame), addr);
}

const PyTypeObject *const PyUnstable_ExecutableKinds[PyUnstable_EXECUTABLE_KINDS+1] = {
    [PyUnstable_EXECUTABLE_KIND_SKIP] = &_PyNone_Type,
    [PyUnstable_EXECUTABLE_KIND_PY_FUNCTION] = &PyCode_Type,
    [PyUnstable_EXECUTABLE_KIND_BUILTIN_FUNCTION] = &PyMethod_Type,
    [PyUnstable_EXECUTABLE_KIND_METHOD_DESCRIPTOR] = &PyMethodDescr_Type,
    [PyUnstable_EXECUTABLE_KINDS] = NULL,
};
