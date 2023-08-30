#include "Python.h"
#include "opcode.h"
#include "pycore_interp.h"
#include "pycore_opcode.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_uops.h"
#include "pycore_long.h"
#include "cpython/optimizer.h"
#include "pycore_optimizer.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define OVERALLOCATE_FACTOR 2
#define REGISTERS_COUNT 3

#ifdef Py_DEBUG
    #define DPRINTF(level, ...) \
    if (lltrace >= (level)) { printf(__VA_ARGS__); }
#else
    #define DPRINTF(level, ...)
#endif

typedef struct _Py_UOpsSymbolicExpression {
    PyObject_VAR_HEAD
    char is_terminal;
    int opcode;
    Py_hash_t cached_hash;
    struct _Py_UOpsSymbolicExpression *operands[1];
} _Py_UOpsSymbolicExpression;

static int
sym_traverse(PyObject *o, visitproc visit, void *arg)
{
    _Py_UOpsSymbolicExpression *self = (_Py_UOpsSymbolicExpression *)o;
    Py_ssize_t n = Py_SIZE(o);
    for (Py_ssize_t i = 0; i < n; i++) {
        Py_VISIT(self->operands[i]);
    }
    return 0;
}

static int
sym_clear(PyObject *o)
{
    _Py_UOpsSymbolicExpression *self = (_Py_UOpsSymbolicExpression *)o;
    Py_ssize_t n = Py_SIZE(o);
    for (Py_ssize_t i = 0; i < n; i++) {
        Py_CLEAR(self->operands[i]);
    }
    return 0;
}

static void
sym_dealloc(PyObject *o)
{
    _Py_UOpsSymbolicExpression *self = (_Py_UOpsSymbolicExpression *)o;
    PyObject_GC_UnTrack(o);
    Py_ssize_t size = Py_SIZE(o);
    for (Py_ssize_t i = 0; i < size; i++) {
        Py_XDECREF(self->operands[i]);
    }
    Py_TYPE(o)->tp_free(o);
}

static PyTypeObject _Py_UOpsSymbolicExpression_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops symbolic expression",
    .tp_basicsize = sizeof(_Py_UOpsSymbolicExpression) - sizeof(_Py_UOpsSymbolicExpression *),
    .tp_itemsize = sizeof(_Py_UOpsSymbolicExpression *),
    .tp_dealloc = sym_dealloc,
    .tp_free = PyObject_GC_Del,
    .tp_traverse = sym_traverse,
    .tp_clear = sym_clear,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC
};

static _Py_UOpsSymbolicExpression*
_Py_UOpsSymbolicExpression_New(bool is_terminal, int num_subexprs, ...)
{
    _Py_UOpsSymbolicExpression *self = PyObject_GC_NewVar(_Py_UOpsSymbolicExpression,
                                                          &_Py_UOpsSymbolicExpression_Type,
                                                          num_subexprs);
    if (self == NULL) {
        return NULL;
    }

    self->is_terminal = (char)is_terminal;
    // Setup
    va_list curr;

    va_start(curr, num_subexprs);

    for (int i = 0; i < num_subexprs; i++) {
        self->operands[i] = va_arg(curr, _Py_UOpsSymbolicExpression *);
    }

    va_end(curr);

    return self;
}

// Snapshot of _Py_UOpsAbstractInterpContext locals BEFORE a region.
typedef struct _Py_UOpsAbstractStore {
    PyObject_VAR_HEAD
    // The next store in the trace
    struct _Py_UOpsAbstractStore *next;
    _Py_UOpsSymbolicExpression *registers[REGISTERS_COUNT];
    _Py_UOpsSymbolicExpression *locals[1];
} _Py_UOpsAbstractStore;

