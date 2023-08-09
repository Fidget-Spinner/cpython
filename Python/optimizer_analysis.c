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
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "pycore_optimizer.h"

#define PARTITION_DEBUG 1

// TYPENODE is a tagged pointer that uses the last 2 LSB as the tag
#define _Py_PARTITIONNODE_t uintptr_t

// PARTITIONNODE Tags
typedef enum _Py_TypeNodeTags {
    // Node is unused
    TYPE_NULL = 0,
    // TYPE_ROOT_POSITIVE can point to a root struct or be a NULL
    TYPE_ROOT= 1,
    // TYPE_REF points to a TYPE_ROOT or a TYPE_REF
    TYPE_REF = 2,
} _Py_TypeNodeTags;

typedef struct _Py_PartitionRootNode {
    PyObject_HEAD
    // For partial evaluation
    // 0 - static
    // 1 - dynamic
    uint8_t static_or_dynamic;
    PyObject *const_val;
    // For types (TODO)
} _Py_PartitionRootNode;

static void
partitionnode_dealloc(PyObject *o)
{
    _Py_PartitionRootNode *self = (_Py_PartitionRootNode *)o;
    Py_CLEAR(self->const_val);
    Py_TYPE(self)->tp_free(o);
}

PyTypeObject _Py_PartitionRootNode_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops abstract interpreter's root node",
    .tp_basicsize = sizeof(_Py_PartitionRootNode),
    .tp_dealloc = partitionnode_dealloc,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

static inline _Py_TypeNodeTags
partitionnode_get_tag(_Py_PARTITIONNODE_t node)
{
    return node & 0b11;
}

static inline _Py_PARTITIONNODE_t
partitionnode_clear_tag(_Py_PARTITIONNODE_t node)
{
    return node & (~(uintptr_t)(0b11));
}

// static_or_dynamic
// 0 - static
// 1 - dynamic
// If static, const_value must be set!
static inline _Py_PARTITIONNODE_t
partitionnode_make_root(uint8_t static_or_dynamic, PyObject *const_val)
{
    _Py_PartitionRootNode *root = PyObject_New(_Py_PartitionRootNode, &_Py_PartitionRootNode_Type);
    if (root == NULL) {
        return 0;
    }
    root->static_or_dynamic = static_or_dynamic;
    root->const_val = Py_NewRef(const_val);
    return (_Py_PARTITIONNODE_t)root | TYPE_ROOT;
}

static inline _Py_PARTITIONNODE_t
partitionnode_make_ref(_Py_PARTITIONNODE_t *node)
{
    return partitionnode_clear_tag((_Py_PARTITIONNODE_t)node) | TYPE_REF;
}


static _Py_PARTITIONNODE_t PARTITIONNODE_NULLROOT = (_Py_PARTITIONNODE_t)_Py_NULL | TYPE_ROOT;

// Tier 2 types meta interpreter
typedef struct _Py_UOpsAbstractInterpContext {
    PyObject_HEAD
    // The following are abstract stack and locals.
    // points to one element after the abstract stack
    _Py_PARTITIONNODE_t *stack_pointer;
    int stack_len;
    _Py_PARTITIONNODE_t *stack;
    int locals_len;
    _Py_PARTITIONNODE_t *locals;

    // Indicates whether the stack entry is real or virtualised.
    // true - virtual false - real
    bool *stack_virtual_or_real;
    // The following represent the real (emitted instructions) stack and locals.
    // points to one element after the abstract stack
    _Py_PARTITIONNODE_t *real_stack_pointer;
    _Py_PARTITIONNODE_t *real_stack;
    _Py_PARTITIONNODE_t *real_locals;
} _Py_UOpsAbstractInterpContext;

