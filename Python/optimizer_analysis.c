#include "Python.h"
#include "opcode.h"
#include "pycore_interp.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_uops.h"
#include "pycore_long.h"
#include "cpython/optimizer.h"
#include "pycore_optimizer.h"
#include "pycore_object.h"
#include "pycore_dict.h"

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

static bool
_PyOpcode_isimmutable(int opcode)
{
    // TODO subscr tuple is immutable
    return false;
}

static bool
_PyOpcode_isterminal(int opcode)
{
    return (opcode == LOAD_FAST ||
            opcode == LOAD_FAST_CHECK ||
            opcode == LOAD_FAST_AND_CLEAR ||
            opcode == INIT_FAST ||
            opcode == LOAD_CONST);
}


typedef struct _Py_UOpsSymbolicExpression {
    PyObject_VAR_HEAD
    Py_ssize_t idx;
    // Note: separated from refcnt so we don't have to deal with counting
    int usage_count;
    int opcode;
    int oparg;
    PyObject *const_val;
    Py_hash_t cached_hash;
    // The store where this expression was first created.
    // This matters for anything that isn't immutable
    // void* because otherwise need a forward decl of _Py_UOpsAbstractStore.
    void *originating_store;
    struct _Py_UOpsSymbolicExpression *operands[1];
} _Py_UOpsSymbolicExpression;

static void
sym_dealloc(PyObject *o)
{
    _Py_UOpsSymbolicExpression *self = (_Py_UOpsSymbolicExpression *)o;
    // Note: we are not decerfing the symbolic expressions because we only hold
    // a borrowed ref to them. The symexprs are kept alive by the global table.
    Py_CLEAR(self->const_val);
    Py_TYPE(o)->tp_free(o);
}

static Py_hash_t
sym_hash(PyObject *o)
{
    _Py_UOpsSymbolicExpression *self = (_Py_UOpsSymbolicExpression *)o;
    if (self->cached_hash != -1) {
        return self->cached_hash;
    }
    // TODO a faster hash function that doesn't allocate?
    PyObject *temp = PyTuple_New(Py_SIZE(o) + 2);
    if (temp == NULL) {
        return -1;
    }
    Py_ssize_t len = Py_SIZE(o);
    for (Py_ssize_t i = 0; i < len; i++) {
        PyTuple_SET_ITEM(temp, i, Py_NewRef(self->operands[i]));
    }
    PyObject *opcode = PyLong_FromLong(self->opcode);
    if (opcode == NULL) {
        Py_DECREF(temp);
        return -1;
    }
    PyObject *oparg = PyLong_FromLong(self->oparg);
    if (oparg == NULL) {
        Py_DECREF(temp);
        Py_DECREF(opcode);
        return -1;
    }
    PyTuple_SET_ITEM(temp, Py_SIZE(o), opcode);
    PyTuple_SET_ITEM(temp, Py_SIZE(o) + 1, oparg);
    Py_hash_t hash = PyObject_Hash(temp);
    Py_DECREF(temp);
    self->cached_hash = hash;
    return hash;
}

