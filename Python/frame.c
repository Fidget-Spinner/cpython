
#define _PY_INTERPRETER

#include "Python.h"
#include "frameobject.h"
#include "pycore_code.h"          // stats
#include "pycore_frame.h"
#include "pycore_object.h"        // _PyObject_GC_UNTRACK()
#include "opcode.h"

// Reconstructs inlined frames.
void
_PyFrame_Reconstruct(_PyInterpreterFrame *frame)
{
    assert(frame->has_inlinee);
    assert(frame->first_inlined_frame_offset > 0);
    _PyStackRef *old_sp = frame->stackpointer;
    _Py_CODEUNIT *old_ip = frame->instr_ptr;
    // Layout:
    // Frame:
    // 0. Offset from current localsplus to start frame.
    // 1. __globals__
    // 2. __builtins__
    // 3. __code__
    // 4. __funcobj__
    // 5. instr_ptr
    // 6. n_stackentries
    // 7. return_offset
    // 8. Next frame's offset from host frame's localsplus.
    // 9. _EXIT_TRACE Note: oparg = 0 if no more frames left.
    _PyInterpreterFrame *first_inlined = (_PyInterpreterFrame *)(frame->localsplus + frame->first_inlined_frame_offset);
    _PyUOpInstruction *reconstruction = (_PyUOpInstruction *)first_inlined->previous;
    assert(reconstruction->oparg == _RECONSTRUCTION_INFO);
    _PyInterpreterFrame *prev = frame->previous;
    _PyInterpreterFrame *next = frame;

    next->instr_ptr = ((_Py_CODEUNIT *)reconstruction[5].operand);
    next->stackpointer = next->localsplus + (reconstruction[6].operand);

    assert(reconstruction[9].opcode == _EXIT_TRACE);
    reconstruction += 9;
    prev = next;
    while (reconstruction[9].oparg) {
        next = (_PyInterpreterFrame *)frame->localsplus + reconstruction->operand;
        next->f_globals = ((PyObject *)reconstruction[1].operand);
        next->f_builtins = ((PyObject *)reconstruction[2].operand);
        next->f_executable = Py_NewRef((PyObject *)reconstruction[3].operand);
        next->f_funcobj = Py_NewRef((PyObject *)reconstruction[4].operand);
        next->instr_ptr = ((_Py_CODEUNIT *)reconstruction[5].operand);
        next->stackpointer = next->localsplus + (reconstruction[6].operand);
        next->return_offset = (reconstruction[7].operand);
        next->first_inlined_frame_offset = 0;
        next->owner = FRAME_OWNED_BY_THREAD;
        next->previous = prev;
        prev = next;
        assert(reconstruction[9].opcode == _EXIT_TRACE);
        reconstruction += 9;
    }

    // If the frame is the topmost frame, set the stack pointer and instr ptr to the current one.
    next->stackpointer = old_sp;
    next->instr_ptr = old_ip;
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