static void
abstractinterp_dealloc(PyObject *o)
{
    _Py_UOpsAbstractInterpContext *self = (_Py_UOpsAbstractInterpContext *)o;
    // Traverse all nodes and decref the root objects (if they are not NULL).
    // Note: stack is after locals so this is safe
    int total = self->locals_len + self->stack_len;
    for (int i = 0; i < total; i++) {
        _Py_PARTITIONNODE_t node = self->locals[i];
        if (partitionnode_get_tag(node) == TYPE_ROOT) {
            Py_XDECREF(partitionnode_clear_tag(node));
        }
    }
    PyMem_Free(self->locals);
    // No need to free stack because it is allocated together with the locals.
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PyTypeObject _Py_UOpsAbstractInterpContext_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops abstract interpreter's context",
    .tp_basicsize = sizeof(_Py_UOpsAbstractInterpContext),
    .tp_dealloc = abstractinterp_dealloc,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

_Py_UOpsAbstractInterpContext *
_Py_UOpsAbstractInterpContext_New(int stack_len, int locals_len, int curr_stacklen)
{
    _Py_UOpsAbstractInterpContext *self = (_Py_UOpsAbstractInterpContext *)PyType_GenericAlloc(
        (PyTypeObject *)&_Py_UOpsAbstractInterpContext_Type, 0);
    if (self == NULL) {
        return NULL;
    }

    // Setup
    self->stack_len = stack_len;
    self->locals_len = locals_len;

    // Double the size needed because we also need a representation for the real stack and locals.
    _Py_PARTITIONNODE_t *locals_with_stack = PyMem_New(_Py_PARTITIONNODE_t, (locals_len + stack_len) * 2);
    if (locals_with_stack == NULL) {
        Py_DECREF(self);
        return NULL;
    }

    bool *virtual_or_real = PyMem_New(bool, stack_len);
    if (virtual_or_real == NULL) {
        Py_DECREF(self);
        PyMem_Free(locals_with_stack);
        return NULL;
    }

    for (int i = 0; i < (locals_len + stack_len) * 2; i++) {
        locals_with_stack[i] = PARTITIONNODE_NULLROOT;
    }

    self->locals = locals_with_stack;
    self->stack = locals_with_stack + locals_len;
    self->stack_pointer = self->stack + curr_stacklen;

    self->stack_virtual_or_real = virtual_or_real;

    self->real_locals = self->locals + locals_len + stack_len;
    self->real_stack = self->stack + locals_len + stack_len;
    self->real_stack_pointer = self->stack_pointer + locals_len + stack_len;
    return self;
}

#if PARTITION_DEBUG
static void print_ctx(_Py_UOpsAbstractInterpContext *ctx);
#endif

static _Py_PARTITIONNODE_t *
partitionnode_get_rootptr(_Py_PARTITIONNODE_t *ref)
{
    _Py_TypeNodeTags tag = partitionnode_get_tag(*ref);
    while (tag != TYPE_ROOT) {
        ref = (_Py_PARTITIONNODE_t *)(partitionnode_clear_tag(*ref));
        tag = partitionnode_get_tag(*ref);
    }
    return ref;
}

/**
 * @brief Checks if two nodes are in the same partition.
*/
static bool
partitionnode_is_same_partition(_Py_PARTITIONNODE_t *x, _Py_PARTITIONNODE_t *y)
{
    return partitionnode_get_rootptr(x) == partitionnode_get_rootptr(y);
}

/**
 * @brief Performs SET operation. dst tree becomes part of src tree
 *
 * If src_is_new is set, src is interpreted as a TYPE_ROOT
 * not part of the type_context. Otherwise, it is interpreted as a pointer
 * to a _Py_PARTITIONNODE_t.
 *
 * If src_is_new:
 *   Overwrites the root of the dst tree with the src node
 * else:
 *   Makes the root of the dst tree a TYPE_REF to src
 *
*/
static void
partitionnode_set(_Py_PARTITIONNODE_t *src, _Py_PARTITIONNODE_t *dst, bool src_is_new)
{
    {

#ifdef Py_DEBUG
        // If `src_is_new` is set:
        //   - `src` doesn't belong inside the type context yet.
        //   - `src` has to be a TYPE_ROOT
        //   - `src` is to be interpreted as a _Py_TYPENODE_t
        if (src_is_new) {
            assert(partitionnode_get_tag(*src) == TYPE_ROOT);
        }
#endif

        // This prevents cycles from forming
        if (!src_is_new && partitionnode_is_same_partition(src, dst)) {
            return;
        }

        _Py_TypeNodeTags tag = partitionnode_get_tag(*dst);
        switch (tag) {
            case TYPE_ROOT: {
                _Py_PARTITIONNODE_t old_root = partitionnode_clear_tag(*dst);
                Py_XDECREF(old_root);
                if (!src_is_new) {
                    // Make dst a reference to src
                    *dst = partitionnode_make_ref(src);
                    break;
                }
                // Make dst the src
                *dst = *src;
                break;
            }
            case TYPE_REF: {
                _Py_PARTITIONNODE_t *rootptr = partitionnode_get_rootptr(dst);
                _Py_PARTITIONNODE_t old_root = partitionnode_clear_tag(*rootptr);
                Py_XDECREF(old_root);
                if (!src_is_new) {
                    // Traverse up to the root of dst, make root a reference to src
                    *rootptr = partitionnode_make_ref(src);
                    break;
                }
                // Make root of dst the src
                *rootptr = *src;
                break;
            }
            default:
                Py_UNREACHABLE();
            }
    }
}


/**
 * @brief Performs OVERWRITE operation. dst node gets overwritten by src node
 *
 * If src_is_new is set, src is interpreted as a TYPE_ROOT
 * not part of the ctx. Otherwise, it is interpreted as a pointer
 * to a _Py_PARTITIONNODE_t.
 *
 * If src_is_new:
 *   Removes dst node from its tree (+fixes all the references to dst)
 *   Overwrite the dst node with the src node
 * else:
 *   Removes dst node from its tree (+fixes all the references to dst)
 *   Makes the root of the dst tree a TYPE_REF to src
 *
*/
static void
partitionnode_overwrite(_Py_UOpsAbstractInterpContext *ctx,
    _Py_PARTITIONNODE_t *src, _Py_PARTITIONNODE_t *dst, bool src_is_new)
{
#ifdef Py_DEBUG
    if (src_is_new) {
        assert(partitionnode_get_tag((_Py_PARTITIONNODE_t)src) == TYPE_ROOT);
    }
#endif

    // This prevents cycles from forming
    if (!src_is_new && partitionnode_is_same_partition(src, dst)) {
        return;
    }

    _Py_TypeNodeTags tag = partitionnode_get_tag(*dst);
    switch (tag) {
        case TYPE_ROOT: {

            _Py_PARTITIONNODE_t old_dst = *dst;
            if (!src_is_new) {
                // Make dst a reference to src
                *dst = partitionnode_make_ref(src);
                assert(partitionnode_clear_tag(*dst) != (_Py_PARTITIONNODE_t)_Py_NULL);
            }
            else {
                // Make dst the src
                *dst = (_Py_PARTITIONNODE_t)src;
            }


            /* Pick one child of dst and make that the new root of the dst tree */

            // Children of dst will have this form
            _Py_PARTITIONNODE_t child_test = partitionnode_make_ref(
                (_Py_PARTITIONNODE_t *)partitionnode_clear_tag((_Py_PARTITIONNODE_t)dst));
            // Will be initialised to the first child we find
            _Py_PARTITIONNODE_t *new_root = (_Py_PARTITIONNODE_t *)NULL;

            // Search locals for children
            int nlocals = ctx->locals_len;
            for (int i = 0; i < nlocals; i++) {
                _Py_PARTITIONNODE_t *node_ptr = &(ctx->locals[i]);
                if (*node_ptr == child_test) {
                    if (new_root == (_Py_PARTITIONNODE_t)NULL) {
                        // First child encountered! initialise root
                        new_root = node_ptr;
                        *node_ptr = old_dst;
                        Py_XINCREF(partitionnode_clear_tag(old_dst));
                    }
                    else {
                        // Not the first child encounted, point it to the new root
                        *node_ptr = partitionnode_make_ref(new_root);
                    }
                }
            }

            // Search stack for children
            int nstack = ctx->stack_len;
            for (int i = 0; i < nstack; i++) {
                _Py_PARTITIONNODE_t *node_ptr = &(ctx->stack[i]);
                if (*node_ptr == child_test) {
                    if (new_root == (_Py_PARTITIONNODE_t)NULL) {
                        // First child encountered! initialise root
                        new_root = node_ptr;
                        *node_ptr = old_dst;
                        Py_XINCREF(partitionnode_clear_tag(old_dst));
                    }
                    else {
                        // Not the first child encounted, point it to the new root
                        *node_ptr = partitionnode_make_ref(new_root);
                    }
                }
            }

            // This ndoe is no longer referencing the old root.
            Py_XDECREF(partitionnode_clear_tag(old_dst));
            break;
        }
        case TYPE_REF: {

            _Py_PARTITIONNODE_t old_dst = *dst;
            // Make dst a reference to src
            if (!src_is_new) {
                // Make dst a reference to src
                *dst = partitionnode_make_ref(src);
                assert(partitionnode_get_tag(*dst) == TYPE_REF);
                assert(partitionnode_clear_tag(*dst) != (_Py_PARTITIONNODE_t)_Py_NULL);
            }
            else {
                // Make dst the src
                *dst = (_Py_PARTITIONNODE_t)src;
            }

            /* Make all child of src be a reference to the parent of dst */

            // Children of dst will have this form
            _Py_PARTITIONNODE_t child_test = partitionnode_make_ref(
                (_Py_PARTITIONNODE_t *)partitionnode_clear_tag((_Py_PARTITIONNODE_t)dst));

            // Search locals for children
            int nlocals = ctx->locals_len;
            for (int i = 0; i < nlocals; i++) {
                _Py_PARTITIONNODE_t *node_ptr = &(ctx->locals[i]);
                if (*node_ptr == child_test) {
                    // Is a child of dst. Point it to the parent of dst
                    *node_ptr = old_dst;
                }
            }

            // Search stack for children
            int nstack = ctx->stack_len;
            for (int i = 0; i < nstack; i++) {
                _Py_PARTITIONNODE_t *node_ptr = &(ctx->stack[i]);
                if (*node_ptr == child_test) {
                    // Is a child of dst. Point it to the parent of dst
                    *node_ptr = old_dst;
                }
            }
            break;
        }
    default:
        Py_UNREACHABLE();
    }
}

#if PARTITION_DEBUG

/**
 * @brief Print the entries in the abstract interpreter context (along with locals).
*/
static void
print_ctx(_Py_UOpsAbstractInterpContext *ctx)
{
    _Py_PARTITIONNODE_t *locals = ctx->locals;
    _Py_PARTITIONNODE_t *stackptr = ctx->stack_pointer;

    int nstack_use = (int)(stackptr - ctx->stack);
    int nstack = ctx->stack_len;
    int nlocals = ctx->locals_len;

    bool is_local = false;
    bool is_stack = false;

    int locals_offset = -1;
    int stack_offset = -1;
    int parent_idx = -1;

    fprintf(stderr, "      Stack: %p: [", ctx->stack);
    for (int i = 0; i < nstack; i++) {
        _Py_PARTITIONNODE_t *node = &ctx->stack[i];
        _Py_PARTITIONNODE_t tag = partitionnode_get_tag(*node);

        _Py_PARTITIONNODE_t *root = partitionnode_get_rootptr(node);

        fprintf(stderr, "%s", i == nstack_use ? "." : " ");

        if (tag == TYPE_REF) {
            _Py_PARTITIONNODE_t *parent = (_Py_PARTITIONNODE_t *)(partitionnode_clear_tag(*node));
            int local_index = (int)(parent - ctx->locals);
            int stack_index = (int)(parent - ctx->stack);
            is_local = local_index >= 0 && local_index < ctx->locals_len;
            is_stack = stack_index >= 0 && stack_index < nstack;
            parent_idx = is_local
                ? local_index
                : is_stack
                ? stack_index
                : -1;
        }


        _Py_PartitionRootNode *ptr = (_Py_PartitionRootNode *)partitionnode_clear_tag(*root);
        fprintf(stderr, "%s:",
            ptr == NULL ? "?" : (ptr->static_or_dynamic == 0 ? "static" : "dynamic"));
        if (ptr != NULL && ptr->static_or_dynamic == 0) {
            PyObject_Print(ptr->const_val, stderr, 0);
        }

        if (tag == TYPE_REF) {
            const char *wher = is_local
                ? "locals"
                : is_stack
                ? "stack"
                : "const";
            fprintf(stderr, "->%s[%d]",
                wher, parent_idx);
        }
        fprintf(stderr, " | ");
    }
    fprintf(stderr, "]\n");

    fprintf(stderr, "      Locals %p: [", locals);
    for (int i = 0; i < nlocals; i++) {
        _Py_PARTITIONNODE_t *node = &ctx->locals[i];
        _Py_PARTITIONNODE_t tag = partitionnode_get_tag(*node);

        _Py_PARTITIONNODE_t *root = partitionnode_get_rootptr(node);

        if (tag == TYPE_REF) {
            _Py_PARTITIONNODE_t *parent = (_Py_PARTITIONNODE_t *)(partitionnode_clear_tag(*node));
            int local_index = (int)(parent - ctx->locals);
            int stack_index = (int)(parent - ctx->stack);
            is_local = local_index >= 0 && local_index < ctx->locals_len;
            is_stack = stack_index >= 0 && stack_index < nstack;
            parent_idx = is_local
                ? local_index
                : is_stack
                ? stack_index
                : -1;
        }

        _Py_PartitionRootNode *ptr = (_Py_PartitionRootNode *)partitionnode_clear_tag(*root);
        fprintf(stderr, "%s:",
            ptr == NULL ? "?" : (ptr->static_or_dynamic == 0 ? "static" : "dynamic"));
        if (ptr != NULL && ptr->static_or_dynamic == 0) {
            PyObject_Print(ptr->const_val, stderr, 0);
        }

        if (tag == TYPE_REF) {
            const char *wher = is_local
                ? "locals"
                : is_stack
                ? "stack"
                : "const";
            fprintf(stderr, "->%s[%d]",
                wher, parent_idx);
        }
        fprintf(stderr, " | ");
    }
    fprintf(stderr, "]\n");
}
#endif

static bool
partitionnode_is_static(_Py_PARTITIONNODE_t *node)
{
    _Py_PARTITIONNODE_t *root = partitionnode_get_rootptr(node);
    _Py_PartitionRootNode *root_obj = (_Py_PartitionRootNode *)partitionnode_clear_tag(*root);
    if (root_obj == _Py_NULL) {
        return false;
    }
    return root_obj->static_or_dynamic == 0;
}

// MUST BE GUARDED BY partitionnode_is_static BEFORE CALLING THIS
static inline PyObject *
get_const(_Py_PARTITIONNODE_t *node)
{
    assert(partitionnode_is_static(node));
    _Py_PARTITIONNODE_t *root = partitionnode_get_rootptr(node);
    _Py_PartitionRootNode *root_obj = (_Py_PartitionRootNode * )partitionnode_clear_tag(*root);
    return root_obj->const_val;
}

// Hardcoded for now, @TODO autogenerate these from the DSL.
static inline bool
op_is_pure(int opcode, int oparg, _Py_PARTITIONNODE_t *locals)
{
    switch (opcode) {
    case LOAD_CONST:
    case _BINARY_OP_MULTIPLY_INT:
    case _BINARY_OP_ADD_INT:
    case _BINARY_OP_SUBTRACT_INT:
    case _GUARD_BOTH_INT:
        return true;
    case LOAD_FAST:
        return partitionnode_is_static(&locals[oparg]) && get_const(&locals[oparg]) != _Py_NULL;
    default:
        return false;
    }
}

// Remove contiguous SAVE_IPs, leaving only the last one before a non-SAVE_IP instruction.
static int
remove_duplicate_save_ips(_PyUOpInstruction *trace, int trace_len)
{
    _PyUOpInstruction *temp_trace = PyMem_New(_PyUOpInstruction, trace_len);
    if (temp_trace == NULL) {
        return trace_len;
    }
    int temp_trace_len = 0;

    _PyUOpInstruction curr;
    for (int i = 0; i < trace_len; i++) {
        curr = trace[i];
        if (i < trace_len && curr.opcode == SAVE_IP && trace[i+1].opcode == SAVE_IP) {
            continue;
        }
        temp_trace[temp_trace_len] = curr;
        temp_trace_len++;
    }
    memcpy(trace, temp_trace, temp_trace_len * sizeof(_PyUOpInstruction));
    PyMem_Free(temp_trace);

#if PARTITION_DEBUG
    fprintf(stderr, "Removed %d SAVE_IPs\n", trace_len - temp_trace_len);
#endif
    return temp_trace_len;
}

/**
 * Fixes all side exits due to jumps. This MUST be called as the last
 * pass over the trace. Otherwise jumps will point to invalid ends.
*/
static int
fix_jump_side_exits(_PyUOpInstruction *trace, int trace_len)
{
    for (int i = 0; i < trace_len; i++) {
        int oparg = trace[i].oparg;
        int opcode = trace[i].opcode;
        switch (opcode) {
        case _POP_JUMP_IF_TRUE:
        case _POP_JUMP_IF_FALSE:
            trace[i].oparg = trace_len - 2;
        }
    }
    return trace_len;
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

int
_Py_uop_analyze_and_optimize(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    int trace_len,
    int curr_stacklen
)
{
#define STACK_LEVEL()     ((int)(*stack_pointer - stack))
#define STACK_SIZE()      (co->co_stacksize)
#define BASIC_STACKADJ(n) (*stack_pointer += n)

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
#define PEEK(idx)              (&((*stack_pointer)[-(idx)]))
#define GETLOCAL(idx)          (&(locals[idx]))

#define PARTITIONNODE_SET(src, dst, flag)        partitionnode_set((src), (dst), (flag))
#define PARTITIONNODE_OVERWRITE(src, dst, flag)  partitionnode_overwrite(ctx, (src), (dst), (flag))
#define MAKE_STATIC_ROOT(val)                    partitionnode_make_root(0, (val))
    _PyUOpInstruction *temp_writebuffer = PyMem_New(_PyUOpInstruction, trace_len);
    if (temp_writebuffer == NULL) {
        return trace_len;
    }

    int buffer_trace_len = 0;

    _Py_UOpsAbstractInterpContext *ctx = _Py_UOpsAbstractInterpContext_New(co->co_stacksize, co->co_nlocals, curr_stacklen);
    if (ctx == NULL) {
        PyMem_Free(temp_writebuffer);
        return trace_len;
    }

    PyObject *co_const_copy = PyList_New(PyTuple_Size(co->co_consts));
    if (co_const_copy == NULL) {
        goto abstract_error;
    }
    // Copy over the co_const tuple
    for (int x = 0; x < PyTuple_GET_SIZE(co->co_consts); x++) {
        PyList_SET_ITEM(co_const_copy, x, Py_NewRef(PyTuple_GET_ITEM(co->co_consts, x)));
    }

    int oparg;
    int opcode;
    bool *stack_virtual_or_real = ctx->stack_virtual_or_real;

    _Py_PARTITIONNODE_t **stack_pointer = &ctx->stack_pointer;
    _Py_PARTITIONNODE_t *stack = ctx->stack;
    _Py_PARTITIONNODE_t *locals = ctx->locals;
    for (int i = 0; i < trace_len; i++) {
        oparg = trace[i].oparg;
        opcode = trace[i].opcode;

        // Partial evaluation - the partition nodes already gave us the static-dynamic variable split.
        // For partial evaluation, we simply need to follow these rules:
        // 1. Operations on dynamic variables need to be emitted.
        //      If an operand was previously partially evaluated and not yet emitted, then emit the residual with a LOAD_CONST.
        // 2. Operations on static variables are a no-op as the abstract interpreter already analyzed their results.

        // For all stack inputs, are their variables static?
        int num_inputs = _PyOpcode_num_popped(opcode, oparg, false);
        int num_dynamic_operands = 0;

        // We need to also check if this operation is "pure". That it can accept
        // constant nodes, output constant nodes, and does not cause any side effects.
        bool should_emit = !op_is_pure(opcode, oparg, locals);

        int virtual_objects = 0;
        assert(num_inputs >= 0);
        for (int x = num_inputs; x > 0; x--) {
            if (!partitionnode_is_static(PEEK(x))) {
                should_emit = true;
                num_dynamic_operands++;
            }
            if (stack_virtual_or_real[STACK_LEVEL() - num_inputs]) {
                virtual_objects++;
            }
        }

        int num_static_operands = num_inputs - num_dynamic_operands;

        assert(num_static_operands >= 0);


        if (should_emit) {
            if (num_static_operands > 0) {
                int real_stack_size = num_dynamic_operands;
                int virtual_stack_size = (int)(ctx->stack_pointer - ctx->stack);
                assert(virtual_stack_size >= real_stack_size);
                for (int x = num_inputs; x > 0; x--) {
                    // Re-materialise all virtual (partially-evaluated) constants
                    if (partitionnode_is_static(PEEK(x)) && stack_virtual_or_real[STACK_LEVEL() - x]) {
                        stack_virtual_or_real[STACK_LEVEL() - x] = false;
                        PyObject *const_val = get_const(PEEK(x));
                        _PyUOpInstruction load_const;
                        load_const.opcode = LOAD_CONST;
                        load_const.oparg = (int)PyList_GET_SIZE(co_const_copy);
                        if (PyList_Append(co_const_copy, const_val) < 0) {
                            goto abstract_error;
                        }

#if PARTITION_DEBUG
                        fprintf(stderr, "Emitting LOAD_CONST\n");
#endif
                        temp_writebuffer[buffer_trace_len] = load_const;
                        buffer_trace_len++;


                        // INSERT to the correct position in the stack
                        int offset_from_target = x - num_dynamic_operands - 1;
                        if (offset_from_target > 0) {
                            _PyUOpInstruction insert;
                            insert.opcode = INSERT;
                            insert.oparg = -offset_from_target;

#if PARTITION_DEBUG
                            fprintf(stderr, "Emitting INSERT %d\n", offset_from_target);
#endif
                            temp_writebuffer[buffer_trace_len] = insert;
                            buffer_trace_len++;
                        }
                        num_dynamic_operands++;
                    }

                }
            }
#if PARTITION_DEBUG
            fprintf(stderr, "Emitting %s\n", (opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[opcode]);
#endif
            temp_writebuffer[buffer_trace_len] = trace[i];
            buffer_trace_len++;
        }
        /*
        * The following are special cased:
        * @TODO: shift these to the DSL
        */

#ifdef PARTITION_DEBUG
#ifdef Py_DEBUG
        fprintf(stderr, "  [-] Type propagating across: %s{%d} : %d\n",
            (opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[opcode],
            opcode, oparg);
#endif
#endif
        switch (opcode) {
#include "abstract_interp_cases.c.h"
        // @TODO convert these to autogenerated using DSL
        case LOAD_FAST:
        case LOAD_FAST_CHECK:
            STACK_GROW(1);
            PARTITIONNODE_OVERWRITE(GETLOCAL(oparg), PEEK(1), false);
            break;
        case LOAD_FAST_AND_CLEAR: {
            STACK_GROW(1);
            PARTITIONNODE_OVERWRITE(GETLOCAL(oparg), PEEK(1), false);
            PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)PARTITIONNODE_NULLROOT, GETLOCAL(oparg), true);
            break;
        }
        case LOAD_CONST: {
            _Py_PARTITIONNODE_t* value = (_Py_PARTITIONNODE_t *)MAKE_STATIC_ROOT(GETITEM(co_const_copy, oparg));
            STACK_GROW(1);
            PARTITIONNODE_OVERWRITE(value, PEEK(1), true);
            break;
        }
        case STORE_FAST:
        case STORE_FAST_MAYBE_NULL: {
            _Py_PARTITIONNODE_t *value = PEEK(1);
            PARTITIONNODE_OVERWRITE(value, GETLOCAL(oparg), false);
            STACK_SHRINK(1);
            break;
        }
        case COPY: {
            _Py_PARTITIONNODE_t *bottom = PEEK(1 + (oparg - 1));
            STACK_GROW(1);
            PARTITIONNODE_OVERWRITE(bottom, PEEK(1), false);
            break;
        }

        // Arithmetic operations

        case _BINARY_OP_MULTIPLY_INT: {
            if (!should_emit) {
                PyObject *right;
                PyObject *left;
                PyObject *res;
                right = get_const(PEEK(1));
                left = get_const(PEEK(2));
                STAT_INC(BINARY_OP, hit);
                res = _PyLong_Multiply((PyLongObject *)left, (PyLongObject *)right);
                if (res == NULL) goto abstract_error;
                STACK_SHRINK(1);
                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)MAKE_STATIC_ROOT(res), PEEK(-(-1)), true);
                break;
            }
            else {
                STACK_SHRINK(1);
                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)PARTITIONNODE_NULLROOT, PEEK(-(-1)), true);
                break;
            }

        }

        case _BINARY_OP_ADD_INT: {
            if (!should_emit) {
                PyObject *right;
                PyObject *left;
                PyObject *res;
                right = get_const(PEEK(1));
                left = get_const(PEEK(2));
                STAT_INC(BINARY_OP, hit);
                res = _PyLong_Add((PyLongObject *)left, (PyLongObject *)right);
                if (res == NULL) goto abstract_error;
                STACK_SHRINK(1);
                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)MAKE_STATIC_ROOT(res), PEEK(-(-1)), true);
                break;
            }
            else {
                STACK_SHRINK(1);
                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)PARTITIONNODE_NULLROOT, PEEK(-(-1)), true);
                break;
            }
        }

        case _BINARY_OP_SUBTRACT_INT: {
            if (!should_emit) {
                PyObject *right;
                PyObject *left;
                PyObject *res;
                right = get_const(PEEK(1));
                left = get_const(PEEK(2));
                STAT_INC(BINARY_OP, hit);
                res = _PyLong_Subtract((PyLongObject *)left, (PyLongObject *)right);
                if (res == NULL) goto abstract_error;
                STACK_SHRINK(1);
                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)MAKE_STATIC_ROOT(res), PEEK(-(-1)), true);
                break;
            }
            else {
                STACK_SHRINK(1);
                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)PARTITIONNODE_NULLROOT, PEEK(-(-1)), true);
                break;
            }
        }
        default:
            fprintf(stderr, "Unknown opcode in abstract interpreter\n");
            Py_UNREACHABLE();
        }