static void
abstractstore_dealloc(PyObject *o)
{
    _Py_UOpsAbstractStore *self = (_Py_UOpsAbstractStore *)o;
    Py_XDECREF(self->next);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject _Py_UOpsAbstractStore_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops abstract store",
    .tp_basicsize = sizeof(_Py_UOpsAbstractStore) - sizeof(_Py_UOpsSymbolicExpression *),
    .tp_itemsize = sizeof(_Py_UOpsSymbolicExpression *),
    .tp_dealloc = abstractstore_dealloc,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

static _Py_UOpsAbstractStore*
_Py_UOpsAsbstractStore_New(int locals_len)
{
    _Py_UOpsAbstractStore *self = PyObject_NewVar(_Py_UOpsAbstractStore,
                                                       &_Py_UOpsAbstractStore_Type,
                                                       locals_len);
    if (self == NULL) {
        return NULL;
    }

    self->next = NULL;

    for (int i = 0; i < REGISTERS_COUNT; i++) {
        self->registers[i] = NULL;
    }

    // Initialize with the initial state of all local variables
    for (int i = 0; i < locals_len; i++) {
        _Py_UOpsSymbolicExpression *local = _Py_UOpsSymbolicExpression_New(true, 0);
        if (local == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        self->locals[i] = local;
    }

    return self;
}

// Tier 2 types meta interpreter
typedef struct _Py_UOpsAbstractInterpContext {
    PyObject_VAR_HEAD
    _Py_UOpsSymbolicExpression *registers[REGISTERS_COUNT];
    // The following are abstract stack and locals.
    // points to one element after the abstract stack
    _Py_UOpsSymbolicExpression **stack_pointer;
    int stack_len;
    _Py_UOpsSymbolicExpression **stack;
    int locals_len;
    _Py_UOpsSymbolicExpression **locals;

    _Py_UOpsSymbolicExpression *locals_with_stack[1];

} _Py_UOpsAbstractInterpContext;

static void
abstractinterp_dealloc(PyObject *o)
{
    _Py_UOpsAbstractInterpContext *self = (_Py_UOpsAbstractInterpContext *)o;
    // Traverse all nodes and decref the root objects (if they are not NULL).
    // Note: stack is after locals so this is safe
    int total = Py_SIZE(self);
    for (int i = 0; i < total; i++) {
        Py_XDECREF(self->locals_with_stack[i]);
    }
    // No need to free stack because it is allocated together with the locals.
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject _Py_UOpsAbstractInterpContext_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops abstract interpreter's context",
    .tp_basicsize = sizeof(_Py_UOpsAbstractInterpContext) - sizeof(_Py_UOpsSymbolicExpression *),
    .tp_itemsize = sizeof(_Py_UOpsSymbolicExpression *),
    .tp_dealloc = (destructor)abstractinterp_dealloc,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

_Py_UOpsAbstractInterpContext *
_Py_UOpsAbstractInterpContext_New(_Py_UOpsAbstractStore *store,
                                  int stack_len, int locals_len, int curr_stacklen)
{
    _Py_UOpsAbstractInterpContext *self = PyObject_NewVar(_Py_UOpsAbstractInterpContext,
                                                          &_Py_UOpsAbstractInterpContext_Type,
                                                          stack_len + locals_len);
    if (self == NULL) {
        return NULL;
    }
    // Setup
    self->stack_len = stack_len;
    self->locals_len = locals_len;
    self->locals = self->locals_with_stack;
    self->stack = self->locals_with_stack + locals_len;
    self->stack_pointer = self->stack + curr_stacklen;

    memcpy(self->locals, store->locals, sizeof(_Py_UOpsSymbolicExpression *) * locals_len);

    for (int i = 0; i < stack_len; i++) {
        self->stack[i] = NULL;
    }
    return self;
}


static inline bool
op_is_jump(int opcode)
{
    return (opcode == _POP_JUMP_IF_FALSE || opcode == _POP_JUMP_IF_TRUE);
}

// Number the jump targets and the jump instructions with a unique (negative) ID.
// This replaces the instruction's opcode in the trace with their negative IDs.
// Aids relocation later when we need to recompute jumps after optimization passes.
static _PyUOpInstruction *
number_jumps_and_targets(_PyUOpInstruction *trace, int trace_len, int *max_id)
{
    int jump_and_target_count = 0;
    int jump_and_target_id = -1;
    for (int i = 0; i < trace_len; i++) {
        if (op_is_jump(trace[i].opcode)) {
            // 1 for the jump, 1 for its target
            jump_and_target_count += 2;
        }
    }

    // +1 because 1-based indexing not zero based
    _PyUOpInstruction *jump_id_to_instruction = PyMem_New(_PyUOpInstruction, jump_and_target_count + 1);
    if (jump_id_to_instruction == NULL) {
        return NULL;
    }


    for (int i = 0; i < trace_len; i++) {
        if (op_is_jump(trace[i].opcode)) {
            int target = trace[i].oparg;
            int target_id = jump_and_target_id;

            // 1 for the jump target
            assert(jump_and_target_id < 0);
            // Negative opcode!
            assert(trace[target].opcode >= 0);
            // Already assigned a jump ID
            if (trace[target].opcode < 0) {
                target_id = trace[target].opcode;
            }
            else {
                // Else, assign a new jump ID.
                jump_id_to_instruction[-target_id] = trace[target];
                trace[target].opcode = target_id;
                jump_and_target_id--;
            }

            // 1 for the jump
            assert(jump_and_target_id < 0);
            jump_id_to_instruction[-jump_and_target_id] = trace[i];
            // Negative opcode!
            assert(trace[i].opcode >= 0);
            trace[i].opcode = jump_and_target_id;
            jump_and_target_id--;
            // Point the jump to the target ID.
            trace[i].oparg = target_id;

        }
    }
    *max_id = jump_and_target_id;
    return jump_id_to_instruction;
}

// Remove contiguous SAVE_IPs, leaving only the last one before a non-SAVE_IP instruction.
static int
remove_duplicate_save_ips(_PyUOpInstruction *trace, int trace_len)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif

    // Don't have to allocate a temporary trace array
    // because the writer is guaranteed to be behind the reader.
    int new_temp_len = 0;

    _PyUOpInstruction curr;
    for (int i = 0; i < trace_len - 1; i++) {
        curr = trace[i];
        if (curr.opcode == SAVE_IP && trace[i+1].opcode == SAVE_IP) {
            continue;
        }
        trace[new_temp_len] = curr;
        new_temp_len++;
    }


    DPRINTF(2, "Removed %d SAVE_IPs\n", trace_len - new_temp_len);

    return new_temp_len;
}

/**
 * Fixes all side exits due to jumps. This MUST be called as the last
 * pass over the trace. Otherwise jumps will point to invalid ends.
 *
 * Runtime complexity of O(n*k), where n is trace length and k is number of jump
 * instructions. Since k is usually quite low, this is nearly linear.
*/
static void
fix_jump_side_exits(_PyUOpInstruction *trace, int trace_len,
                    _PyUOpInstruction *jump_id_to_instruction, int max_jump_id)
{
    for (int i = 0; i < trace_len; i++) {
        int oparg = trace[i].oparg;
        int opcode = trace[i].opcode;
        // Indicates it's a jump target or jump instruction
        if (opcode < 0 && opcode > max_jump_id) {
            opcode = -opcode;
            int real_opcode = jump_id_to_instruction[opcode].opcode;
            if (op_is_jump(real_opcode)) {
                trace[i].opcode = real_opcode;

                // Search for our target ID.
                int target_id = oparg;
                for (int x = 0; x < trace_len; x++) {
                    if (trace[x].opcode == target_id) {
                        trace[i].oparg = x;
                        break;
                    }
                }

                assert(trace[i].oparg >= 0);
            }
        }
    }

    // Final pass to swap out all the jump target IDs with their actual targets.
    for (int i = 0; i < trace_len; i++) {
        int opcode = trace[i].opcode;
        // Indicates it's a jump target or jump instruction
        if (opcode < 0 && opcode > max_jump_id) {
            int real_oparg = jump_id_to_instruction[-opcode].oparg;
            int real_opcode = jump_id_to_instruction[-opcode].opcode;
            trace[i].oparg = real_oparg;
            trace[i].opcode = real_opcode;
        }
    }
}

static inline bool
op_is_pure(int opcode)
{
    switch(opcode) {
        case LOAD_FAST:
        case LOAD_CONST:
        case BINARY_OP_ADD_INT:
        // Technically not fully pure, but because we restore state at
        // impure boundaries, this will have no visible side effect
        // to user Python code.
        case STORE_FAST:
            return true;
        default:
            return false;
    }
}

#ifndef Py_DEBUG
#define GETITEM(v, i) PyList_GET_ITEM((v), (i))
#else
static inline PyObject *
GETITEM(PyObject *v, Py_ssize_t i) {
    assert(PyList_CheckExact(v));
    assert(i >= 0);
    assert(i < PyList_GET_SIZE(v));
    return PyList_GET_ITEM(v, i);
}
#endif

static int
uop_abstract_interpret_single_inst(
    PyCodeObject *co,
    _PyUOpInstruction *inst,
    _Py_UOpsAbstractInterpContext *ctx,
    PyObject *co_const_copy,
    _PyUOpInstruction *jump_id_to_instruction,
    int max_jump_id
)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif

#define STACK_LEVEL()     ((int)(ctx->stack_pointer - ctx->stack))
#define STACK_SIZE()      (co->co_stacksize)
#define BASIC_STACKADJ(n) (ctx->stack_pointer += n)

#ifdef Py_DEBUG
#define STACK_GROW(n)   do { \
                            assert(n >= 0); \
                            BASIC_STACKADJ(n); \
                            assert(STACK_LEVEL() <= STACK_SIZE()); \
                        } while (0)
#define STACK_SHRINK(n) do { \
                            assert(n >= 0); \
                            assert(STACK_LEVEL() >= n); \
                            BASIC_STACKADJ(-(n)); \
                        } while (0)
