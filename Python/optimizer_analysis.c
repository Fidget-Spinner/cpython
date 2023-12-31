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
#include "pycore_function.h"
#include "pycore_uop_metadata.h"
#include "pycore_uop_ids.h"


#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_FRAME_GROWTH 128

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
        case PUSH_NULL:
            return true;
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
    return (opcode == _SET_IP ||
        opcode == _CHECK_VALIDITY);
}

// These opcodes just adjust the stack.
static inline bool
is_dataonly_opcode(int opcode) {
    return (opcode == POP_TOP || opcode == SWAP);
}


typedef enum {
    // Types with aux
    GUARD_KEYS_VERSION_TYPE = 0,
    GUARD_TYPE_VERSION_TYPE = 1,
    // TODO improvement: to be more exact, this actually needs to encode oparg
    // info as well, see _CHECK_FUNCTION_EXACT_ARGS.
    // However, since oparg is tied to code object is tied to function version,
    // it should be safe if function version matches.
    PYFUNCTION_TYPE_VERSION_TYPE = 2,

    // Types without aux
    PYINT_TYPE = 3,
    PYFLOAT_TYPE = 4,
    PYUNICODE_TYPE = 5,
    NULL_TYPE = 6,
    PYMETHOD_TYPE = 7,
    GUARD_DORV_VALUES_TYPE = 8,
    GUARD_DORV_VALUES_INST_ATTR_FROM_DICT_TYPE = 9,


    // GUARD_GLOBALS_VERSION_TYPE, / Environment check
    // GUARD_BUILTINS_VERSION_TYPE, // Environment check
    // CHECK_CALL_BOUND_METHOD_EXACT_ARGS_TYPE, // idk how to deal with this, requires stack check
    // CHECK_PEP_523_TYPE, // Environment check
    // CHECK_FUNCTION_EXACT_ARGS_TYPE, // idk how to deal with this, requires stack check
    // CHECK_STACK_SPACE_TYPE // Environment check
    INVALID_TYPE = -1
} _Py_UOpsSymExprTypeEnum;

#define MAX_TYPE_WITH_AUX 2
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

    // Type of the symbolic expression
    _Py_UOpsSymType sym_type;
    PyObject *const_val;
    Py_hash_t cached_hash;
    // The region where this expression was first created.
    // This matters for anything that isn't immutable
    int originating_region;

    // The following fields are for codegen.
    // How many of this expression we have emitted so far. Only used for CSE.
    int emitted_count;
    _PyUOpInstruction inst;

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

    if (self->const_val && other->const_val) {
        return PyObject_RichCompare(self->const_val, other->const_val, Py_EQ);
    }

    if (_PyOpcode_isterminal(self_opcode) != _PyOpcode_isterminal(other_opcode)) {
        Py_RETURN_FALSE;
    }

    if (_PyOpcode_isimmutable(self_opcode) != _PyOpcode_isimmutable(other_opcode)) {
        Py_RETURN_FALSE;
    }

    if (!_PyOpcode_isimmutable(self_opcode)) {
        if (self->originating_region != other->originating_region) {
            Py_RETURN_FALSE;
        }
    }

    // Terminal ops are kinda like special sentinels.
    // They are always considered unique, except for constant values
    // which can be repeated
    if (_PyOpcode_isterminal(self_opcode) && _PyOpcode_isterminal(other_opcode)) {
        // Note: even if two LOAD_FAST have the same opcode and oparg,
        // They are not the same because we are constructing a new terminal.
        // All terminals except constants are unique.
        return self->idx == other->idx ? Py_True : Py_False;
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

    // DONT CHECK THE GUARDS ARE THE SAME. GUARDS CAN DIFFER
    // BECAUSER OF INFO GAINED IN TYPE PROP.
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


typedef enum _Py_UOps_IRStore_IdKind {
    TARGET_NONE = -2,
    TARGET_UNUSED = -1,
    TARGET_LOCAL = 0,
} _Py_UOps_IRStore_IdKind;

typedef enum _Py_UOps_IRStore_EntryKind {
    IR_PLAIN_INST = 0,
    IR_SYMBOLIC = 1,
    IR_FRAME_PUSH_INFO = 2,
    IR_FRAME_POP_INFO = 3,
    IR_NOP = 4,
} _Py_UOps_IRStore_EntryKind;

typedef struct _Py_UOpsOptIREntry {
    _Py_UOps_IRStore_EntryKind typ;
    union {
        // IR_PLAIN_INST
        _PyUOpInstruction inst;
        // IR_SYMBOLIC
        struct {
            _Py_UOps_IRStore_IdKind assignment_target;
            _Py_UOpsSymbolicExpression *expr;
        };
        // IR_FRAME_PUSH_INFO, always precedes a _PUSH_FRAME IR_PLAIN_INST
        struct {
            // Strong reference. Needed for later when we reconstruct the frame.
            // From the code object as a constructor.
            PyCodeObject *frame_co_code;
            // Only used in codegen for bookkeeping.
            struct _Py_UOpsOptIREntry *prev_frame_ir;
            // Only if inlined, then which stack slot to start from in codegen.
            int localsplus_offset;
            bool is_inlineable;
            // Self is consumed if it's not NULL during inlining.
            bool consumed_self;
        };
        // IR_FRAME_POP_INFO, always prior to a _POP_FRAME IR_PLAIN_INST
        // no fields, just a sentinel
    };
} _Py_UOpsOptIREntry;

typedef struct _Py_UOps_Opt_IR {
    PyObject_VAR_HEAD
    int curr_write;
    _Py_UOpsOptIREntry entries[1];
} _Py_UOps_Opt_IR;

static PyTypeObject _Py_UOps_Opt_IR_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops SSA IR",
    .tp_basicsize = sizeof(_Py_UOps_Opt_IR) - sizeof(_Py_UOpsOptIREntry),
    .tp_itemsize = sizeof(_Py_UOpsOptIREntry),
    .tp_dealloc = PyObject_Del,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

static void
ir_store(_Py_UOps_Opt_IR *ir, _Py_UOpsSymbolicExpression *expr, _Py_UOps_IRStore_IdKind store_fast_idx)
{
    // Don't store stuff we know will never get compiled.
    if(_PyOpcode_isunknown(expr->inst.opcode) && store_fast_idx == TARGET_NONE) {
        return;
    }
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
    DPRINTF(3, "ir_store: #%d, expr: %s oparg: %d, operand: %p\n", store_fast_idx,
            (expr->inst.opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[expr->inst.opcode],
                        expr->inst.oparg,
                        (void *)expr->inst.operand);
#endif
    _Py_UOpsOptIREntry *entry = &ir->entries[ir->curr_write];
    entry->typ = IR_SYMBOLIC;
    entry->assignment_target = store_fast_idx;
    entry->expr = expr;
    ir->curr_write++;
}

static void
ir_plain_inst(_Py_UOps_Opt_IR *ir, _PyUOpInstruction inst)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
    DPRINTF(3, "ir_inst: opcode: %s oparg: %d, operand: %p\n",
            (inst.opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[inst.opcode],
                        inst.oparg,
                        (void *)inst.operand);
#endif
    _Py_UOpsOptIREntry *entry = &ir->entries[ir->curr_write];
    entry->typ = IR_PLAIN_INST;
    entry->inst = inst;
    ir->curr_write++;
}

static _Py_UOpsOptIREntry *
ir_frame_push_info(_Py_UOps_Opt_IR *ir)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
    DPRINTF(3, "ir_frame_push_info\n");
#endif
    _Py_UOpsOptIREntry *entry = &ir->entries[ir->curr_write];
    entry->typ = IR_FRAME_PUSH_INFO;
    entry->is_inlineable = false;
    // Unassigned, only assigned at codegen.
    entry->localsplus_offset = -1;
    entry->consumed_self = false;
    // Code object is set by frame push.
    ir->curr_write++;
    return entry;
}

static int
ir_entry_calculate_localsplus_offset(_Py_UOpsOptIREntry *ir, int original_localsplus_offset)
{
    // Root frame.
    if (ir == NULL) {
        return original_localsplus_offset;
    }
    assert(ir->typ == IR_FRAME_PUSH_INFO);
    return ir->localsplus_offset + original_localsplus_offset;
}

static void
ir_frame_pop_info(_Py_UOps_Opt_IR *ir)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
    DPRINTF(3, "ir_frame_pop_info\n");
#endif
    _Py_UOpsOptIREntry *entry = &ir->entries[ir->curr_write];
    entry->typ = IR_FRAME_POP_INFO;
    ir->curr_write++;
}