static PyObject *
sym_richcompare(PyObject *o1, PyObject *o2, int op)
{
    assert(op == Py_EQ);
    if (Py_TYPE(o1) != Py_TYPE(o2)) {
        Py_RETURN_FALSE;
    }
    _Py_UOpsSymbolicExpression *self = (_Py_UOpsSymbolicExpression *)o1;
    _Py_UOpsSymbolicExpression *other = (_Py_UOpsSymbolicExpression *)o2;
    if (_PyOpcode_isterminal(self->opcode) != _PyOpcode_isterminal(other->opcode)) {
        Py_RETURN_FALSE;
    }

    if (_PyOpcode_isimmutable(self->opcode) != _PyOpcode_isimmutable(other->opcode)) {
        Py_RETURN_FALSE;
    }

    if (!_PyOpcode_isimmutable(self->opcode)) {
        assert(self->originating_store != NULL);
        assert(other->originating_store != NULL);
        if (self->originating_store != other->originating_store) {
            Py_RETURN_FALSE;
        }
    }

    // Terminal ops are kinda like special sentinels.
    // They are always considered unique, except for constant values
    // which can be repeated
    if (_PyOpcode_isterminal(self->opcode) && _PyOpcode_isterminal(other->opcode)) {
        if (self->const_val && other->const_val) {
            return PyObject_RichCompare(self->const_val, other->const_val, Py_EQ);
        } else {
            // Note: even if two LOAD_FAST have the same opcode and oparg,
            // They are not the same because we are constructing a new terminal.
            // All terminals except constants are unique.
            return self->idx == other->idx ? Py_True : Py_False;
        }
    }
    // Two symbolic expressions are the same iff
    // 1. Their opcodes are equal.
    // 2. If they are mutable, they must be from the same store.
    // 3. Their constituent subexpressions are equal.
    // For 2. we use a quick hack, and compare by their global ID. Note
    // that this ID is only populated later on, after we have determined the
    // expression is not a duplicate. The invariant that must hold is that
    // the subexpressions have already been checked for duplicates and their
    // id populated. This should always hold.
    // The symexpr's own id does not have to be populated yet.
    // Note: WE DO NOT COMPARE THEIR CONST_VAL, BECAUSE THAT CAN BE POPULATED
    // LATER.
    if (self->opcode != other->opcode) {
        Py_RETURN_FALSE;
    }
    Py_ssize_t self_len = Py_SIZE(self);
    Py_ssize_t other_len = Py_SIZE(other);
    if (self_len != other_len) {
        Py_RETURN_FALSE;
    }
    for (Py_ssize_t i = 0; i < self_len; i++) {
        _Py_UOpsSymbolicExpression *a = self->operands[i];
        _Py_UOpsSymbolicExpression *b = other->operands[i];
        assert(a != NULL);
        assert(b != NULL);
        assert(a->idx != -1);
        assert(b->idx != -1);
        if (a->idx != b->idx) {
            Py_RETURN_FALSE;
        }
    }

    Py_RETURN_TRUE;
}

static PyTypeObject _Py_UOpsSymbolicExpression_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops symbolic expression",
    .tp_basicsize = sizeof(_Py_UOpsSymbolicExpression) - sizeof(_Py_UOpsSymbolicExpression *),
    .tp_itemsize = sizeof(_Py_UOpsSymbolicExpression *),
    .tp_hash = sym_hash,
    .tp_richcompare = sym_richcompare,
    .tp_dealloc = sym_dealloc,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};


// Snapshot of _Py_UOpsAbstractInterpContext locals BEFORE a region.
typedef struct _Py_UOpsAbstractStore {
    PyObject_VAR_HEAD
    // Store - 1 inst - 0, allows for tagged unions
    char store_or_inst;
    // The next store/impure instruction in the trace
    void *next;

    // The preceding PyListObject of (hoisted guards)
    // Consists of _Py_UOpsSymbolicExpression
    PyObject *hoisted_guards;

    // The following are abstract stack and locals.
    // points to one element after the abstract stack
    _Py_UOpsSymbolicExpression **stack_pointer;
    _Py_UOpsSymbolicExpression **stack;
    _Py_UOpsSymbolicExpression **locals;
    _Py_UOpsSymbolicExpression *registers[REGISTERS_COUNT];
    _Py_UOpsSymbolicExpression *locals_with_stack[1];
} _Py_UOpsAbstractStore;

typedef struct _Py_UopImpureInstruction {
    PyObject_VAR_HEAD
    // Store - 1 inst - 0, allows for tagged unions
    char store_or_inst;
    // The next store/impure instruction in the trace
    void *next;
    _Py_UOpsSymbolicExpression *inst;
} _Py_UopImpureInstruction;