#else
    #define STACK_GROW(n)          BASIC_STACKADJ(n)
#define STACK_SHRINK(n)        BASIC_STACKADJ(-(n))
#endif
#define PEEK(idx)              (((ctx->stack_pointer)[-(idx)]))
#define GETLOCAL(idx)          ((ctx->locals[idx]))

    int oparg = inst->oparg;
    int opcode = inst->opcode;


    // Is a special jump/target ID, decode that
    if (opcode < 0 && opcode > max_jump_id) {
        DPRINTF(2, "Special jump target/ID %d\n", opcode);
        oparg = jump_id_to_instruction[-opcode].oparg;
        opcode = jump_id_to_instruction[-opcode].opcode;
    }

    DPRINTF(2, "Abstract interpreting %s:%d\n",
            (opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[opcode],
            oparg);
    switch (opcode) {
#include "abstract_interp_cases.c.h"
        // @TODO convert these to autogenerated using DSL
        case LOAD_FAST:
        case LOAD_FAST_CHECK:
            STACK_GROW(1);
            PEEK(1) = GETLOCAL(oparg);
            break;
        case LOAD_FAST_AND_CLEAR: {
            STACK_GROW(1);
            PEEK(1) = GETLOCAL(oparg);
            GETLOCAL(oparg) = NULL;
            break;
        }
        case LOAD_CONST: {
            PyObject *value = GETITEM(co_const_copy, oparg);
            STACK_GROW(1);
            PEEK(1) = (_Py_UOpsSymbolicExpression *)value;
            break;
        }
        case STORE_FAST:
        case STORE_FAST_MAYBE_NULL: {
            _Py_UOpsSymbolicExpression *value = PEEK(1);
            GETLOCAL(oparg) = value;
            STACK_SHRINK(1);
            break;
        }
        case COPY: {
            _Py_UOpsSymbolicExpression *bottom = PEEK(1 + (oparg - 1));
            STACK_GROW(1);
            PEEK(1) = bottom;
            break;
        }

        default:
            DPRINTF(1, "Unknown opcode in abstract interpreter\n");
            Py_UNREACHABLE();
    }
    return 0;
}

