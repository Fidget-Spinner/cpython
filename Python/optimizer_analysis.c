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

#ifdef Py_DEBUG
    #define DPRINTF(level, ...) \
    if (lltrace >= (level)) { printf(__VA_ARGS__); }
#else
    #define DPRINTF(level, ...)
#endif

static inline bool
_PyOpcode_isimmutable(uint32_t opcode)
{
    // TODO subscr tuple is immutable
    switch (opcode) {
        case PUSH_NULL: true;
    }
    return false;
}

static inline bool
_PyOpcode_isterminal(uint32_t opcode)
{
    return (opcode == LOAD_FAST ||
            opcode == LOAD_FAST_CHECK ||
            opcode == LOAD_FAST_AND_CLEAR ||
            opcode == INIT_FAST ||
            opcode == LOAD_CONST || opcode == CACHE);
}

static inline bool
_PyOpcode_isunknown(uint32_t opcode)
{
    return (opcode == CACHE);
}

static inline bool
is_bookkeeping_opcode(int opcode) {
    return (opcode == _SET_IP || opcode == _CHECK_VALIDITY);
}

// These opcodes just adjust the stack.
static inline bool
is_dataonly_opcode(int opcode) {
    return (opcode == POP_TOP);
}

typedef enum {
    // Types with aux
    GUARD_KEYS_VERSION_TYPE = 0,
    GUARD_TYPE_VERSION_TYPE = 1,

    // Types without aux
    PYINT_TYPE = 2,
    PYFLOAT_TYPE = 3,
    PYUNICODE_TYPE = 4,
    NULL_TYPE = 5,
    PYMETHOD_TYPE = 6,
    GUARD_DORV_VALUES_TYPE = 7,
    GUARD_DORV_VALUES_INST_ATTR_FROM_DICT_TYPE = 8,


    // GUARD_GLOBALS_VERSION_TYPE, / Environment check
    // GUARD_BUILTINS_VERSION_TYPE, // Environment check
    // CHECK_CALL_BOUND_METHOD_EXACT_ARGS_TYPE, // idk how to deal with this, requires stack check
    // CHECK_PEP_523_TYPE, // Environment check
    // CHECK_FUNCTION_EXACT_ARGS_TYPE, // idk how to deal with this, requires stack check
    // CHECK_STACK_SPACE_TYPE // Environment check
    INVALID_TYPE = -1
} _Py_UOpsSymExprTypeEnum;

#define MAX_TYPE_WITH_AUX 1
typedef struct {
    // bitmask of types
    uint32_t types;
    // auxiliary data for the types
    uint32_t aux[MAX_TYPE_WITH_AUX + 1];
} _Py_UOpsSymType;




