// To use preserve_none in JIT builds, we need to declare a separate function
// pointer with __attribute__((preserve_none)), since this attribute may not be
// supported by the compiler used to build the rest of the interpreter.
// We don't actually use preserve_all. This is just a sentinel which get
// substituted out with a preserve_none later.
typedef jit_func __attribute__((preserve_all)) jit_func_preserve_none;

#define PATCH_VALUE(TYPE, NAME, ALIAS) \
    PyAPI_DATA(void) ALIAS;            \
    TYPE NAME = (TYPE)(uintptr_t)&ALIAS;