static void
abstractstore_dealloc(PyObject *o)
{
    _Py_UOpsAbstractStore *self = (_Py_UOpsAbstractStore *)o;
    Py_XDECREF(self->next);
    Py_DECREF(self->hoisted_guards);
    // Py_ssize_t len = Py_SIZE(self);
    // No need dealloc locals and stack. We only hold weak references to them.
//    for (Py_ssize_t i = 0; i < len; i++) {
//        Py_XDECREF(self->locals_with_stack[i]);
//    }
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

static PyTypeObject _PyUOpsImpureInstruction_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "a single impure instruction",
    .tp_basicsize = sizeof(_Py_UopImpureInstruction),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)PyObject_Del,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

// Tier 2 types meta interpreter
typedef struct _Py_UOpsAbstractInterpContext {
    PyObject_VAR_HEAD
    // Maps a sym_expr to itself, so we can do O(1) lookups of it later
    PyDictObject *sym_exprs_to_sym_exprs;
    // Current ID to assign a new (non-duplicate) sym_expr
    Py_ssize_t sym_curr_id;

    // Max stacklen
    int stack_len;
    int locals_len;
    int curr_stacklen;
    // Actual stack and locals are stored in the current abstract store
    _Py_UOpsAbstractStore *curr_store;
} _Py_UOpsAbstractInterpContext;

static void
abstractinterp_dealloc(PyObject *o)
{
    _Py_UOpsAbstractInterpContext *self = (_Py_UOpsAbstractInterpContext *)o;
    Py_DECREF(self->sym_exprs_to_sym_exprs);
    Py_XDECREF(self->curr_store);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject _Py_UOpsAbstractInterpContext_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops abstract interpreter's context",
    .tp_basicsize = sizeof(_Py_UOpsAbstractInterpContext),
    .tp_itemsize = 0,
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

    self->sym_exprs_to_sym_exprs = (PyDictObject *)PyDict_New();
    if (self->sym_exprs_to_sym_exprs == NULL) {
        Py_DECREF(self);
        return NULL;
    }

    self->stack_len = stack_len;
    self->locals_len = locals_len;
    self->curr_stacklen = curr_stacklen;

    return self;
}

static _Py_UOpsSymbolicExpression *
check_uops_already_exists(_Py_UOpsAbstractInterpContext *ctx, _Py_UOpsSymbolicExpression *self)
{
    assert(ctx->sym_exprs_to_sym_exprs);

    // Check if this sym expr already exists
    PyObject *res = PyDict_GetItemWithError(
        (PyObject *)ctx->sym_exprs_to_sym_exprs, (PyObject *)self);
    // No entry, return ourselves.
    if (res == NULL) {
        if (PyErr_Occurred()) {
            Py_DECREF(self);
            return NULL;
        }
        // If not, add it to our sym expression global book
        int res = PyDict_SetItem((PyObject *)ctx->sym_exprs_to_sym_exprs,
                                 (PyObject *)self, (PyObject *)self);
        if (res < 0) {
            Py_DECREF(self);
            return NULL;
        }
        // Assign an ID
        self->idx = ctx->sym_curr_id;
        ctx->sym_curr_id++;
        return self;
    }
    // There's an entry. Reuse that instead
    Py_DECREF(self);
    return (_Py_UOpsSymbolicExpression *)res;
}

// Steals a reference to const_val
static _Py_UOpsSymbolicExpression*
_Py_UOpsSymbolicExpression_New(_Py_UOpsAbstractInterpContext *ctx,
                               int opcode, int oparg,
                               PyObject *const_val, int num_subexprs, ...)
{
    _Py_UOpsSymbolicExpression *self = PyObject_NewVar(_Py_UOpsSymbolicExpression,
                                                          &_Py_UOpsSymbolicExpression_Type,
                                                          num_subexprs);
    if (self == NULL) {
        return NULL;
    }


    self->idx = -1;
    self->cached_hash = -1;
    self->usage_count = 0;
    self->const_val = NULL;
    self->opcode = opcode;
    self->oparg = oparg;
    self->const_val = const_val;
    // Borrowed ref. We don't want to have to make our type GC as this will
    // slow down things. This is guaranteed to be safe within our usage.
    self->originating_store = ctx->curr_store;

    assert(Py_SIZE(self) >= num_subexprs);
    // Setup
    va_list curr;

    va_start(curr, num_subexprs);

    for (int i = 0; i < num_subexprs; i++) {
        // Note: no incref here. symexprs are kept alive by the global expression
        // table.
        // We intentionally don't want to hold a reference to it so we don't
        // need GC.
        self->operands[i] = va_arg(curr, _Py_UOpsSymbolicExpression *);
        assert(self->operands[i]);
        self->operands[i]->usage_count++;
    }

    va_end(curr);

    return check_uops_already_exists(ctx, self);
}