typedef struct _Py_UOpsSymbolicExpression {
    PyObject_VAR_HEAD
    Py_ssize_t idx;
    // Note: separated from refcnt so we don't have to deal with counting
    // How many times this is nested as a subexpression of another
    // expression. Used for CSE.
    int usage_count;
    // Counts how many symbolic expressions nest this one
    // (ie points to this one). Slightly different from usage count
    // as usage count can extend to expressions outside this store as well.
    // In most cases, this is just usage_count - 1.
    int in_degree;
    // How many of this expression we have emitted so far. Only used for CSE.
    int emitted_count;
    _PyUOpInstruction inst;

    // Only populated by guards
    uint64_t operand;

    // Type of the symbolic expression
    _Py_UOpsSymType sym_type;
    PyObject *const_val;
    Py_hash_t cached_hash;
    // The store where this expression was first created.
    // This matters for anything that isn't immutable
    // void* because otherwise need a forward decl of _Py_UOpsPureStore.
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
    PyObject *opcode = PyLong_FromLong(self->inst.opcode);
    if (opcode == NULL) {
        Py_DECREF(temp);
        return -1;
    }
    PyObject *oparg = PyLong_FromLong(self->inst.oparg);
    if (oparg == NULL) {
        Py_DECREF(temp);
        Py_DECREF(opcode);
        return -1;
    }
    // Note: DO NOT add target here, because we rearrange exits anyways.
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

    int self_opcode = self->inst.opcode;
    int other_opcode = other->inst.opcode;
    if (_PyOpcode_isterminal(self_opcode) != _PyOpcode_isterminal(other_opcode)) {
        Py_RETURN_FALSE;
    }

    if (_PyOpcode_isimmutable(self_opcode) != _PyOpcode_isimmutable(other_opcode)) {
        Py_RETURN_FALSE;
    }

    if (!_PyOpcode_isimmutable(self_opcode)) {
        assert(self->originating_store != NULL);
        assert(other->originating_store != NULL);
        if (self->originating_store != other->originating_store) {
            Py_RETURN_FALSE;
        }
    }

    // Terminal ops are kinda like special sentinels.
    // They are always considered unique, except for constant values
    // which can be repeated
    if (_PyOpcode_isterminal(self_opcode) && _PyOpcode_isterminal(other_opcode)) {
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
    if ((self_opcode != other_opcode)
        || (self->inst.oparg != other->inst.oparg)
        // NOTE: TARGET NOT HERE BECAUSE WE RERRANGE CODE ANYWAYS
        || (self->inst.operand != other->inst.operand)) {
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

#define INSTRUCTION_STORE_HEAD \
    /* pure - 1 impure - 0, allows for tagged unions */ \
    char pure_or_impure; \
    /* The next store/impure instruction in the trace */ \
    void *next;

// Snapshot of _Py_UOpsAbstractInterpContext locals BEFORE a region.
typedef struct _Py_UOpsPureStore {
    PyObject_VAR_HEAD
    INSTRUCTION_STORE_HEAD
    // The preceding PyListObject of (hoisted guards)
    // Consists of _Py_UOpsSymbolicExpression
    PyObject *hoisted_guards;

    // Initial locals state. Needed to reconstruct hoisted guards later.
    _Py_UOpsSymbolicExpression **initial_locals;
    // Initial stack state, needed to calculate the diff to emit.
    _Py_UOpsSymbolicExpression **initial_stack;

    // The following are abstract stack and locals.
    // points to one element after the abstract stack
    _Py_UOpsSymbolicExpression **stack_pointer;
    _Py_UOpsSymbolicExpression **stack;
    _Py_UOpsSymbolicExpression **locals;
    _Py_UOpsSymbolicExpression *locals_with_stack[1];
} _Py_UOpsPureStore;

typedef struct _Py_UOpsImpureStore {
    PyObject_VAR_HEAD
    INSTRUCTION_STORE_HEAD
    // A contiguous block of impure instructions
    _PyUOpInstruction *start;
    _PyUOpInstruction *end; // (non-inclusive)
} _Py_UOpsImpureStore;

typedef struct _Py_UOpsStoreUnion {
    PyObject_VAR_HEAD
    INSTRUCTION_STORE_HEAD
} _Py_UOpsStoreUnion;

static void
purestore_dealloc(PyObject *o)
{
    _Py_UOpsPureStore *self = (_Py_UOpsPureStore *)o;
    Py_XDECREF(self->next);
    Py_DECREF(self->hoisted_guards);
    // No need dealloc locals and stack. We only hold weak references to them.
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static void
impurestore_dealloc(PyObject *o)
{
    _Py_UOpsImpureStore *self = (_Py_UOpsImpureStore *)o;
    Py_XDECREF(self->next);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject _Py_UOpsPureStore_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops pure store",
    .tp_basicsize = sizeof(_Py_UOpsPureStore) - sizeof(_Py_UOpsSymbolicExpression *),
    .tp_itemsize = sizeof(_Py_UOpsSymbolicExpression *),
    .tp_dealloc = purestore_dealloc,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

static PyTypeObject _PyUOpsImpureStore_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops impure store",
    .tp_basicsize = sizeof(_Py_UOpsImpureStore),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)impurestore_dealloc,
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
    // Symbolic version of co_consts
    PyObject *sym_consts;

    // Max stacklen
    int stack_len;
    int locals_len;
    int curr_stacklen;
    // Actual stack and locals are stored in the current abstract store
    // Borrowed reference
    _Py_UOpsStoreUnion *curr_store;
} _Py_UOpsAbstractInterpContext;

static void
abstractinterp_dealloc(PyObject *o)
{
    _Py_UOpsAbstractInterpContext *self = (_Py_UOpsAbstractInterpContext *)o;
    Py_DECREF(self->sym_exprs_to_sym_exprs);
    Py_DECREF(self->sym_consts);
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

static inline _Py_UOpsSymbolicExpression*
sym_init_const(_Py_UOpsAbstractInterpContext *ctx, PyObject *const_val, int const_idx);


_Py_UOpsAbstractInterpContext *
_Py_UOpsAbstractInterpContext_New(_Py_UOpsPureStore *store, PyObject *co_consts,
                                  int stack_len, int locals_len, int curr_stacklen)
{
    _Py_UOpsAbstractInterpContext *self = PyObject_NewVar(_Py_UOpsAbstractInterpContext,
                                                          &_Py_UOpsAbstractInterpContext_Type,
                                                          stack_len + locals_len);
    if (self == NULL) {
        return NULL;
    }

    PyObject *sym_consts = NULL;

    self->sym_consts = NULL;
    self->curr_store = NULL;

    self->sym_exprs_to_sym_exprs = (PyDictObject *)PyDict_New();
    if (self->sym_exprs_to_sym_exprs == NULL) {
        goto error;
    }

    Py_ssize_t co_const_len = PyTuple_GET_SIZE(co_consts);
    sym_consts = PyTuple_New(co_const_len);
    if (sym_consts == NULL) {
        goto error;
    }
    for (Py_ssize_t i = 0; i < co_const_len; i++) {
        _Py_UOpsSymbolicExpression *res = sym_init_const(self, PyTuple_GET_ITEM(co_consts, i), (int)i);
        if (res == NULL) {
            goto error;
        }
        PyTuple_SET_ITEM(sym_consts, i, res);
    }

    self->sym_consts = sym_consts;
    self->stack_len = stack_len;
    self->locals_len = locals_len;
    self->curr_stacklen = curr_stacklen;

    return self;

error:
    Py_XDECREF(sym_consts);
    Py_DECREF(self);
    return NULL;
}

static _Py_UOpsSymbolicExpression *
check_sym_already_exists(_Py_UOpsAbstractInterpContext *ctx, _Py_UOpsSymbolicExpression *self)
{
    // Unknown opcodes are treated as always unique
    if (self->inst.opcode == CACHE) {
        return self;
    }
    assert(ctx->sym_exprs_to_sym_exprs);

    // Check if this sym expr already exists
    PyObject *res = PyDict_GetItem(
        (PyObject *)ctx->sym_exprs_to_sym_exprs, (PyObject *)self);
    // No entry, return ourselves.
    if (res == NULL) {
        // Add it to our sym expression global book
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
    ((_Py_UOpsSymbolicExpression *)res)->usage_count++;
    return (_Py_UOpsSymbolicExpression *)res;
}

// Steals a reference to const_val
static _Py_UOpsSymbolicExpression*
_Py_UOpsSymbolicExpression_New(_Py_UOpsAbstractInterpContext *ctx,
                               _PyUOpInstruction inst,
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
    self->usage_count = 1;
    self->in_degree = 0;
    self->emitted_count = 0;
    self->sym_type.types = 0;
    self->inst = inst;
    self->operand = 0;
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
        // Constant values shouldn't calculate their in degrees. Makes no sense to.
        if (self->operands[i]->const_val == NULL) {
            self->operands[i]->in_degree++;
        }
    }

    va_end(curr);

    return check_sym_already_exists(ctx, self);
}

static _Py_UOpsSymbolicExpression*
_Py_UOpsSymbolicExpression_NewSingleton(
    _Py_UOpsAbstractInterpContext *ctx,
    _PyUOpInstruction inst)
{
    _Py_UOpsSymbolicExpression *self = PyObject_NewVar(_Py_UOpsSymbolicExpression,
                                                          &_Py_UOpsSymbolicExpression_Type,
                                                          0);
    if (self == NULL) {
        return NULL;
    }


    self->idx = -1;
    self->cached_hash = -1;
    self->usage_count = 0;
    self->in_degree = 0;
    self->sym_type.types = 0;
    self->inst = inst;
    self->operand = 0;
    self->const_val = NULL;
    self->originating_store = 0;

    return check_sym_already_exists(ctx, self);
}

static _Py_UOpsSymbolicExpression*
_Py_UOpsSymbolicExpression_NewFromArray(_Py_UOpsAbstractInterpContext *ctx,
                                _PyUOpInstruction inst, int num_subexprs,
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
    self->sym_type.types = 0;
    self->const_val = NULL;
    self->inst = inst;

    // Setup
    for (int i = 0; i < num_subexprs; i++) {
        self->operands[i] = arr_start[i];
        self->operands[i]->usage_count++;
        // Constant values shouldn't calculate their in degrees. Makes no sense to.
        if (self->operands[i]->const_val == NULL) {
            self->operands[i]->in_degree++;
        }
    }


    return check_sym_already_exists(ctx, self);
}

static void
sym_set_type(_Py_UOpsSymbolicExpression *sym, _Py_UOpsSymExprTypeEnum typ, uint32_t aux)
{
    sym->sym_type.types |= 1 << typ;
    if (typ <= MAX_TYPE_WITH_AUX) {
        sym->sym_type.aux[typ] = aux;
    }
}

static void
sym_set_type_from_const(_Py_UOpsSymbolicExpression *sym, PyObject *obj)
{
    PyTypeObject *tp = Py_TYPE(obj);

    if (tp == &PyLong_Type) {
        sym_set_type(sym, PYINT_TYPE, 0);
    }
    else if (tp == &PyFloat_Type) {
        sym_set_type(sym, PYFLOAT_TYPE, 0);
    }
    else if (tp == &PyUnicode_Type) {
        sym_set_type(sym, PYUNICODE_TYPE, 0);
    }

    if (tp->tp_flags & Py_TPFLAGS_MANAGED_DICT) {
        PyDictOrValues *dorv = _PyObject_DictOrValuesPointer(obj);

        if (_PyDictOrValues_IsValues(*dorv) ||
            _PyObject_MakeInstanceAttributesFromDict(obj, dorv)) {
            sym_set_type(sym, GUARD_DORV_VALUES_INST_ATTR_FROM_DICT_TYPE, 0);

            PyTypeObject *owner_cls = tp;
            PyHeapTypeObject *owner_heap_type = (PyHeapTypeObject *)owner_cls;
            sym_set_type(
                sym,
                GUARD_KEYS_VERSION_TYPE,
                owner_heap_type->ht_cached_keys->dk_version
            );
        }

        if (!_PyDictOrValues_IsValues(*dorv)) {
            sym_set_type(sym, GUARD_DORV_VALUES_TYPE, 0);
        }
    }

    sym_set_type(sym, GUARD_TYPE_VERSION_TYPE, tp->tp_version_tag);
}


static inline _Py_UOpsSymbolicExpression*
sym_init_var(_Py_UOpsAbstractInterpContext *ctx, int locals_idx)
{
    _PyUOpInstruction inst = {INIT_FAST, locals_idx, 0, 0};
    return _Py_UOpsSymbolicExpression_New(ctx,
                                          inst,
                                          NULL, 0);
}

static inline _Py_UOpsSymbolicExpression*
sym_init_unknown(_Py_UOpsAbstractInterpContext *ctx)
{
    _PyUOpInstruction inst = {CACHE, 0, 0, 0};
    return _Py_UOpsSymbolicExpression_New(ctx,
                                          inst,
                                          NULL, 0);
}

static inline _Py_UOpsSymbolicExpression*
sym_init_const(_Py_UOpsAbstractInterpContext *ctx, PyObject *const_val, int const_idx)
{
    _PyUOpInstruction inst = {LOAD_CONST, const_idx, 0, 0};
    _Py_UOpsSymbolicExpression *temp = _Py_UOpsSymbolicExpression_New(
        ctx,
        inst,
        const_val,
        0
    );
    if (temp == NULL) {
        return NULL;
    }
    sym_set_type_from_const(temp, const_val);
    return temp;
}

static inline _Py_UOpsSymbolicExpression*
sym_init_guard(_Py_UOpsAbstractInterpContext *ctx, _PyUOpInstruction *guard,
               int num_stack_inputs)
{
    assert(ctx->curr_store->pure_or_impure);
    _Py_UOpsSymbolicExpression *res =
        _Py_UOpsSymbolicExpression_NewFromArray(
            ctx,
            *guard,
            num_stack_inputs,
            &((_Py_UOpsPureStore *)ctx->curr_store)->stack_pointer[-(num_stack_inputs)]
        );
    if (res == NULL) {
        return NULL;
    }
    res->operand = guard->operand;
    return res;
}

static inline bool
sym_matches_type(_Py_UOpsSymbolicExpression *sym, _Py_UOpsSymExprTypeEnum typ, uint32_t aux)
{
    if ((sym->sym_type.types & (1 << typ)) == 0) {
        return false;
    }
    if (typ <= MAX_TYPE_WITH_AUX) {
        return sym->sym_type.aux[typ] == aux;
    }
    return true;
}

static _Py_UOpsPureStore*
_Py_UOpsPureStore_New(_Py_UOpsAbstractInterpContext *ctx)
{
    bool is_first_store = false;
    _Py_UOpsPureStore *self = PyObject_NewVar(_Py_UOpsPureStore,
                                              &_Py_UOpsPureStore_Type,
                                              (ctx->locals_len + ctx->stack_len) * 2);
    if (self == NULL) {
        return NULL;
    }
    self->pure_or_impure = 1;
    self->next = NULL;


    // Setup

    // We are the first store
    // We MUST create it here, because new symbolic expressions require a store set.
    if (ctx->curr_store == NULL) {
        is_first_store = true;
        ctx->curr_store = (_Py_UOpsStoreUnion *)self;
    }
    // Initialize the hoisted guards
    self->hoisted_guards = PyList_New(0);
    if (self->hoisted_guards == NULL) {
        return NULL;
    }

    self->locals = self->locals_with_stack;
    self->stack = self->locals_with_stack + ctx->locals_len;
    self->stack_pointer = self->stack + ctx->curr_stacklen;
    self->initial_locals = self->stack + ctx->stack_len;
    self->initial_stack = self->initial_locals + ctx->locals_len;

    int total_len = (ctx->locals_len + ctx->stack_len) * 2;
    // Null out everything first
    for (int i = 0; i < total_len; i++) {
        self->locals_with_stack[i] = NULL;
    }
    // Initialize with the initial state of all local variables
    for (int i = 0; i < ctx->locals_len; i++) {
        // TODO copy over immutables
        _Py_UOpsSymbolicExpression *local = sym_init_var(ctx, i);
        if (local == NULL) {
            goto error;
        }
        self->locals[i] = local;
    }


    // Initialize the stack as well
    for (int i = 0; i < ctx->curr_stacklen; i++) {
        // TODO copy over the immutables
        _Py_UOpsSymbolicExpression *stackvar = sym_init_unknown(ctx);
        if (stackvar == NULL) {
            goto error;
        }
        self->stack[i] = stackvar;
    }

    // Transfer ownership (no incref).
    if (!is_first_store) {
        ctx->curr_store->next = self;
    }

    return self;

error:
    Py_DECREF(self);
    return NULL;
}

// Steals a reference to next
static struct _Py_UOpsImpureStore*
_Py_UOpsImpureStore_New(_Py_UOpsAbstractInterpContext *ctx)
{
    _Py_UOpsImpureStore *self = PyObject_NewVar(_Py_UOpsImpureStore,
                                                &_PyUOpsImpureStore_Type,
                                                0);
    if (self == NULL) {
        return NULL;
    }

    // Transfer ownership (no incref)
    if (ctx->curr_store) {
        ctx->curr_store->next = (_Py_UOpsPureStore *)self;
    }

    self->pure_or_impure = 0;
    self->next = NULL;


    return self;
}

static inline bool
op_is_jump(uint32_t opcode)
{
    return (opcode == _POP_JUMP_IF_FALSE || opcode == _POP_JUMP_IF_TRUE);
}

static inline bool
op_is_end(uint32_t opcode)
{
    return opcode == _EXIT_TRACE || opcode == _JUMP_TO_TOP;
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

static int
copy_over_exit_stubs(_PyUOpInstruction *old_trace, int old_trace_len,
                     _PyUOpInstruction *new_trace, int new_trace_len)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
    DPRINTF(3, "EMITTING SIDE EXIT STUBS\n");
#endif

    int new_trace_len_copy = new_trace_len;
    for (int i = 0; i < new_trace_len_copy; i++) {
        _PyUOpInstruction inst = new_trace[i];
        if (op_is_jump(inst.opcode)) {
            // Find target in original trace
            _PyUOpInstruction *target = old_trace + inst.oparg;
            // Point to inst to the new stub
            (&new_trace[i])->oparg = new_trace_len;
            // Start emitting exit stub from there
            do {
                DPRINTF(3, "Emitting instruction at [%d] op: %s, oparg: %d, operand: %" PRIu64 " \n",
                        (int)(new_trace_len),
                        (target->opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[target->opcode],
                        target->oparg,
                        target->operand);

                new_trace[new_trace_len] = *target;
                new_trace_len++;
                target++;
            }
            while(!op_is_end((target-1)->opcode));

        }
    }
    return new_trace_len;
}

typedef enum {
    ABSTRACT_INTERP_ERROR,
    ABSTRACT_INTERP_NORMAL,
    ABSTRACT_INTERP_GUARD_REQUIRED,
} AbstractInterpExitCodes;

// 1 on success
// 0 on failure
// -1 on exception
static int
try_hoist_guard(_Py_UOpsAbstractInterpContext *ctx,
                _PyUOpInstruction *curr)
{
    assert(ctx->curr_store->pure_or_impure);
    _Py_UOpsPureStore *curr_store = ((_Py_UOpsPureStore *)ctx->curr_store);
    bool can_hoist = true;
    int locals_idx = -1;

    // Try hoisting the guard.
    // A guard can be hoisted IFF all its inputs are in the initial
    // locals state, or are constants.
    // TODO constant input
    // Global state guards can't be hoisted.
    // Assumption: state guards are those that have no stack effect.

    int num_stack_inputs = _PyOpcode_num_popped(curr->opcode, curr->opcode, false);
    if (num_stack_inputs == 0) {
        return 0;
    }
    for (int i = 0; i < num_stack_inputs; i++) {
        bool input_present = false;
        _Py_UOpsSymbolicExpression *input = curr_store->stack_pointer[-(i + 1)];
        for (int x = 0; x < ctx->locals_len; x++) {
            if (input == curr_store->initial_locals[x]) {
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
        sym_init_guard(ctx, curr, num_stack_inputs);
    if (guard == NULL) {
        return -1;
    }
    int res = PyList_Append(curr_store->hoisted_guards, (PyObject *)guard);
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

#define DEOPT_IF(COND, INSTNAME) \
    if ((COND)) {                \
        goto guard_required;         \
    }

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
    bool should_type_propagate
)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif

#define STACK_LEVEL()     ((int)(stack_pointer - curr_store->stack))
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
#define GETLOCAL(idx)          ((curr_store->locals[idx]))

#define CURRENT_OPARG() (oparg)

#define CURRENT_OPERAND() (operand)

#define STAT_INC(opname, name) ((void)0)
#define TIER_TWO_ONLY ((void)0)

    int oparg = inst->oparg;
    uint32_t opcode = inst->opcode;
    uint64_t operand = inst->operand;

    assert(ctx->curr_store->pure_or_impure);
    _Py_UOpsPureStore *curr_store = ((_Py_UOpsPureStore *)ctx->curr_store);

    _Py_UOpsSymbolicExpression **stack_pointer = curr_store->stack_pointer;

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
            if(PEEK(1) == NULL || PEEK(1)->inst.opcode == INIT_FAST) {
                goto error;
            }
            break;
        case LOAD_FAST:
            STACK_GROW(1);
            // Guaranteed by the CPython bytecode compiler to not be uninitialized.
            PEEK(1) = GETLOCAL(oparg);
            assert(PEEK(1));

            break;
        case LOAD_FAST_AND_CLEAR: {
            STACK_GROW(1);
            PEEK(1) = GETLOCAL(oparg);
            GETLOCAL(oparg) = NULL;
            break;
        }
        case LOAD_CONST: {
            // TODO, symbolify all the constants and load from there directly.
            STACK_GROW(1);
            PEEK(1) = (_Py_UOpsSymbolicExpression *)GETITEM(ctx->sym_consts, oparg);
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

        case POP_TOP: {
            STACK_SHRINK(1);
            break;
        }

        case PUSH_NULL: {
            STACK_GROW(1);
            _PyUOpInstruction inst = {PUSH_NULL, 0, 0, 0};
            _Py_UOpsSymbolicExpression *null_sym =  _Py_UOpsSymbolicExpression_NewSingleton(ctx, inst);
            sym_set_type(null_sym, NULL_TYPE, 0);
            PEEK(1) = null_sym;
            break;
        }

        // TODO SWAP
        default:
            DPRINTF(1, "Unknown opcode in abstract interpreter\n");
            Py_UNREACHABLE();
    }
    DPRINTF(2, "stack_pointer %p\n", stack_pointer);
    curr_store->stack_pointer = stack_pointer;
    ctx->curr_stacklen = STACK_LEVEL();
    assert(STACK_LEVEL() >= 0);

    return ABSTRACT_INTERP_NORMAL;

pop_2_error_tier_two:
    STACK_SHRINK(1);
    STACK_SHRINK(1);
error:
    DPRINTF(1, "Encountered error in abstract interpreter\n");
    return ABSTRACT_INTERP_ERROR;

guard_required:
    return ABSTRACT_INTERP_GUARD_REQUIRED;
}

static _Py_UOpsStoreUnion *
uop_abstract_interpret(
    PyCodeObject *co,
    _PyUOpInstruction *trace,
    int trace_len,
    int curr_stacklen
)
{

#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif
    // Initialize the symbolic consts

    _Py_UOpsAbstractInterpContext *ctx = NULL;
    _Py_UOpsPureStore *store = NULL;
    _Py_UOpsPureStore *first_store = NULL;
    _Py_UOpsSymbolicExpression **prev_store_stack = NULL;
    // Just hold onto the first locals because we need to free them.
    _Py_UOpsSymbolicExpression **first_temp_local_store = NULL;

    ctx = _Py_UOpsAbstractInterpContext_New(
        NULL, co->co_consts, co->co_stacksize, co->co_nlocals, curr_stacklen);
    if (ctx == NULL) {
        goto error;
    }

    store = first_store = _Py_UOpsPureStore_New(ctx);
    if (store == NULL) {
        goto error;
    }
    ctx->sym_curr_id = 0;

    // Copy over the current locals
    memcpy(store->initial_locals, store->locals, ctx->locals_len * sizeof(_Py_UOpsSymbolicExpression *));
    memcpy(store->initial_stack, store->stack, ctx->stack_len * sizeof(_Py_UOpsSymbolicExpression *));


    _PyUOpInstruction *curr = trace;
    _PyUOpInstruction *end = trace + trace_len;

    while (curr < end && !op_is_end(curr->opcode)) {

        DPRINTF(3, "starting pure region\n")

        // Form pure regions
        while(curr < end && (_PyOpcode_ispure(curr->opcode) ||
            _PyOpcode_isguard(curr->opcode) ||
            is_bookkeeping_opcode(curr->opcode))) {

            int status = uop_abstract_interpret_single_inst(
                co, curr, ctx,
                false
            );
            if (status == ABSTRACT_INTERP_ERROR) {
                goto error;
            }

            if (status == ABSTRACT_INTERP_GUARD_REQUIRED) {
                int res = try_hoist_guard(ctx, curr);
                if (res < 0) {
                    goto error;
                }
                // Cannot hoist the guard, break out of pure region formation.
                if (res == 0) {
                    DPRINTF(3, "breaking out due to guard\n");
                    break;
                }
                DPRINTF(3, "hoisted guard!\n");
                // Type propagate its information into this pure region
                int stat = uop_abstract_interpret_single_inst(
                    co, curr, ctx,
                    true
                );
                assert(stat == ABSTRACT_INTERP_NORMAL);
            }

            curr++;
        }

        if (curr >= end || op_is_end(curr->opcode)) {
            break;
        }

        assert(store->pure_or_impure == 1);
        prev_store_stack = store->stack;

        DPRINTF(3, "creating impure region\n")
        // The last instruction of the previous region should be a _CHECK_VALIDITY
        assert((curr-1)->opcode == _CHECK_VALIDITY);
        assert((curr-2)->opcode == _SET_IP);
        curr-=2;
        _Py_UOpsImpureStore *impure_store = _Py_UOpsImpureStore_New(ctx);
        if (impure_store == NULL) {
            goto error;
        }
        ctx->curr_store = (_Py_UOpsStoreUnion *)impure_store;
        // Create a new abstract store to keep track of stack and local effects for
        // the next pure region.
        store = _Py_UOpsPureStore_New(ctx);
        if (store == NULL) {
            goto error;
        }

        ctx->curr_store = (_Py_UOpsStoreUnion *)store;

        assert(ctx->curr_store);
        // Form impure region
        impure_store->start = curr;
        // SET_IP shouldn't break and form a new region.
        while (curr < end && (!_PyOpcode_ispure(curr->opcode) ||
            is_bookkeeping_opcode(curr->opcode) ||
            is_dataonly_opcode(curr->opcode))) {
            DPRINTF(3, "impure opcode: %d\n", curr->opcode);
            int num_stack_inputs = _PyOpcode_num_popped((int)curr->opcode, (int)curr->oparg, false);
            // Adjust the stack and such
            int status = uop_abstract_interpret_single_inst(
                co, curr, ctx,
                true
            );
            assert(status == ABSTRACT_INTERP_NORMAL || status == ABSTRACT_INTERP_GUARD_REQUIRED);
            curr++;
            if (op_is_end(curr->opcode)) {
                break;
            }
        }
        impure_store->end = curr;

        if (curr >= end || op_is_end(curr->opcode)) {
            break;
        }

        // Copy over the current locals
        memcpy(store->initial_locals, store->locals, ctx->locals_len * sizeof(_Py_UOpsSymbolicExpression *));
        memcpy(store->initial_stack, store->stack, ctx->stack_len * sizeof(_Py_UOpsSymbolicExpression *));


    }

    // TODO proper lifecycle for ctx later
    // Py_DECREF(ctx);
    PyMem_Free(first_temp_local_store);
    return (_Py_UOpsStoreUnion *)first_store;

error:
    if(PyErr_Occurred()) {
        PyErr_Clear();
    }
    // TODO proper lifecycle for ctx later
    // Py_DECREF(ctx);
    PyMem_Free(first_temp_local_store);
    return NULL;
}

typedef struct _Py_UOpsEmitter {
    _PyUOpInstruction *writebuffer;
    _PyUOpInstruction *writebuffer_end;
    int curr_i;
    // Calculated from writebuffer_end + n_scratch_slots
    // Note: each slot is 128 bit instruction, so it can hold 2 64 bit
    // PyObjects
    int n_scratch_slots;
    int max_scratch_slots;
    // A dict mapping the common expressions to the slots indexes.
    PyObject *common_syms;

    // Layed out in reverse order.
    // The first scratch slot is the last entry of the buffer, counting
    // backwards. Ie scratch_start > scratch_end
    PyObject **scratch_start;
    PyObject **scratch_end;
    PyObject **scratch_available;

} _Py_UOpsEmitter;

static inline int
emit_i(_Py_UOpsEmitter *emitter,
       _PyUOpInstruction inst)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif
    if (emitter->curr_i < 0) {
        return -1;
    }
    if (emitter->writebuffer + emitter->curr_i >= emitter->writebuffer_end) {
        return -1;
    }
    DPRINTF(3, "Emitting instruction at [%d] op: %s, oparg: %d, operand: %" PRIu64 " \n",
            emitter->curr_i,
            (inst.opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[inst.opcode],
            inst.oparg,
            inst.operand);
    emitter->writebuffer[emitter->curr_i] = inst;
    emitter->curr_i++;
    return 0;
}

static int
compile_sym_to_uops(_Py_UOpsEmitter *emitter,
                    _Py_UOpsSymbolicExpression *sym,
                    _Py_UOpsPureStore *store,
                    bool use_locals, bool do_cse);

// Find a slot to store the result of a common subexpression.
static int
compile_common_sym(_Py_UOpsEmitter *emitter,
            _Py_UOpsSymbolicExpression *sym,
           _Py_UOpsPureStore *store,
           bool use_locals)
{
    sym->emitted_count++;

    PyObject *idx = PyDict_GetItem(emitter->common_syms, (PyObject *)sym);
    // Present - just use that
    if (idx != NULL) {
        long index = PyLong_AsLong(idx);
        assert(!PyErr_Occurred());
        PyObject **addr = emitter->scratch_start - index;
        _PyUOpInstruction load_common = {_LOAD_COMMON, 0, 0, (uint64_t)addr};
        if(emit_i(emitter, load_common) < 0) {
            return -1;
        }
        return 0;
    }

    // Not present, emit the whole thing, then store that
    if (compile_sym_to_uops(emitter, sym, store, use_locals, false) < 0) {
        return -1;
    }

    // Not really used - not worth to store it.
    if (!(sym->usage_count > 1 && sym->emitted_count < (sym->usage_count))){
        return 0;
    }
    // No space left, TODO evict something based on usage count.
    // And store there.
    if (emitter->n_scratch_slots >= emitter->max_scratch_slots) {
        return 0;
    }

    // If there's space, expand the scratch slot.
    emitter->scratch_end--;
    // Grow backwards.
    while ((char *)emitter->writebuffer_end > (char *)emitter->scratch_end) {
        emitter->writebuffer_end--;
    }
    emitter->n_scratch_slots++;
    PyObject **available = emitter->scratch_available;
    assert(available >= emitter->scratch_end);
    emitter->scratch_available--;

    *available = NULL;
    // TODO memory leak
    _PyUOpInstruction store_common = {_STORE_COMMON, 0, 0, (uint64_t)available};
    if(emit_i(emitter, store_common) < 0) {
        return -1;
    }
    long index = (long)(emitter->scratch_start - available);
    assert(index >= 0);
    idx = PyLong_FromLong(index);
    if (idx == NULL) {
        return -1;
    }
    if (PyDict_SetItem(emitter->common_syms, (PyObject *)sym, idx) < 0) {
        PyErr_Clear();
        Py_DECREF(idx);
        return -1;
    }
    Py_DECREF(idx);
    return 0;
}

static int
compile_sym_to_uops(_Py_UOpsEmitter *emitter,
                   _Py_UOpsSymbolicExpression *sym,
                   _Py_UOpsPureStore *store,
                   bool use_locals,
                   bool do_cse)
{
    _PyUOpInstruction inst;
    // Since CPython is a stack machine, just compile in the order
    // seen in the operands, then the instruction itself.

    // Constant propagated value, load immediate constant
    if (sym->const_val != NULL) {
        inst.opcode = _LOAD_CONST_IMMEDIATE;
        inst.oparg = 0;
        // TODO memory leak.
        inst.operand = (uint64_t)Py_NewRef(sym->const_val);
        return emit_i(emitter, inst);
    }


    // Common subexpression elimination.
    if (do_cse && !_PyOpcode_isterminal(sym->inst.opcode) && sym->usage_count > 1) {
        return compile_common_sym(emitter, sym, store, use_locals);
    }

    // If sym is already in locals, just reuse that.
    // TODO use a dict here.
    if (use_locals) {
        int locals_len = (int)(store->stack - store->locals);
        for (int i = 0; i < locals_len; i++) {
            // NOTE: This is the current locals, not the initial one!z
            if (sym == store->locals[i]) {
                inst.opcode = LOAD_FAST;
                inst.oparg = i;
                inst.operand = 0;
                return emit_i(emitter, inst);
            }
        }
    }

    if (_PyOpcode_isterminal(sym->inst.opcode)) {
        // These are for unknown stack entries.
        if (_PyOpcode_isunknown(sym->inst.opcode)) {
            // Leave it be. These are initial values from the start
            return 0;
        }
        inst = sym->inst;
        inst.opcode = sym->inst.opcode == INIT_FAST ? LOAD_FAST : sym->inst.opcode;
        return emit_i(emitter, inst);
    }

    // Compile each operand
    Py_ssize_t operands_count = Py_SIZE(sym);
    for (int i = 0; i < operands_count; i++) {
        if (sym->operands[i] == NULL) {
            continue;
        }
        // TODO Py_EnterRecursiveCall ?
        if (compile_sym_to_uops(
            emitter,
            sym->operands[i],
            store, use_locals, true) < 0) {
            return -1;
        }
    }

    // Finally, emit the operation itself.
    return emit_i(emitter, sym->inst);
}

// Note: when we start supporting loop optimization,
// this code will break as it assumes a DAG.
static void
decrement_in_degree(_Py_UOpsSymbolicExpression *e)
{
    e->in_degree--;
    Py_ssize_t operand_count = Py_SIZE(e);
    for (Py_ssize_t i = 0; i < operand_count; i++) {
        _Py_UOpsSymbolicExpression *o = e->operands[i];
        decrement_in_degree(o);
    }
}

// Counts the number of references e has to comp.
static int
count_references_to(_Py_UOpsSymbolicExpression *e, _Py_UOpsSymbolicExpression *comp)
{
    int res =  e == comp ? 1 : 0;
    Py_ssize_t operand_count = Py_SIZE(e);
    for (Py_ssize_t i = 0; i < operand_count; i++) {
        _Py_UOpsSymbolicExpression *o = e->operands[i];
        res += count_references_to(o, comp);
    }
    return res;
}

static int
emit_uops_from_pure_store(
    PyCodeObject *co,
    _Py_UOpsPureStore *pure_store,
    _Py_UOpsEmitter *emitter
)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif
    DPRINTF(3, "EMITTING PURE REGION\n");
    int locals_len = co->co_nlocals;

    int stack_grown = 0;
    // 1. Emit the hoisted guard region.
    PyObject *hoisted_guards = pure_store->hoisted_guards;
    Py_ssize_t len = PyList_GET_SIZE(hoisted_guards);
    for (Py_ssize_t i = 0; i < len; i++) {
        // For each guard, emit the prerequisite loads
        _Py_UOpsSymbolicExpression *guard = (_Py_UOpsSymbolicExpression *)
            PyList_GET_ITEM(hoisted_guards, i);
        Py_ssize_t guard_operands_count = Py_SIZE(guard);
        int load_fast_count = 0;
        for (int guard_op_idx = 0; guard_op_idx < guard_operands_count; guard_op_idx++) {
            _Py_UOpsSymbolicExpression *guard_operand = guard->operands[guard_op_idx];
            // Search local for the thing
            for (int locals_idx = 0; locals_idx < locals_len; locals_idx++) {
                if (guard_operand == pure_store->initial_locals[locals_idx]) {
                    _PyUOpInstruction load_fast = {_LOAD_FAST_NO_INCREF, locals_idx, 0, 0};
                    if (emit_i(emitter,load_fast) < 0) {
                        return -1;
                    }
                    load_fast_count++;
                    break;
                }
            }
            // Todo: search consts for the thing
        }
        assert(load_fast_count == guard_operands_count);
        stack_grown += load_fast_count;
        // Now, time to emit the guard itself
        if (emit_i(emitter, guard->inst) < 0) {
            return -1;
        }

        decrement_in_degree(guard);

    }

    if (stack_grown) {
        // Shrink stack after checking guards.
        // TODO HIGH PRIORITY on any deoptimize, the stack needs to be popped by the deopt too.
        _PyUOpInstruction stack_shrink = {_SHRINK_STACK, stack_grown, 0};

        if (emit_i(emitter, stack_shrink) < 0) {
            return -1;
        }
    }

    // 2. Emit the pure region itself.
    // Need to emit in the following order:
    // Locals first, then followed by stack, from bottom to top of stack.
    // Due to CPython's stack machine style, this will naturally produce
    // stack-valid code.

    DPRINTF(2, "==EMITTING LOCALS==:\n");
    // Final state of the locals.
    // TODO make sure we don't blow the stack!!!!
    // We need to emit the locals in topological order.
    // That is, we emit locals that other locals do not rely on first.
    // This allows us to have no data conflicts.

    bool done = false;
    // The hard upper bound to number of loops we need should be
    // the number of locals.
    for (int loops = 0; loops < locals_len && !done; loops++) {
        done = true;
        for (int locals_i = 0; locals_i < locals_len; locals_i++) {
            _Py_UOpsSymbolicExpression *local = pure_store->locals[locals_i];
            _Py_UOpsSymbolicExpression *initial_local = pure_store->initial_locals[locals_i];
            // If no change in locals, don't emit:
            if (local == initial_local) {
                continue;
            }
            done = false;

            DPRINTF(3, "In degree: i: %d deg: %d\n", locals_i, initial_local ? initial_local->in_degree : -999);
            // If this local does not depend on anything else, ie its
            // in_degree is 0, then emit it.
            // Might be < 0 for things like constants, where the in degree
            // is irrelevant.
            int initial_local_in_degree = initial_local == NULL ? -999 : initial_local->in_degree;
            if (initial_local_in_degree <= 0 ||
                (local != NULL &&
                    (local->const_val != NULL ||
                        count_references_to(local, initial_local) == initial_local_in_degree))) {
                if (compile_sym_to_uops(
                    emitter,
                    local,
                    pure_store,
                    false, true) < 0) {
                    return -1;
                }

                // After that, reduce the in_degree of all its constituents.
                decrement_in_degree(local);

                // Mark as done.
                pure_store->initial_locals[locals_i] = local;

                _PyUOpInstruction prev_i = emitter->writebuffer[emitter->curr_i-1];
                // Micro optimizations -- if LOAD_FAST then STORE_FAST, get rid of that
                if (prev_i.opcode == LOAD_FAST && prev_i.oparg == locals_i) {
                    emitter->curr_i--;
                    DPRINTF(3, "LOAD_FAST to be followed by STORE_FAST, ignoring LOAD_FAST. \n");
                    continue;
                }
                _PyUOpInstruction store_fast = {STORE_FAST, locals_i, 0, 0};
                if (emit_i(emitter, store_fast) < 0) {
                    return -1;
                }
            }
        }
    }

    // Error could not resolve the DAG
    if (!done) {
        return -1;
    }

    DPRINTF(2, "==================\n");
    DPRINTF(2, "==EMITTING STACK==:\n");
    int stack_len = (int)(pure_store->stack_pointer - pure_store->stack);
    for (int stack_i = 0; stack_i < stack_len; stack_i++) {
        // If no change in stack, don't emit:
        // Do we need _SWAP_AND_POP just to be safe?
        if (pure_store->stack[stack_i] == pure_store->initial_stack[stack_i]) {
            continue;
        }

        if (compile_sym_to_uops(
            emitter,
            pure_store->stack[stack_i],
            pure_store,
            true, true) < 0) {
            return -1;
        }
    }
    DPRINTF(2, "==================\n");

    return 0;
}

static int
emit_uops_from_impure_store(
    PyCodeObject *co,
    _Py_UOpsImpureStore *impure_store,
    _Py_UOpsEmitter *emitter
)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
    DPRINTF(3, "EMITTING IMPURE REGION\n");
#endif
    Py_ssize_t len = (impure_store->end - impure_store->start);
    assert(impure_store->start->opcode == _SET_IP);

    if (emitter->writebuffer + len >= emitter->writebuffer_end) {
        return -1;
    }

#ifdef Py_DEBUG
    for (Py_ssize_t i = 0; i < len; i++) {
        _PyUOpInstruction inst = impure_store->start[i];
        DPRINTF(2, "Emitting instruction at [%d] op: %s, oparg: %d, operand: %" PRIu64 " \n",
                (int)(emitter->curr_i + i),
                (inst.opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[inst.opcode],
                inst.oparg,
                inst.operand);
    }
#endif


    memcpy(
        emitter->writebuffer + emitter->curr_i,
        impure_store->start,
        len * sizeof(_PyUOpInstruction)
    );

    emitter->curr_i += (int)len;
    return 0;
}

static int
emit_uops_from_stores(
    PyCodeObject *co,
    _Py_UOpsStoreUnion *first_store,
    _PyUOpInstruction *trace_writebuffer,
    _PyUOpInstruction *writebuffer_end
)
{

    PyObject *sym_store = PyDict_New();
    if (sym_store == NULL) {
        return -1;
    }


    _Py_UOpsEmitter emitter = {
        trace_writebuffer,
        writebuffer_end,
        0,
        0,
        // Should not use more than 20% of the space for common expressions.
        (int)((writebuffer_end - trace_writebuffer) / 5),
        sym_store,
        // One wasted object, but it's fine I'd rather not use that to prevent logic bugs.
        (PyObject **)(writebuffer_end - 1),
        (PyObject **)(writebuffer_end - 1),
        (PyObject **)(writebuffer_end - 1)
    };

    // Emission is simple: traverse the linked list of stores:
    // - For pure stores, emit their final states.
    //     - For hoisted guards of pure stores, emit the stuff they need,
    //       at the end, adjust the stack back to the level we need.
    // - For impure stores, just emit them directly.

    _Py_UOpsStoreUnion *curr_store = first_store;
    while(curr_store != NULL) {
        if (curr_store->pure_or_impure) {
            if (emit_uops_from_pure_store(co,
                                      (_Py_UOpsPureStore *)curr_store,
                                      &emitter) < 0) {
                goto error;
            }
        }
        else {
            if (emit_uops_from_impure_store(co,
                                      (_Py_UOpsImpureStore *)curr_store,
                                      &emitter) < 0) {
                goto error;
            }
        }
        curr_store = curr_store->next;
    }

    Py_DECREF(sym_store);

    // Add the _JUMP_TO_TOP at the end of the trace.
    _PyUOpInstruction jump_to_top = {_JUMP_TO_TOP, 0, 0};
    if (emit_i(&emitter, jump_to_top) < 0) {
        return -1;
    }
    return emitter.curr_i;

error:
    Py_DECREF(sym_store);
    return -1;
}
static void
remove_unneeded_uops(_PyUOpInstruction *buffer, int buffer_size)
{
    int last_set_ip = -1;
    bool maybe_invalid = false;
    for (int pc = 0; pc < buffer_size; pc++) {
        int opcode = buffer[pc].opcode;
        if (opcode == _SET_IP) {
            buffer[pc].opcode = NOP;
            last_set_ip = pc;
        }
        else if (opcode == _CHECK_VALIDITY) {
            if (maybe_invalid) {
                maybe_invalid = false;
            }
            else {
                buffer[pc].opcode = NOP;
            }
        }
        else if (opcode == _JUMP_TO_TOP || opcode == _EXIT_TRACE) {
            break;
        }
        else {
            if (OPCODE_HAS_ESCAPES(opcode)) {
                maybe_invalid = true;
                if (last_set_ip >= 0) {
                    buffer[last_set_ip].opcode = _SET_IP;
                }
            }
            if (OPCODE_HAS_ERROR(opcode) || opcode == _PUSH_FRAME) {
                if (last_set_ip >= 0) {
                    buffer[last_set_ip].opcode = _SET_IP;
                }
            }
        }
    }
}


int
_Py_uop_analyze_and_optimize(
    PyCodeObject *co,
    _PyUOpInstruction *buffer,
    int buffer_size,
    int curr_stacklen
)
{
    int original_trace_len = buffer_size;
    int trace_len = buffer_size;
    _PyUOpInstruction *temp_writebuffer = NULL;

    temp_writebuffer = PyMem_New(_PyUOpInstruction, trace_len * OVERALLOCATE_FACTOR);
    if (temp_writebuffer == NULL) {
        goto error;
    }


    // Pass: Abstract interpretation and symbolic analysis
    _Py_UOpsStoreUnion *first_abstract_store = uop_abstract_interpret(
        co, buffer,
        trace_len, curr_stacklen);

    if (first_abstract_store == NULL) {
        goto error;
    }

    _PyUOpInstruction *writebuffer_end = temp_writebuffer + buffer_size;
    // Compile the stores
    trace_len = emit_uops_from_stores(
        co,
        first_abstract_store,
        temp_writebuffer,
        writebuffer_end
    );
    if (trace_len < 0) {
        goto error;
    }

    // Pass: fix up side exit stubs. This MUST be called as the last pass!
    // trace_len = copy_over_exit_stubs(buffer, original_trace_len, temp_writebuffer, trace_len);

    // Fill in our new trace!
    memcpy(buffer, temp_writebuffer, trace_len * sizeof(_PyUOpInstruction));

    PyMem_Free(temp_writebuffer);

    remove_unneeded_uops(buffer, buffer_size);

    return 0;
error:
    PyMem_Free(temp_writebuffer);
    return -1;
}