typedef struct _Py_UOpsAbstractFrame {
    PyObject_VAR_HEAD
    // Strong reference.
    struct _Py_UOpsAbstractFrame *prev;
    // Borrowed reference.
    struct _Py_UOpsAbstractFrame *next;
    // Symbolic version of co_consts
    PyObject *sym_consts;
    // Max stacklen
    int stack_len;
    int locals_len;

    _Py_UOpsOptIREntry *frame_ir_entry;

    _Py_UOpsSymbolicExpression **stack_pointer;
    _Py_UOpsSymbolicExpression **stack;
    _Py_UOpsSymbolicExpression **locals;
    _Py_UOpsSymbolicExpression *locals_with_stack[1];
} _Py_UOpsAbstractFrame;

static void
abstractframe_dealloc(_Py_UOpsAbstractFrame *self)
{
    Py_DECREF(self->sym_consts);
    Py_XDECREF(self->prev);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject _Py_UOpsAbstractFrame_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "uops abstract frame",
    .tp_basicsize = sizeof(_Py_UOpsAbstractFrame) - sizeof(_Py_UOpsSymbolicExpression *),
    .tp_itemsize = sizeof(_Py_UOpsSymbolicExpression *),
    .tp_dealloc = (destructor)abstractframe_dealloc,
    .tp_free = PyObject_Free,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION
};

static int
abstractframe_get_localsplus_index(_Py_UOpsAbstractFrame *self, _Py_UOpsSymbolicExpression **ptr)
{
    assert(ptr != NULL);
    int result = (ptr - self->locals_with_stack);
    assert(result >= 0);
    return result;
}


// Tier 2 types meta interpreter
typedef struct _Py_UOpsAbstractInterpContext {
    PyObject_HEAD
    // Maps a sym_expr to itself, so we can do O(1) lookups of it later
    PyDictObject *sym_exprs_to_sym_exprs;
    // Current ID to assign a new (non-duplicate) sym_expr
    Py_ssize_t sym_curr_id;
    // Stores the symbolic for the upcoming new frame that is about to be created.
    _Py_UOpsSymbolicExpression *new_frame_sym;
    // Localsplus of the new frame that is about to be created. Used during inlining.
    _Py_UOpsSymbolicExpression **new_frame_localsplus;
    _Py_UOpsAbstractFrame *frame;
    // Number of extra PyObject * we need in the frame, heuristic for inlining.
    int frame_entries_needed;

    int curr_region_id;
    _Py_UOps_Opt_IR *ir;

    // The terminating instruction for the trace. Could be _JUMP_TO_TOP or
    // _EXIT_TRACE.
    _PyUOpInstruction *terminating;
} _Py_UOpsAbstractInterpContext;