static _Py_UOpsSymbolicExpression*
_Py_UOpsSymbolicExpression_NewFromArray(_Py_UOpsAbstractInterpContext *ctx,
                               int opcode, int oparg, int num_subexprs,
                               _Py_UOpsSymbolicExpression **arr_start)
{
    _Py_UOpsSymbolicExpression *self = PyObject_NewVar(_Py_UOpsSymbolicExpression,
                                                          &_Py_UOpsSymbolicExpression_Type,
                                                          num_subexprs);
    if (self == NULL) {
        return NULL;
    }


    self->idx = -1;
    self->cached_hash = -1;
    self->usage_count = 0;
    self->const_val = NULL;
    self->opcode = opcode;
    self->oparg = oparg;

    // Setup
    for (int i = 0; i < num_subexprs; i++) {
        self->operands[i] = arr_start[i];
        self->operands[i]->usage_count++;
    }


    return check_uops_already_exists(ctx, self);
}


static inline _Py_UOpsSymbolicExpression*
sym_init_var(_Py_UOpsAbstractInterpContext *ctx, int locals_idx)
{
    return _Py_UOpsSymbolicExpression_New(ctx,
                                          INIT_FAST, locals_idx,
                                          NULL, 0);
}

static inline _Py_UOpsSymbolicExpression*
sym_init_const(_Py_UOpsAbstractInterpContext *ctx, PyObject *const_val, int const_idx)
{
    _Py_UOpsSymbolicExpression *temp = _Py_UOpsSymbolicExpression_New(
        ctx,
        LOAD_CONST,
        const_idx,
        const_val,
        0
    );
    if (temp == NULL) {
        return NULL;
    }
    return temp;
}

static inline _Py_UOpsSymbolicExpression*
sym_init_guard(_Py_UOpsAbstractInterpContext *ctx, int opcode, int oparg, int num_stack_inputs)
{
    return
        _Py_UOpsSymbolicExpression_NewFromArray(
            ctx,
            opcode,
            oparg,
            num_stack_inputs,
            &ctx->curr_store->stack_pointer[-(num_stack_inputs + 1)]
        );
}

static _Py_UOpsAbstractStore*
_Py_UOpsAsbstractStore_New(_Py_UOpsAbstractInterpContext *ctx)
{
    _Py_UOpsAbstractStore *self = PyObject_NewVar(_Py_UOpsAbstractStore,
                                                  &_Py_UOpsAbstractStore_Type,
                                                  ctx->locals_len + ctx->stack_len);
    if (self == NULL) {
        return NULL;
    }
    self->store_or_inst = 1;
    self->next = NULL;

    for (int i = 0; i < REGISTERS_COUNT; i++) {
        self->registers[i] = NULL;
    }

    // Setup

    // Initialize the hoisted guards
    self->hoisted_guards = PyList_New();
    if (self->hoisted_guards == NULL) {
        return NULL;
    }

    self->locals = self->locals_with_stack;
    self->stack = self->locals_with_stack + ctx->locals_len;
    self->stack_pointer = self->stack + ctx->curr_stacklen;

    // Null out everything first
    for (int i = 0; i < ctx->locals_len + ctx->stack_len; i++) {
        self->locals_with_stack[i] = NULL;
    }
    // Initialize with the initial state of all local variables
    for (int i = 0; i < ctx->locals_len; i++) {
        _Py_UOpsSymbolicExpression *local = sym_init_var(ctx, i);
        if (local == NULL) {
            goto error;
        }
        self->locals[i] = local;
    }

    // Initialize the stack as well
    for (int i = 0; i < ctx->curr_stacklen; i++) {
        _Py_UOpsSymbolicExpression *stackvar = sym_init_var(ctx, i);
        if (stackvar == NULL) {
            goto error;
        }
        self->stack[i] = stackvar;
    }

    return self;

error:
    Py_DECREF(self);
    return NULL;
}