static int
uop_abstract_interpret(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    _PyUOpInstruction *new_trace,
    int trace_len,
    int curr_stacklen,
    _PyUOpInstruction *jump_id_to_instruction,
    int max_jump_id
)
{

#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif

    PyObject *co_const_copy = NULL;
    _Py_UOpsAbstractInterpContext *ctx = NULL;
    _Py_UOpsAbstractStore *store = NULL;
    _Py_UOpsAbstractStore *first_store = NULL;
    store = first_store = _Py_UOpsAsbstractStore_New(co->co_nlocals);
    if (store == NULL) {
        goto abstract_error;
    }
    int buffer_trace_len = 0;

    ctx = _Py_UOpsAbstractInterpContext_New(
        store, co->co_stacksize, co->co_nlocals, curr_stacklen);
    if (ctx == NULL) {
        goto abstract_error;
    }

    // We will be adding more constants due to constant propagation.
    co_const_copy = PyList_New(PyTuple_Size(co->co_consts));
    if (co_const_copy == NULL) {
        goto abstract_error;
    }
    // Copy over the co_const tuple
    for (int x = 0; x < PyTuple_GET_SIZE(co->co_consts); x++) {
        PyList_SET_ITEM(co_const_copy, x, Py_NewRef(PyTuple_GET_ITEM(co->co_consts, x)));
    }

    _PyUOpInstruction *curr = trace;
    _PyUOpInstruction *end = trace + trace_len;

    while (curr < end) {

        // Form pure regions
        while(op_is_pure(curr->opcode)) {

            int err = uop_abstract_interpret_single_inst(
                co, curr, ctx, co_const_copy,
                jump_id_to_instruction, max_jump_id
            );
            if (err < 0) {
                goto abstract_error;
            }

            if (curr->opcode == EXIT_TRACE) {
                break;
            }

            curr++;
        }

        // End of a pure region, create a new abstract store
        if (curr->opcode != EXIT_TRACE) {
            _Py_UOpsAbstractStore *temp = store;
            store = _Py_UOpsAsbstractStore_New(co->co_nlocals);
            if (store == NULL) {
                goto abstract_error;
            }
            // Transfer the reference over (note: No incref!)
            temp->next = store;
        }

        // Form impure region
        if(!op_is_pure(curr->opcode)) {

            int err = uop_abstract_interpret_single_inst(
                co, curr, ctx, co_const_copy,
                jump_id_to_instruction, max_jump_id
            );
            if (err < 0) {
                goto abstract_error;
            }

            if (curr->opcode == EXIT_TRACE) {
                break;
            }

            curr++;

            // End of an impure instruction, create a new abstract store
            // TODO we can do some memory optimization here to not use abstract
            // stores each time, since that's quite overkill.
            if (curr->opcode != EXIT_TRACE) {
                _Py_UOpsAbstractStore *temp = store;
                store = _Py_UOpsAsbstractStore_New(co->co_nlocals);
                if (store == NULL) {
                    goto abstract_error;
                }
                // Transfer the reference over (note: No incref!)
                temp->next = store;
            };
        }

    }
    assert(STACK_SIZE() >= 0);

#ifdef Py_DEBUG
    if (buffer_trace_len < trace_len) {
        DPRINTF(2, "Shortened trace by %d instructions\n", trace_len - buffer_trace_len);
    }
#endif

    Py_DECREF(ctx);

    PyObject *co_const_final = PyTuple_New(PyList_Size(co_const_copy));
    if (co_const_final == NULL) {
        goto abstract_error;
    }
    // Copy over the co_const tuple
    for (int x = 0; x < PyList_GET_SIZE(co_const_copy); x++) {
        PyTuple_SET_ITEM(co_const_final, x, Py_NewRef(PyList_GET_ITEM(co_const_copy, x)));
    }

    Py_SETREF(co->co_consts, co_const_final);
    Py_XDECREF(co_const_copy);
    Py_DECREF(first_store);
    return buffer_trace_len;

abstract_error:
    Py_XDECREF(co_const_copy);
    Py_XDECREF(first_store);
    Py_DECREF(ctx);
    if(PyErr_Occurred()) {
        PyErr_Clear();
    }
    return trace_len;
}