static void
abstractinterp_dealloc(PyObject *o)
{
    _Py_UOpsAbstractInterpContext *self = (_Py_UOpsAbstractInterpContext *)o;
    Py_DECREF(self->sym_exprs_to_sym_exprs);
    Py_XDECREF(self->frame);
    Py_DECREF(self->ir);
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

// Mark current frame and all previous frames not inlineable
static void
frame_propagate_not_inlineable(_Py_UOpsAbstractInterpContext *ctx)
{
    _Py_UOpsAbstractFrame *curr = ctx->frame;
    // Topmost frame has frame_ir_entry as NULL.
    while (curr != NULL && curr->frame_ir_entry != NULL) {
        curr->frame_ir_entry->is_inlineable = false;
        PyCodeObject *co = curr->frame_ir_entry->frame_co_code;
        ctx->frame_entries_needed -= (co->co_nlocalsplus + co->co_stacksize);
        curr = curr->prev;
    }
}

// Progressive uninlines frames starting from the earliest, until we have
// enough space.
static bool
frame_uninline(_Py_UOpsAbstractInterpContext *ctx, int required_space)
{
    _Py_UOpsAbstractFrame *prev = ctx->frame;
    // First, go to the earliest inlined frame.
    while (prev != NULL && prev->frame_ir_entry != NULL && prev->frame_ir_entry->is_inlineable) {
        prev = prev->prev;
    }

    _Py_UOpsAbstractFrame *curr = prev;
    if (curr == NULL) {
        return false;
    }
    if (curr->frame_ir_entry == NULL) {
        curr = curr->next;
    }
    while (curr != NULL && curr->frame_ir_entry != NULL) {
        curr->frame_ir_entry->is_inlineable = false;
        PyCodeObject *co = curr->frame_ir_entry->frame_co_code;
        ctx->frame_entries_needed -= (co->co_nlocalsplus + co->co_stacksize);
        if (MAX_FRAME_GROWTH - ctx->frame_entries_needed > required_space) {
            return true;
        }
        curr = curr->next;
    }
    return false;
}


// The inlining heuristic is as follows:
// 1. Inline small frames, AND
// 2. Make sure we do not interleave inlined and non-inlined frames.
// They make frame reconstruction for sys._getframe and tracebacks too
// complicated.
static void
frame_decide_inlineable(_Py_UOpsAbstractInterpContext *ctx)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif
    _Py_UOpsAbstractFrame *frame = ctx->frame;
    assert(frame->frame_ir_entry != NULL);
    PyCodeObject *co = frame->frame_ir_entry->frame_co_code;
    int extra_needed = (co->co_nlocalsplus + co->co_stacksize);
    // Ban closures
    if (co->co_ncellvars > 0 || co->co_nfreevars > 0) {
        DPRINTF(3, "inline_fail: closure\n");
        return;
    }
    // Ban generators, async, etc.
    int flags = co->co_flags;
    if ((flags & CO_COROUTINE) ||
        (flags & CO_GENERATOR) ||
        (flags & CO_ITERABLE_COROUTINE) ||
        (flags & CO_ASYNC_GENERATOR) ||
        // TODO we can support these in the future.
        (flags & CO_VARKEYWORDS) ||
        (flags & CO_VARARGS)) {
        DPRINTF(3, "inline_fail: generator/coroutine\n");
        return;
    }
    // Too many locals, or too big stack. This is somewhat arbitrary, but
    // we don't want to inline anything that has too many locals because
    // we would have to copy over a lot more on deopt. Thus
    // making inlining not really worth it.
    // Also too many locals might make our requests for stack space
    // more likely to fail.
    if (extra_needed > 32) {
        frame->frame_ir_entry->is_inlineable = false;
        frame_propagate_not_inlineable(ctx);
        DPRINTF(3, "inline_fail: too many locals/stack\n");
        return;
    }
    // Not enough space left, try un-inlining previous frames.
    if (ctx->frame_entries_needed + extra_needed > MAX_FRAME_GROWTH) {
        if (frame_uninline(ctx, extra_needed)) {
            ctx->frame_entries_needed += extra_needed;
            assert(ctx->frame_entries_needed <= MAX_FRAME_GROWTH);
            frame->frame_ir_entry->is_inlineable = true;
        }
        else {
            frame_propagate_not_inlineable(ctx);
            DPRINTF(3, "inline_fail: out of space\n");
        }
        return;
    }
    DPRINTF(3, "inline_success\n");
    ctx->frame_entries_needed += extra_needed;
    frame->frame_ir_entry->is_inlineable = true;
    return;
}

static inline _Py_UOpsAbstractFrame *
_Py_UOpsAbstractFrame_New(_Py_UOpsAbstractInterpContext *ctx,
PyObject *co_consts, int stack_len, int locals_len,
int curr_stacklen, _Py_UOpsOptIREntry *frame_ir_entry);

static inline _Py_UOps_Opt_IR *
_Py_UOpsSSA_IR_New(int entries)
{
    _Py_UOps_Opt_IR *ir = PyObject_NewVar(_Py_UOps_Opt_IR,
                                          &_Py_UOps_Opt_IR_Type,
                                          entries);
    ir->curr_write = 0;
    return ir;
}

static _Py_UOpsAbstractInterpContext *
_Py_UOpsAbstractInterpContext_New(PyObject *co_consts,
                                  int stack_len,
                                  int locals_len,
                                  int curr_stacklen,
                                  int ir_entries)
{
    _Py_UOpsAbstractFrame *frame = NULL;
    PyDictObject *sym_exprs_to_sym_exprs = NULL;
    _Py_UOps_Opt_IR *ir = _Py_UOpsSSA_IR_New(ir_entries);
    if (ir == NULL) {
        goto error;
    }


    sym_exprs_to_sym_exprs = (PyDictObject *)PyDict_New();
    if (sym_exprs_to_sym_exprs == NULL) {
        goto error;
    }

    _Py_UOpsAbstractInterpContext *self = PyObject_New(_Py_UOpsAbstractInterpContext,
                                                       &_Py_UOpsAbstractInterpContext_Type);
    if (self == NULL) {
        goto error;
    }

    self->sym_exprs_to_sym_exprs = sym_exprs_to_sym_exprs;
    self->ir = ir;
    self->new_frame_sym = NULL;
    self->frame_entries_needed = 0;
    frame = _Py_UOpsAbstractFrame_New(self, co_consts, stack_len, locals_len, curr_stacklen, NULL);
    self->frame = frame;

    if (frame == NULL) {
        goto error;
    }

    return self;

error:
    Py_XDECREF(ir);
    Py_XDECREF(frame);
    Py_XDECREF(sym_exprs_to_sym_exprs);
    return NULL;
}

static inline _Py_UOpsSymbolicExpression*
sym_init_const(_Py_UOpsAbstractInterpContext *ctx, PyObject *const_val, int const_idx);

static inline PyObject *
create_sym_consts(_Py_UOpsAbstractInterpContext *ctx, PyObject *co_consts)
{
    Py_ssize_t co_const_len = PyTuple_GET_SIZE(co_consts);
    PyObject *sym_consts = PyTuple_New(co_const_len);
    if (sym_consts == NULL) {
        return NULL;
    }
    for (Py_ssize_t i = 0; i < co_const_len; i++) {
        _Py_UOpsSymbolicExpression *res = sym_init_const(ctx, PyTuple_GET_ITEM(co_consts, i), (int)i);
        if (res == NULL) {
            goto error;
        }
        PyTuple_SET_ITEM(sym_consts, i, res);
    }

    return sym_consts;
error:
    Py_DECREF(sym_consts);
    return NULL;
}

static inline _Py_UOpsSymbolicExpression*
sym_init_var(_Py_UOpsAbstractInterpContext *ctx, int locals_idx);

static inline _Py_UOpsSymbolicExpression*
sym_init_unknown(_Py_UOpsAbstractInterpContext *ctx);