static struct _Py_UopImpureInstruction*
_Py_UOpsImupreInstruction_New(_Py_UOpsAbstractInterpContext *ctx)
{
    _Py_UOpsAbstractStore *self = PyObject_NewVar(_Py_UopImpureInstruction,
                                                  &_PyUOpsImpureInstruction_Type,
                                                  0);
    if (self == NULL) {
        return NULL;
    }
    self->store_or_inst = 0;
    self->next = NULL;

    return self;
}

static inline bool
op_is_jump(int opcode)
{
    return (opcode == _POP_JUMP_IF_FALSE || opcode == _POP_JUMP_IF_TRUE);
}

static inline bool
is_const(_Py_UOpsSymbolicExpression *expr)
{
    return expr->const_val != NULL;
}

static inline PyObject *
get_const(_Py_UOpsSymbolicExpression *expr)
{
    return Py_NewRef(expr->const_val);
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

// Remove contiguous SET_IPs, leaving only the last one before a non-SET_IP instruction.
static int
remove_duplicate_set_ips(_PyUOpInstruction *trace, int trace_len)
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
        if (curr.opcode == _SET_IP && trace[i+1].opcode == _SET_IP) {
            continue;
        }
        trace[new_temp_len] = curr;
        new_temp_len++;
    }


    DPRINTF(2, "Removed %d SET_IPs\n", trace_len - new_temp_len);

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

// 1 on success
// 0 on failure
// -1 on exception
static int
try_hoist_guard(_Py_UOpsAbstractInterpContext *ctx,
                _PyUOpInstruction *curr,
                _Py_UOpsSymbolicExpression **prev_store_locals)
{
    bool can_hoist = true;

    // Try hoisting the guard.
    // A guard can be hoisted IFF all its inputs are in the initial
    // locals state.
    // Global state guards can't be hoisted.
    // Assumption: state guards are those that have no stack effect.

    int num_stack_inputs = _PyOpcode_num_popped(curr->opcode, curr->opcode, false);
    for (int i = 0; i < num_stack_inputs; i++) {
        bool input_present = false;
        _Py_UOpsSymbolicExpression *input = ctx->curr_store->stack_pointer[-(i + 1)];
        for (int x = 0; x < ctx->locals_len; x++) {
            if (input == prev_store_locals[x]) {
                input_present = true;
                break;
            }
        }
        if (!input_present) {
            can_hoist = false;
            break;
        }
    }
    // Get out of the pure region formation if cannot hoist.
    if (!can_hoist) {
        return 0;
    }
    // Yay can hoist
    _Py_UOpsSymbolicExpression *guard =
        sym_init_guard(ctx, curr->opcode, curr->oparg, num_stack_inputs);
    if (guard == NULL) {
        return -1;
    }
    int res = PyList_Append(ctx->curr_store->hoisted_guards, (PyObject *)guard);
    Py_DECREF(guard);
    return res < 0 ? -1 : 1;
}

#define DECREF_INPUTS_AND_REUSE_FLOAT(left, right, dval, result) \
do { \
    if (Py_REFCNT(left) == 1) { \
        ((PyFloatObject *)left)->ob_fval = (dval); \
        _Py_DECREF_SPECIALIZED(right, _PyFloat_ExactDealloc);\
        result = (left); \
    } \
    else if (Py_REFCNT(right) == 1)  {\
        ((PyFloatObject *)right)->ob_fval = (dval); \
        _Py_DECREF_NO_DEALLOC(left); \
        result = (right); \
    }\
    else { \
        result = PyFloat_FromDouble(dval); \
        if ((result) == NULL) goto error; \
        _Py_DECREF_NO_DEALLOC(left); \
        _Py_DECREF_NO_DEALLOC(right); \
    } \
} while (0)

