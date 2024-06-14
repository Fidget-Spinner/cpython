#ifndef Py_INTERNAL_RANGE_H
#define Py_INTERNAL_RANGE_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

typedef struct {
    PyObject_HEAD
    long start;
    long step;
    long len;
#ifdef Py_GIL_DISABLED
    _PySeqLock sequence;
#endif
} _PyRangeIterObject;

#ifdef __cplusplus
}
#endif
#endif   /* !Py_INTERNAL_RANGE_H */