int
_Py_uop_analyze_and_optimize(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    int trace_len,
    int curr_stacklen
)
{
    int original_trace_len = trace_len;
    _PyUOpInstruction *temp_writebuffer = NULL;
    _PyUOpInstruction *jump_id_to_instruction = NULL;

    temp_writebuffer = PyMem_New(_PyUOpInstruction, trace_len * OVERALLOCATE_FACTOR);
    if (temp_writebuffer == NULL) {
        goto error;
    }

    int max_jump_id = 0;

    // Pass: Jump target calculation and setup (preparation for relocation)
    jump_id_to_instruction = number_jumps_and_targets(trace, trace_len, &max_jump_id);
    if (jump_id_to_instruction == NULL) {
        goto error;
    }

    // Pass: Abstract interpretation and symbolic analysis
    trace_len = uop_abstract_interpret(co, trace, temp_writebuffer,
                                       trace_len, curr_stacklen,
                                       jump_id_to_instruction, max_jump_id);
    if (trace_len < 0) {
        goto error;
    }

    // Pass: Remove duplicate SAVE_IPs
    trace_len = remove_duplicate_save_ips(temp_writebuffer, trace_len);

    // Final pass: fix jumps. This MUST be called as the last pass!
    fix_jump_side_exits(temp_writebuffer, trace_len, jump_id_to_instruction, max_jump_id);

    // Fill in our new trace!
    // memcpy(trace, temp_writebuffer, trace_len * sizeof(_PyUOpInstruction));

    // TODO return new trace_len soon!
    return original_trace_len;
error:
    PyMem_Free(temp_writebuffer);
    PyMem_Free(jump_id_to_instruction);
    return original_trace_len;
}