#ifndef Py_DEBUG
#define GETITEM(v, i) PyTuple_GET_ITEM((v), (i))
#else
static inline PyObject *
GETITEM(PyObject *v, Py_ssize_t i) {
    assert(PyTuple_CheckExact(v));
    assert(i >= 0);
    assert(i < PyTuple_GET_SIZE(v));
    return PyTuple_GET_ITEM(v, i);
}
#endif

static int
uop_abstract_interpret_single_inst(
    PyCodeObject *co,
    _PyUOpInstruction *inst,
    _Py_UOpsAbstractInterpContext *ctx,
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

#define STACK_LEVEL()     ((int)(stack_pointer - ctx->curr_store->stack))
#define STACK_SIZE()      (co->co_stacksize)
#define BASIC_STACKADJ(n) (stack_pointer += n)

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
#define PEEK(idx)              (((stack_pointer)[-(idx)]))
#define GETLOCAL(idx)          ((ctx->curr_store->locals[idx]))

#define STAT_INC(opname, name) ((void)0)
    int oparg = inst->oparg;
    int opcode = inst->opcode;
    uint64_t operand = inst->operand;

    _Py_UOpsSymbolicExpression **stack_pointer = ctx->curr_store->stack_pointer;

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
        // Note: LOAD_FAST_CHECK is not pure!!!
        case LOAD_FAST_CHECK:
            STACK_GROW(1);
            PEEK(1) = GETLOCAL(oparg);
            // Value might be uninitialized, and might error.
            if(PEEK(1) == NULL || PEEK(1)->opcode == INIT_FAST) {
                goto error;
            }
            break;
        case LOAD_FAST:
            STACK_GROW(1);
            // Guaranteed by the CPython bytecode compiler to not be uninitialized
            // replace with LOAD_FAST
            if(GETLOCAL(oparg)->opcode == INIT_FAST) {
                PEEK(1) = _Py_UOpsSymbolicExpression_New(ctx, LOAD_FAST, oparg, NULL, 1, GETLOCAL(oparg));
            }
            else {
                PEEK(1) = GETLOCAL(oparg);
            }
            assert(PEEK(1));

            break;
        case LOAD_FAST_AND_CLEAR: {
            STACK_GROW(1);
            PEEK(1) = GETLOCAL(oparg);
            GETLOCAL(oparg) = NULL;
            break;
        }
        case LOAD_CONST: {
            // TODO, keep a dictionary mapping constant values to their unique symbolic expression
            STACK_GROW(1);
            PEEK(1) = (_Py_UOpsSymbolicExpression *)GETITEM(co->co_consts, oparg);
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

        // TODO SWAP
        default:
            DPRINTF(1, "Unknown opcode in abstract interpreter\n");
            Py_UNREACHABLE();
    }
    fprintf(stderr, "stack_pointer %p\n", stack_pointer);
    ctx->curr_store->stack_pointer = stack_pointer;
    ctx->curr_stacklen = STACK_LEVEL();
    assert(STACK_LEVEL() >= 0);


    return 0;

pop_4_error:
pop_3_error:
pop_2_error:
pop_1_error:
deoptimize:
error:
    DPRINTF(1, "Encountered error in abstract interpreter\n");
    return -1;
}