#if PARTITION_DEBUG
        print_ctx(ctx);
#endif

        // Mark all stack outputs as virtual or real
        int stack_outputs = _PyOpcode_num_pushed(opcode, oparg, false);
        for (int y = stack_outputs; y > 0; y--) {
            stack_virtual_or_real[STACK_LEVEL() - y] = !should_emit;
        }
//        if (should_emit) {
//
//            // Emit instruction
//            temp_writebuffer[buffer_trace_len] = trace[i];
//            buffer_trace_len++;
//
//            // Update the real abstract interpreter
//            stack_pointer = ctx->real_stack_pointer;
//            locals = ctx->real_locals;
//            stack = ctx->real_stack;
//
//            /*
//            * The following are special cased:
//            * @TODO: shift these to the DSL
//            */
//            switch (opcode) {
//#include "abstract_interp_cases.c.h"
//                // @TODO convert these to autogenerated using DSL
//            case LOAD_FAST:
//            case LOAD_FAST_CHECK:
//                STACK_GROW(1);
//                PARTITIONNODE_OVERWRITE(GETLOCAL(oparg), PEEK(1), false);
//                break;
//            case LOAD_FAST_AND_CLEAR: {
//                STACK_GROW(1);
//                PARTITIONNODE_OVERWRITE(GETLOCAL(oparg), PEEK(1), false);
//                PARTITIONNODE_OVERWRITE(&PARTITIONNODE_NULLROOT, GETLOCAL(oparg), false);
//                break;
//            }
//            case LOAD_CONST: {
//                _Py_PARTITIONNODE_t value = MAKE_STATIC_ROOT(GETITEM(co->co_consts, oparg));
//                STACK_GROW(1);
//                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)value, PEEK(1), false);
//                break;
//            }
//            case STORE_FAST:
//            case STORE_FAST_MAYBE_NULL: {
//                _Py_PARTITIONNODE_t *value = PEEK(1);
//                PARTITIONNODE_OVERWRITE(value, GETLOCAL(oparg), false);
//                STACK_SHRINK(1);
//                break;
//            }
//            case COPY: {
//                _Py_PARTITIONNODE_t *bottom = PEEK(1 + (oparg - 1));
//                STACK_GROW(1);
//                PARTITIONNODE_SET(bottom, PEEK(1), false);
//                break;
//            }
//
//            case _BINARY_OP_MULTIPLY_INT: {
//                STACK_SHRINK(1);
//                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)PARTITIONNODE_NULLROOT, PEEK(-(-1)), true);
//                break;
//
//            }
//
//            case _BINARY_OP_ADD_INT: {
//                STACK_SHRINK(1);
//                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)PARTITIONNODE_NULLROOT, PEEK(-(-1)), true);
//                break;
//            }
//
//            case _BINARY_OP_SUBTRACT_INT: {
//                STACK_SHRINK(1);
//                PARTITIONNODE_OVERWRITE((_Py_PARTITIONNODE_t *)PARTITIONNODE_NULLROOT, PEEK(-(-1)), true);
//                break;
//            }
//            default:
//                fprintf(stderr, "Unknown opcode in abstract interpreter\n");
//                Py_UNREACHABLE();
//            }
//
//            ctx->real_stack_pointer = stack_pointer;
//        }
    }
    assert(STACK_SIZE() >= 0);
    assert(buffer_trace_len <= trace_len);

    buffer_trace_len = remove_duplicate_save_ips(temp_writebuffer, buffer_trace_len);
    buffer_trace_len = fix_jump_side_exits(temp_writebuffer, buffer_trace_len);

#if PARTITION_DEBUG
    if (buffer_trace_len < trace_len) {
        fprintf(stderr, "Shortened trace by %d instructions\n", trace_len - buffer_trace_len);
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
    memcpy(trace, temp_writebuffer, buffer_trace_len * sizeof(_PyUOpInstruction));
    PyMem_Free(temp_writebuffer);
    return buffer_trace_len;

abstract_error:
    Py_XDECREF(co_const_copy);
    Py_DECREF(ctx);
    assert(PyErr_Occurred());
    PyErr_Clear();
    return trace_len;
}