static inline _Py_UOpsAbstractFrame *
_Py_UOpsAbstractFrame_New(_Py_UOpsAbstractInterpContext *ctx,
                          PyObject *co_consts, int stack_len, int locals_len,
                          int curr_stacklen, _Py_UOpsOptIREntry *frame_ir_entry)
{
    PyObject *sym_consts = create_sym_consts(ctx, co_consts);
    if (sym_consts == NULL) {
        return NULL;
    }
    int total_len = stack_len + locals_len;
    _Py_UOpsAbstractFrame *frame = PyObject_NewVar(_Py_UOpsAbstractFrame,
                                                      &_Py_UOpsAbstractFrame_Type,
                                                      total_len);
    if (frame == NULL) {
        Py_DECREF(sym_consts);
        return NULL;
    }


    frame->sym_consts = sym_consts;
    frame->stack_len = stack_len;
    frame->locals_len = locals_len;
    frame->prev = NULL;
    frame->next = NULL;

    frame->frame_ir_entry = frame_ir_entry;

    frame->locals = frame->locals_with_stack;
    frame->stack = frame->locals + locals_len;
    frame->stack_pointer = frame->stack + curr_stacklen;

    // Null out everything first
    for (int i = 0; i < total_len; i++) {
        frame->locals_with_stack[i] = NULL;
    }
    // Initialize with the initial state of all local variables
    for (int i = 0; i < locals_len; i++) {
        _Py_UOpsSymbolicExpression *local = sym_init_var(ctx, i);
        if (local == NULL) {
            goto error;
        }
        frame->locals[i] = local;
    }


    // Initialize the stack as well
    for (int i = 0; i < curr_stacklen; i++) {
        // TODO copy over the immutables
        _Py_UOpsSymbolicExpression *stackvar = sym_init_unknown(ctx);
        if (stackvar == NULL) {
            goto error;
        }
        frame->stack[i] = stackvar;
    }
    return frame;

error:
    Py_DECREF(frame);
    return NULL;
}

static inline bool
sym_is_type(_Py_UOpsSymbolicExpression *sym, _Py_UOpsSymExprTypeEnum typ);
static inline uint32_t
sym_type_get_aux(_Py_UOpsSymbolicExpression *sym, _Py_UOpsSymExprTypeEnum typ);

static inline PyFunctionObject *
extract_func_from_sym(_Py_UOpsSymbolicExpression *frame_sym)
{
    switch(frame_sym->inst.opcode) {
        case _INIT_CALL_PY_EXACT_ARGS: {
            _Py_UOpsSymbolicExpression *callable_sym = frame_sym->operands[0];
            if (!sym_is_type(callable_sym, PYFUNCTION_TYPE_VERSION_TYPE)) {
                return NULL;
            }
            int32_t func_version = sym_type_get_aux(callable_sym, PYFUNCTION_TYPE_VERSION_TYPE);
            PyFunctionObject *func = _PyFunction_LookupByVersion(func_version);
            if (func == NULL) {
                return NULL;
            }
            return func;
        }
        default:
            return NULL;
    }
}

static inline _Py_UOpsSymbolicExpression*
extract_self_or_null_from_sym(_Py_UOpsSymbolicExpression *frame_sym)
{
    switch(frame_sym->inst.opcode) {
        case _INIT_CALL_PY_EXACT_ARGS:
            return frame_sym->operands[1];
        default:
            return NULL;
    }
}

static inline _Py_UOpsSymbolicExpression**
extract_args_from_sym(_Py_UOpsSymbolicExpression *frame_sym)
{
    switch(frame_sym->inst.opcode) {
        case _INIT_CALL_PY_EXACT_ARGS:
            return &frame_sym->operands[2];
        default:
            return NULL;
    }
}

// 0 on success, anything else is error.
static int
_Py_UOpsAbstractInterpContext_FramePush(
    _Py_UOpsAbstractInterpContext *ctx,
    _Py_UOpsSymbolicExpression *frame_sym,
    _Py_UOpsOptIREntry *frame_ir_entry
)
{
    assert(frame_sym != NULL);
    // Extract func version from the frame symbolic
    PyFunctionObject *func = extract_func_from_sym(frame_sym);
    if (func == NULL) {
        return -1;
    }
    PyCodeObject *co = (PyCodeObject *)func->func_code;
    _Py_UOpsAbstractFrame *frame = _Py_UOpsAbstractFrame_New(ctx,
        co->co_consts, co->co_stacksize + MAX_FRAME_GROWTH,
        co->co_nlocals,
        0, frame_ir_entry);
    if (frame == NULL) {
        return -1;
    }
    frame->prev = ctx->frame;
    ctx->frame->next = frame;
    ctx->frame = frame;
    frame_ir_entry->frame_co_code = (PyCodeObject*)Py_NewRef(co);

    ctx->frame_entries_needed += (co->co_nlocalsplus + co->co_stacksize);
    assert(ctx->frame_entries_needed >= 0);

    return 0;
}

static int
_Py_UOpsAbstractInterpContext_FramePop(
    _Py_UOpsAbstractInterpContext *ctx
)
{
    _Py_UOpsAbstractFrame *frame = ctx->frame;
    ctx->frame = frame->prev;
    assert(ctx->frame != NULL);
    frame->prev = NULL;
    if (frame->frame_ir_entry != NULL) {
        PyCodeObject *co = frame->frame_ir_entry->frame_co_code;
        ctx->frame_entries_needed -= (co->co_nlocalsplus + co->co_stacksize);
        assert(ctx->frame_entries_needed >= 0);
    }
    Py_DECREF(frame);
    ctx->frame->next = NULL;
    return 0;
}