static _Py_UOpsAbstractStore *
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

    _Py_UOpsAbstractInterpContext *ctx = NULL;
    _Py_UOpsAbstractStore *store = NULL;
    _Py_UOpsAbstractStore *first_store = NULL;
    _Py_UOpsSymbolicExpression **prev_store_locals = NULL;
    // Just hold onto the first locals because we need to free them.
    _Py_UOpsSymbolicExpression **first_temp_local_store = NULL;

    ctx = _Py_UOpsAbstractInterpContext_New(
        store, co->co_stacksize, co->co_nlocals, curr_stacklen);
    if (ctx == NULL) {
        goto error;
    }

    store = first_store = _Py_UOpsAsbstractStore_New(ctx);
    if (store == NULL) {
        goto error;
    }
    ctx->curr_store = store;
    ctx->sym_curr_id = 0;

    first_temp_local_store = prev_store_locals =
        PyMem_New(_Py_UOpsSymbolicExpression *, ctx->locals_len);
    if (prev_store_locals == NULL) {
        return NULL;
    }
    // Copy over the current locals
    memcpy(prev_store_locals, first_store->locals, ctx->locals_len);


    _PyUOpInstruction *curr = trace;
    _PyUOpInstruction *end = trace + trace_len;

    while (curr < end) {

        DPRINTF(3, "starting pure region\n")

        // Form pure regions
        while(_PyOpcode_ispure(curr->opcode) || _PyOpcode_isguard(curr->opcode)) {

            int err = uop_abstract_interpret_single_inst(
                co, curr, ctx,
                jump_id_to_instruction, max_jump_id
            );
            if (err < 0) {
                goto error;
            }

            if (curr->opcode == _EXIT_TRACE) {
                break;
            }

            curr++;
        }


        if (_PyOpcode_isguard(curr->opcode)) {
            int res = try_hoist_guard(ctx, curr, prev_store_locals);
            if (res < 0) {
                goto error;
            }
            // Cannot hoist the guard, break out of pure region formation.
            if (res == 0) {
                break;
            }
        }

        prev_store_locals = store->locals;

        // End of a pure region, create a new abstract store
        if (curr->opcode != _EXIT_TRACE) {
            _Py_UOpsAbstractStore *temp = store;
            store = _Py_UOpsAsbstractStore_New(ctx);
            if (store == NULL) {
                goto error;
            }
            // Transfer the reference over (note: No incref!)
            temp->next = store;
            ctx->curr_store = temp;
            store = temp;
        }

        DPRINTF(3, "starting impure region\n")
        // Form impure region
        if(!_PyOpcode_ispure(curr->opcode)) {


            int num_stack_inputs = _PyOpcode_num_popped(curr->opcode, curr->oparg, false);
            _Py_UOpsAbstractStore *temp = store;
            store = (_Py_UOpsAbstractStore *)
                _Py_UOpsSymbolicExpression_NewFromArray(
                ctx,
                curr->opcode,
                curr->oparg,
                num_stack_inputs,
                &ctx->curr_store->stack_pointer[-(num_stack_inputs + 1)]
            );
            if (store == NULL) {
                goto error;
            }
            // Transfer the reference over (note: No incref!)
            temp->next = store;
            ctx->curr_store = temp;
            store = temp;

            curr++;

            // End of an impure instruction, create a new abstract store for
            // the next pure region.
            // TODO we can do some memory optimization here to not use abstract
            // stores each time, since that's quite overkill.
            if (curr->opcode != _EXIT_TRACE) {
                _Py_UOpsAbstractStore *temp = store;
                store = _Py_UOpsAsbstractStore_New(ctx);
                if (store == NULL) {
                    goto error;
                }
                // Transfer the reference over (note: No incref!)
                temp->next = store;
                ctx->curr_store = temp;
                store = temp;
            };
        }

    }

    Py_DECREF(ctx);
    PyMem_Free(first_temp_local_store);
    return first_store;

error:
    if(PyErr_Occurred()) {
        PyErr_Clear();
    }
    Py_DECREF(ctx);
    PyMem_Free(first_temp_local_store);
    return NULL;
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

    memcpy(temp_writebuffer, trace,  sizeof(_PyUOpInstruction) * original_trace_len);

    int max_jump_id = 0;

    // Pass: Jump target calculation and setup (preparation for relocation)
    jump_id_to_instruction = number_jumps_and_targets(temp_writebuffer, trace_len, &max_jump_id);
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

    // Pass: Remove duplicate SET_IP
    trace_len = remove_duplicate_set_ips(temp_writebuffer, trace_len);

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