static _Py_UOpsSymbolicExpression *
check_sym_already_exists(_Py_UOpsAbstractInterpContext *ctx, _Py_UOpsSymbolicExpression *self)
{
    // Unknown opcodes are treated as always unique
    // Guards are always unique.
    if (self->inst.opcode == CACHE || _PyOpcode_isguard[self->inst.opcode]) {
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
// Creates a symbolic expression consisting of subexpressoins
// from arr_start and va_list.
// The order is
// <va_list elements left to right>, <arr_start elements left to right>
static _Py_UOpsSymbolicExpression*
_Py_UOpsSymbolicExpression_New(_Py_UOpsAbstractInterpContext *ctx,
                               _PyUOpInstruction inst,
                               PyObject *const_val,
                               int num_arr,
                               _Py_UOpsSymbolicExpression **arr_start,
                               int num_subexprs, ...)
{
    int total_subexprs = num_arr + num_subexprs;

    _Py_UOpsSymbolicExpression *self = PyObject_NewVar(_Py_UOpsSymbolicExpression,
                                                          &_Py_UOpsSymbolicExpression_Type,
                                                          total_subexprs);
    if (self == NULL) {
        return NULL;
    }


    self->idx = -1;
    self->cached_hash = -1;
    self->usage_count = 1;
    self->emitted_count = 0;
    self->sym_type.types = 0;
    self->inst = inst;
    self->const_val = const_val;
    self->originating_region = ctx->curr_region_id;

    assert(Py_SIZE(self) >= num_subexprs);

    // Setup
    int i = 0;
    _Py_UOpsSymbolicExpression **operands = self->operands;
    va_list curr;

    va_start(curr, num_subexprs);

    for (; i < num_subexprs; i++) {
        // Note: no incref here. symexprs are kept alive by the global expression
        // table.
        // We intentionally don't want to hold a reference to it so we don't
        // need GC.
        operands[i] = va_arg(curr, _Py_UOpsSymbolicExpression *);
        assert(operands[i]);
    }

    va_end(curr);

    for (int x = 0; x < num_arr; x++) {
        operands[i+x] = arr_start[x];
        assert(operands[i+x]);
    }

    for (int i = 0; i < total_subexprs; i++) {
        if (!_PyOpcode_isguard[inst.opcode]) {
            operands[i]->usage_count++;
        }
    }

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
    self->sym_type.types = 0;
    self->inst = inst;
    self->const_val = NULL;
    self->originating_region = -1;

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
sym_copy_type(_Py_UOpsSymbolicExpression *from_sym, _Py_UOpsSymbolicExpression *to_sym)
{
    to_sym->sym_type = from_sym->sym_type;
    if (from_sym->const_val != NULL) {
        to_sym->const_val = Py_NewRef(from_sym->const_val);
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
                                          NULL,
                                          0,
                                          NULL,
                                          0);
}

static inline _Py_UOpsSymbolicExpression*
sym_init_unknown(_Py_UOpsAbstractInterpContext *ctx)
{
    _PyUOpInstruction inst = {CACHE, 0, 0, 0};
    return _Py_UOpsSymbolicExpression_New(ctx,
                                          inst,
                                          NULL,
                                          0,
                                          NULL,
                                          0);
}

static inline _Py_UOpsSymbolicExpression*
sym_init_const(_Py_UOpsAbstractInterpContext *ctx, PyObject *const_val, int const_idx)
{
    _PyUOpInstruction inst = {LOAD_CONST, const_idx, 0, 0};
    _Py_UOpsSymbolicExpression *temp = _Py_UOpsSymbolicExpression_New(
        ctx,
        inst,
        const_val,
        0,
        NULL,
        0
    );
    if (temp == NULL) {
        return NULL;
    }
    sym_set_type_from_const(temp, const_val);
    return temp;
}


static inline bool
sym_is_type(_Py_UOpsSymbolicExpression *sym, _Py_UOpsSymExprTypeEnum typ)
{
    if ((sym->sym_type.types & (1 << typ)) == 0) {
        return false;
    }
    return true;
}

static inline bool
sym_matches_type(_Py_UOpsSymbolicExpression *sym, _Py_UOpsSymExprTypeEnum typ, uint32_t aux)
{
    if (!sym_is_type(sym, typ)) {
        return false;
    }
    if (typ <= MAX_TYPE_WITH_AUX) {
        return sym->sym_type.aux[typ] == aux;
    }
    return true;
}

static inline uint32_t
sym_type_get_aux(_Py_UOpsSymbolicExpression *sym, _Py_UOpsSymExprTypeEnum typ)
{
    assert(sym_is_type(sym, typ));
    assert(typ <= MAX_TYPE_WITH_AUX);
    return sym->sym_type.aux[typ];
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


static bool
inst_is_equal(_PyUOpInstruction lhs, _PyUOpInstruction rhs)
{
    return (lhs.opcode == rhs.opcode) && (lhs.oparg) == (rhs.oparg) && (lhs.operand == rhs.operand);
}

static int
write_stack_to_ir(_Py_UOpsAbstractInterpContext *ctx, _PyUOpInstruction *curr, bool copy_types) {
    // Emit the state of the stack first.
    int stack_entries = ctx->frame->stack_pointer - ctx->frame->stack;
    for (int i = 0; i < stack_entries; i++) {
        // Don't compile unknown stack entries.
        ir_store(ctx->ir, ctx->frame->stack[i], TARGET_NONE);
        _Py_UOpsSymbolicExpression *new_stack = sym_init_unknown(ctx);
        if (new_stack == NULL) {
            goto error;
        }
        if (copy_types) {
            sym_copy_type(ctx->frame->stack[i], new_stack);
        }
        ctx->frame->stack[i] = new_stack;
    }
    // Write bookkeeping ops, but don't write duplicates.
    if((curr-1)->opcode == _CHECK_VALIDITY && (curr-2)->opcode == _SET_IP) {
        ir_plain_inst(ctx->ir, *(curr-2));
        ir_plain_inst(ctx->ir, *(curr-1));
    }
    return 0;

error:
    return -1;
}

typedef enum {
    ABSTRACT_INTERP_ERROR,
    ABSTRACT_INTERP_NORMAL,
    ABSTRACT_INTERP_GUARD_REQUIRED,
} AbstractInterpExitCodes;


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
    _Py_UOpsAbstractInterpContext *ctx
)
{
#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif

#define STACK_LEVEL()     ((int)(stack_pointer - ctx->frame->stack))
#define STACK_SIZE()      (co->co_stacksize + MAX_FRAME_GROWTH)
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
#define GETLOCAL(idx)          ((ctx->frame->locals[idx]))

#define CURRENT_OPARG() (oparg)

#define CURRENT_OPERAND() (operand)

#define STAT_INC(opname, name) ((void)0)
#define TIER_TWO_ONLY ((void)0)

    int oparg = inst->oparg;
    uint32_t opcode = inst->opcode;
    uint64_t operand = inst->operand;

    _Py_UOpsSymbolicExpression **stack_pointer = ctx->frame->stack_pointer;


    DPRINTF(2, "Abstract interpreting %s:%d\n",
            (opcode >= 300 ? _PyOpcode_uop_name : _PyOpcode_OpName)[opcode],
            oparg);
    switch (opcode) {
#include "abstract_interp_cases.c.h"
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
            PEEK(1) = (_Py_UOpsSymbolicExpression *)GETITEM(
                ctx->frame->sym_consts, oparg);
            break;
        }
        case STORE_FAST_MAYBE_NULL:
        case STORE_FAST: {
            _Py_UOpsSymbolicExpression *value = PEEK(1);
            ir_store(ctx->ir, value, oparg);
            _Py_UOpsSymbolicExpression *new_local = sym_init_var(ctx, oparg);
            if (new_local == NULL) {
                goto error;
            }
            sym_copy_type(value, new_local);
            GETLOCAL(oparg) = new_local;
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
            ir_store(ctx->ir, PEEK(1), -1);
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

        case _PUSH_FRAME: {
            _Py_UOpsAbstractFrame *old_frame = ctx->frame;
            // TOS is the new frame.
            write_stack_to_ir(ctx, inst, true);
            _Py_UOpsSymbolicExpression *frame_sym = PEEK(1);
            STACK_SHRINK(1);
            ctx->frame->stack_pointer = stack_pointer;
            _Py_UOpsOptIREntry *frame_ir_entry = ir_frame_push_info(ctx->ir);
            ir_plain_inst(ctx->ir, *inst);
            if (_Py_UOpsAbstractInterpContext_FramePush(ctx, ctx->new_frame_sym, frame_ir_entry) != 0){
                goto error;
            }
            stack_pointer = ctx->frame->stack_pointer;
            _Py_UOpsSymbolicExpression *self_or_null = extract_self_or_null_from_sym(ctx->new_frame_sym);
            assert(self_or_null != NULL);
            assert(ctx->new_frame_sym != NULL);
            _Py_UOpsSymbolicExpression **args = extract_args_from_sym(ctx->new_frame_sym);
            assert(args != NULL);
            ctx->new_frame_sym = NULL;
            int argcount = oparg;
            // Bound method fiddling, same as _INIT_CALL_PY_EXACT_ARGS
            if (!sym_is_type(self_or_null, NULL_TYPE)) {
                args--;
                argcount++;
                ctx->new_frame_localsplus--;
                frame_ir_entry->consumed_self = true;
            }
            for (int i = 0; i < argcount; i++) {
                sym_copy_type(args[i], ctx->frame->locals[i]);
            }
            frame_decide_inlineable(ctx);
            // If the frame is inlined, we
            // don't need to copy over locals, instead interleave the frames.
            // The old stack entries become the locals of the new frame.
            // The new frame steals references to the stack entries.
            frame_ir_entry->localsplus_offset = abstractframe_get_localsplus_index(old_frame, ctx->new_frame_localsplus);
            ctx->new_frame_localsplus = NULL;
            break;
        }

        case _POP_FRAME: {
            write_stack_to_ir(ctx, inst, true);
            _Py_UOpsOptIREntry *frame_ir_entry = ctx->frame->frame_ir_entry;
            ir_frame_pop_info(ctx->ir);
            ir_plain_inst(ctx->ir, *inst);
            _Py_UOpsSymbolicExpression *retval = PEEK(1);
            STACK_SHRINK(1);
            ctx->frame->stack_pointer = stack_pointer;

            // Pop old frame.
            if (_Py_UOpsAbstractInterpContext_FramePop(ctx) != 0){
                goto error;
            }
            stack_pointer = ctx->frame->stack_pointer;
            // Push retval into new frame.
            STACK_GROW(1);
            sym_copy_type(retval, PEEK(1));
            break;
        }

        case SWAP: {
            write_stack_to_ir(ctx, inst, true);
            ir_plain_inst(ctx->ir, *inst);

            _Py_UOpsSymbolicExpression *top;
            _Py_UOpsSymbolicExpression *bottom;
            top = stack_pointer[-1];
            bottom = stack_pointer[-2 - (oparg-2)];
            assert(oparg >= 2);

            _Py_UOpsSymbolicExpression *new_top = sym_init_unknown(ctx);
            if (new_top == NULL) {
                goto error;
            }
            sym_copy_type(top, new_top);

            _Py_UOpsSymbolicExpression *new_bottom = sym_init_unknown(ctx);
            if (new_bottom == NULL) {
                goto error;
            }
            sym_copy_type(bottom, new_bottom);

            stack_pointer[-2 - (oparg-2)] = new_top;
            stack_pointer[-1] = new_bottom;
            break;
        }
        default:
            DPRINTF(1, "Unknown opcode in abstract interpreter\n");
            Py_UNREACHABLE();
    }

    // Store the frame symbolic to extract information later
    if (opcode == _INIT_CALL_PY_EXACT_ARGS) {
        ctx->new_frame_sym = PEEK(1);
        // All the operands would have existed before this new frame sym
        // Warning: tied to _INIT_CALL_PY_EXACT_ARGS implementation!
        ctx->new_frame_localsplus = &PEEK(-(oparg));
    }
    DPRINTF(2, "stack_pointer %p\n", stack_pointer);
    ctx->frame->stack_pointer = stack_pointer;
    assert(STACK_LEVEL() >= 0);

    return ABSTRACT_INTERP_NORMAL;

pop_2_error_tier_two:
    STACK_SHRINK(1);
    STACK_SHRINK(1);
error:
    DPRINTF(1, "Encountered error in abstract interpreter\n");
    return ABSTRACT_INTERP_ERROR;

guard_required:
    assert(_PyOpcode_isguard[opcode]);
required:
    DPRINTF(2, "stack_pointer %p\n", stack_pointer);
    ctx->frame->stack_pointer = stack_pointer;
    assert(STACK_LEVEL() >= 0);

    return ABSTRACT_INTERP_GUARD_REQUIRED;

}

static _Py_UOpsAbstractInterpContext *
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

    ctx = _Py_UOpsAbstractInterpContext_New(
        co->co_consts, co->co_stacksize + MAX_FRAME_GROWTH,
        co->co_nlocals, curr_stacklen,
        trace_len);
    if (ctx == NULL) {
        goto error;
    }
    ctx->sym_curr_id = 0;
    ctx->curr_region_id = 0;

    _PyUOpInstruction *curr = trace;
    _PyUOpInstruction *end = trace + trace_len;
    AbstractInterpExitCodes status = ABSTRACT_INTERP_NORMAL;

    bool first_impure = true;
    while (curr < end && !op_is_end(curr->opcode)) {

        if (!_PyOpcode_ispure[curr->opcode] &&
            !is_bookkeeping_opcode(curr->opcode) &&
            !_PyOpcode_isguard[(curr)->opcode]) {
            DPRINTF(2, "Impure\n");
            if (first_impure) {
                write_stack_to_ir(ctx, curr, false);
            }
            first_impure = false;
            ctx->curr_region_id++;
            ir_plain_inst(ctx->ir, *curr);
        }
        else {
            first_impure = true;
        }

        status = uop_abstract_interpret_single_inst(
            co, curr, ctx
        );
        if (status == ABSTRACT_INTERP_ERROR) {
            goto error;
        }
        else if (status == ABSTRACT_INTERP_GUARD_REQUIRED) {
            DPRINTF(2, "GUARD\n");
            // Emit the state of the stack first.
            // Since this is a guard, copy over the type info
            write_stack_to_ir(ctx, curr, true);
            if((curr-1)->opcode == _CHECK_VALIDITY && (curr-2)->opcode == _SET_IP) {
                ir_plain_inst(ctx->ir, *(curr-2));
                ir_plain_inst(ctx->ir, *(curr-1));
            }
            ir_plain_inst(ctx->ir, *curr);
        }

        curr++;

    }

    ctx->terminating = curr;
    write_stack_to_ir(ctx, curr, false);
    // Frames that are dangling at the end should never be inlined.
    frame_propagate_not_inlineable(ctx);

    return ctx;

error:
    if(PyErr_Occurred()) {
        PyErr_Clear();
    }
    return NULL;
}

typedef struct _Py_UOpsEmitter {
    _PyUOpInstruction *writebuffer;
    _PyUOpInstruction *writebuffer_end;
    int curr_i;

    // A dict mapping the common expressions to the slots indexes.
    PyObject *common_syms;

    int consumed_localsplus_slots;
    _Py_UOpsOptIREntry *curr_frame_ir_entry;
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
                    _Py_UOpsAbstractInterpContext *ctx,
                    bool do_cse);

// Find a slot to store the result of a common subexpression.
//static int
//compile_common_sym(_Py_UOpsEmitter *emitter,
//            _Py_UOpsSymbolicExpression *sym,
//           _Py_UOpsAbstractInterpContext *ctx)
//{
//    sym->emitted_count++;
//
//    PyObject *idx = PyDict_GetItem(emitter->common_syms, (PyObject *)sym);
//    // Present - just use that
//    if (idx != NULL) {
//        long index = PyLong_AsLong(idx);
//        assert(!PyErr_Occurred());
//        PyObject **addr = emitter->scratch_start - index;
//        _PyUOpInstruction load_common = {_LOAD_COMMON, 0, 0, (uint64_t)addr};
//        if(emit_i(emitter, load_common) < 0) {
//            return -1;
//        }
//        return 0;
//    }
//
//    // Not present, emit the whole thing, then store that
//    if (compile_sym_to_uops(emitter, sym, ctx, false) < 0) {
//        return -1;
//    }
//
//    // Not really used - not worth to store it.
//    if (!(sym->usage_count > 1 && sym->emitted_count < (sym->usage_count))){
//        return 0;
//    }
//    // No space left, TODO evict something based on usage count.
//    // And store there.
//    if (emitter->curr_scratch_id >= emitter->max_scratch_slots) {
//        return 0;
//    }
//
//    // If there's space, expand the scratch slot.
//    emitter->scratch_end--;
//    // Grow backwards.
//    while ((char *)emitter->writebuffer_end > (char *)emitter->scratch_end) {
//        emitter->writebuffer_end--;
//    }
//    emitter->n_scratch_slots++;
//    PyObject **available = emitter->scratch_available;
//    assert(available >= emitter->scratch_end);
//    emitter->scratch_available--;
//
//    *available = NULL;
//    // TODO memory leak
//    _PyUOpInstruction store_common = {_STORE_COMMON, 0, 0, (uint64_t)available};
//    if(emit_i(emitter, store_common) < 0) {
//        return -1;
//    }
//    long index = (long)(emitter->scratch_start - available);
//    assert(index >= 0);
//    idx = PyLong_FromLong(index);
//    if (idx == NULL) {
//        return -1;
//    }
//    if (PyDict_SetItem(emitter->common_syms, (PyObject *)sym, idx) < 0) {
//        PyErr_Clear();
//        Py_DECREF(idx);
//        return -1;
//    }
//    Py_DECREF(idx);
//    return 0;
//}

static int
compile_sym_to_uops(_Py_UOpsEmitter *emitter,
                   _Py_UOpsSymbolicExpression *sym,
                    _Py_UOpsAbstractInterpContext *ctx,
                   bool do_cse)
{
    _PyUOpInstruction inst;
    // Since CPython is a stack machine, just compile in the order
    // seen in the operands, then the instruction itself.

    // Constant propagated value, load immediate constant
    if (sym->const_val != NULL && !_PyOpcode_isunknown(sym->inst.opcode)) {
        inst.opcode = _LOAD_CONST_IMMEDIATE;
        inst.oparg = 0;
        // TODO memory leak.
        inst.operand = (uint64_t)Py_NewRef(sym->const_val);
        return emit_i(emitter, inst);
    }


    // Common subexpression elimination.
    // Disabled, fix later.
//    if (do_cse && !_PyOpcode_isterminal(sym->inst.opcode) && sym->usage_count > 1) {
//        return compile_common_sym(emitter, sym, ctx);
//    }


    if (_PyOpcode_isterminal(sym->inst.opcode)) {
        // These are for unknown stack entries.
        if (_PyOpcode_isunknown(sym->inst.opcode)) {
            // Leave it be. These are initial values from the start
            return 0;
        }
        inst = sym->inst;
        inst.opcode = sym->inst.opcode == INIT_FAST ? LOAD_FAST : sym->inst.opcode;
        inst.oparg = sym->inst.opcode == INIT_FAST ?
            ir_entry_calculate_localsplus_offset(emitter->curr_frame_ir_entry, sym->inst.oparg)
            : sym->inst.oparg;
        return emit_i(emitter, inst);
    }

    // Compile each operand
    Py_ssize_t operands_count = Py_SIZE(sym);
    for (Py_ssize_t i = 0; i < operands_count; i++) {
        if (sym->operands[i] == NULL) {
            continue;
        }
        // TODO Py_EnterRecursiveCall ?
        if (compile_sym_to_uops(
            emitter,
            sym->operands[i],
            ctx, true) < 0) {
            return -1;
        }
    }


    // Finally, emit the operation itself.
    return emit_i(emitter, sym->inst);
}



static int
emit_uops_from_ctx(
    _Py_UOpsAbstractInterpContext *ctx,
    _PyUOpInstruction *trace_writebuffer,
    _PyUOpInstruction *writebuffer_end
)
{

#ifdef Py_DEBUG
    char *uop_debug = Py_GETENV("PYTHONUOPSDEBUG");
    int lltrace = 0;
    if (uop_debug != NULL && *uop_debug >= '0') {
        lltrace = *uop_debug - '0';  // TODO: Parse an int and all that
    }
#endif

    PyObject *sym_store = PyDict_New();
    if (sym_store == NULL) {
        return -1;
    }

    // The calculation for this overestimates it abit as it assumes the stack
    // is completely full. In reality, we can use some stack space too.
    // TODO more accurate calculation of interleaved locals and stack.
    int max_additional_nlocals_needed = 0;
    int curr_additional_nlocals_needed = 0;

    _Py_UOpsEmitter emitter = {
        trace_writebuffer,
        writebuffer_end,
        1, // Reserve 1 slot for _SETUP_TIER2_FRAME if needed.
        sym_store,
        0,
        ctx->frame->frame_ir_entry
    };

    _Py_UOps_Opt_IR *ir = ctx->ir;
    int entries = ir->curr_write;
    bool did_inline = false;
    for (int i = 0; i < entries; i++) {
        _Py_UOpsOptIREntry curr = ir->entries[i];
        switch (curr.typ) {
            case IR_SYMBOLIC: {
                if (compile_sym_to_uops(&emitter, curr.expr, ctx, true) < 0) {
                    goto error;
                }
                // Anything less means no assignment target at all.
                if (curr.assignment_target >= TARGET_UNUSED) {
                    _PyUOpInstruction inst = {
                        curr.assignment_target == TARGET_UNUSED
                        ? POP_TOP : STORE_FAST,
                        curr.assignment_target, 0, 0};
                    if (emit_i(&emitter, inst) < 0) {
                        goto error;
                    }
                }
                break;
            }
            case IR_PLAIN_INST: {
                if (emit_i(&emitter, curr.inst) < 0) {
                    goto error;
                }
                break;
            }
            case IR_FRAME_PUSH_INFO: {
                _Py_UOpsOptIREntry *prev = emitter.curr_frame_ir_entry;
                emitter.curr_frame_ir_entry = &ir->entries[i];
                emitter.curr_frame_ir_entry->prev_frame_ir = prev;
                if (!curr.is_inlineable) {
                    break;
                }
                DPRINTF(3, "inlining frame at IR location %d, and interleaving locals\n", i);
                did_inline = true;
                curr_additional_nlocals_needed += (curr.frame_co_code->co_nlocalsplus + curr.frame_co_code->co_stacksize);
                max_additional_nlocals_needed = curr_additional_nlocals_needed > max_additional_nlocals_needed
                    ? curr_additional_nlocals_needed : max_additional_nlocals_needed;
                assert(emitter.writebuffer[emitter.curr_i - 1].opcode == _SAVE_RETURN_OFFSET);
                assert(emitter.writebuffer[emitter.curr_i - 2].opcode == _INIT_CALL_PY_EXACT_ARGS);
                assert(emitter.writebuffer[emitter.curr_i - 3].opcode == _CHECK_STACK_SPACE);
//                int num_shrink = emitter.writebuffer[emitter.curr_i - 2].oparg + 2;
                emitter.curr_i -= 3;
                int nargs = emitter.writebuffer[emitter.curr_i - 2].oparg;
                // Expand the stack to AFTER our new interleaved locals.
                _PyUOpInstruction new_sp = {_PRE_INLINE,
                                            emitter.curr_frame_ir_entry->frame_co_code->co_nlocalsplus -
                                                nargs -
                                                (emitter.curr_frame_ir_entry->consumed_self ? 1 : 0),
                                            0, 0};
                emit_i(&emitter, new_sp);
                assert((ir->entries[i+1].typ == IR_PLAIN_INST && ir->entries[i+1].inst.opcode == _PUSH_FRAME));
                // Skip _PUSH_FRAME.
                i++;
                // Skip _RESUME_CHECK
                for (int x = 0; x < 8; x++) {
                    if (ir->entries[i+x].typ == IR_PLAIN_INST && ir->entries[i+x].inst.opcode == _RESUME_CHECK) {
                        ir->entries[i+x].typ = IR_NOP;
                        break;
                    }
                }
                break;
            }
            case IR_FRAME_POP_INFO: {
                if (emitter.curr_frame_ir_entry->is_inlineable) {
                    assert((ir->entries[i+1].typ == IR_PLAIN_INST && ir->entries[i+1].inst.opcode == _POP_FRAME));
                    PyCodeObject *co = emitter.curr_frame_ir_entry->frame_co_code;
                    curr_additional_nlocals_needed -= (co->co_nlocalsplus + co->co_stacksize);
                    // Skip _POP_FRAME
                    i++;
                    // Cleanup the stack.
                    _PyUOpInstruction new_sp = {_POST_INLINE,
                        emitter.curr_frame_ir_entry->frame_co_code->co_nlocalsplus +
                        // 1 for self, 1 for callable, this aligns with _CALL_PY_EXACT_ARGS
                        (emitter.curr_frame_ir_entry->consumed_self ? 0 : 1) + 1,
                        0, 0};
                    emit_i(&emitter, new_sp);
                }
                _Py_UOpsOptIREntry *prev = emitter.curr_frame_ir_entry->prev_frame_ir;
                emitter.curr_frame_ir_entry->prev_frame_ir = NULL;
                emitter.curr_frame_ir_entry = prev;
                break;
            }
            case IR_NOP: break;
        }
    }

    Py_DECREF(sym_store);

    if (did_inline) {
        DPRINTF(3, "Emitting instruction at [%d] op: %s, oparg: %d, operand: %" PRIu64 " \n",
                0,
                "_SETUP_TIER2_FRAME",
                max_additional_nlocals_needed,
                0L);
        _PyUOpInstruction setup_frame = {_SETUP_TIER2_FRAME, max_additional_nlocals_needed, 0, 0};
        emitter.writebuffer[0] = setup_frame;
    }
    else {
        _PyUOpInstruction nop = {_NOP, 0, 0, 0};
        emitter.writebuffer[0] = nop;
    }
    // Add the _JUMP_TO_TOP/_EXIT_TRACE at the end of the trace.
    _PyUOpInstruction jump_absolute = {
        _JUMP_ABSOLUTE,
        did_inline,
        0
    };
    _PyUOpInstruction terminal = ctx->terminating->opcode == _EXIT_TRACE ?
                                 *ctx->terminating : jump_absolute;
    if (emit_i(&emitter, terminal) < 0) {
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
    _PyUOpInstruction *temp_writebuffer = NULL;

    temp_writebuffer = PyMem_New(_PyUOpInstruction, buffer_size * OVERALLOCATE_FACTOR);
    if (temp_writebuffer == NULL) {
        goto error;
    }


    // Pass: Abstract interpretation and symbolic analysis
    _Py_UOpsAbstractInterpContext *ctx = uop_abstract_interpret(
        co, buffer,
        buffer_size, curr_stacklen);

    if (ctx == NULL) {
        goto error;
    }

    _PyUOpInstruction *writebuffer_end = temp_writebuffer + buffer_size;
    // Compile the SSA IR
    int trace_len = emit_uops_from_ctx(
        ctx,
        temp_writebuffer,
        writebuffer_end
    );
    if (trace_len < 0 || trace_len > buffer_size) {
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
