
        /* Cannot outline _NOP. */

        /* Cannot outline _CHECK_PERIODIC. */

        /* Cannot outline _CHECK_PERIODIC_IF_NOT_YIELD_FROM. */

        /* _QUICKEN_RESUME is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* _LOAD_BYTECODE is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _RESUME_CHECK. */

        /* _MONITOR_RESUME is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _LOAD_FAST_CHECK. */

        /* Cannot outline _LOAD_FAST_0. */

        /* Cannot outline _LOAD_FAST_1. */

        /* Cannot outline _LOAD_FAST_2. */

        /* Cannot outline _LOAD_FAST_3. */

        /* Cannot outline _LOAD_FAST_4. */

        /* Cannot outline _LOAD_FAST_5. */

        /* Cannot outline _LOAD_FAST_6. */

        /* Cannot outline _LOAD_FAST_7. */

        /* Cannot outline _LOAD_FAST. */

        /* Cannot outline _LOAD_FAST_BORROW_0. */

        /* Cannot outline _LOAD_FAST_BORROW_1. */

        /* Cannot outline _LOAD_FAST_BORROW_2. */

        /* Cannot outline _LOAD_FAST_BORROW_3. */

        /* Cannot outline _LOAD_FAST_BORROW_4. */

        /* Cannot outline _LOAD_FAST_BORROW_5. */

        /* Cannot outline _LOAD_FAST_BORROW_6. */

        /* Cannot outline _LOAD_FAST_BORROW_7. */

        /* Cannot outline _LOAD_FAST_BORROW. */

        /* Cannot outline _LOAD_FAST_AND_CLEAR. */

        /* _LOAD_CONST is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _LOAD_CONST_MORTAL. */

        /* Cannot outline _LOAD_CONST_IMMORTAL. */

        /* Cannot outline _LOAD_SMALL_INT_0. */

        /* Cannot outline _LOAD_SMALL_INT_1. */

        /* Cannot outline _LOAD_SMALL_INT_2. */

        /* Cannot outline _LOAD_SMALL_INT_3. */

        /* Cannot outline _LOAD_SMALL_INT. */

        /* Cannot outline _STORE_FAST_0. */

        /* Cannot outline _STORE_FAST_1. */

        /* Cannot outline _STORE_FAST_2. */

        /* Cannot outline _STORE_FAST_3. */

        /* Cannot outline _STORE_FAST_4. */

        /* Cannot outline _STORE_FAST_5. */

        /* Cannot outline _STORE_FAST_6. */

        /* Cannot outline _STORE_FAST_7. */

        /* Cannot outline _STORE_FAST. */

        /* Cannot outline _POP_TOP. */

        /* Cannot outline _PUSH_NULL. */

        /* Cannot outline _END_FOR. */

        /* Cannot outline _END_SEND. */

        /* Cannot outline _UNARY_NEGATIVE. */

        /* Cannot outline _UNARY_NOT. */

        /* Cannot outline _TO_BOOL. */

        /* Cannot outline _TO_BOOL_BOOL. */

        /* Cannot outline _TO_BOOL_INT. */

        /* Cannot outline _GUARD_NOS_LIST. */

        /* Cannot outline _GUARD_TOS_LIST. */

        /* Cannot outline _GUARD_TOS_SLICE. */

        /* Cannot outline _TO_BOOL_LIST. */

        /* Cannot outline _TO_BOOL_NONE. */

        /* Cannot outline _GUARD_NOS_UNICODE. */

        /* Cannot outline _GUARD_TOS_UNICODE. */

        /* Cannot outline _TO_BOOL_STR. */

        /* Cannot outline _REPLACE_WITH_TRUE. */

        /* Cannot outline _UNARY_INVERT. */

        /* Cannot outline _GUARD_NOS_INT. */

        /* Cannot outline _GUARD_TOS_INT. */

        /* Cannot outline _BINARY_OP_MULTIPLY_INT. */

        /* Cannot outline _BINARY_OP_ADD_INT. */

        /* Cannot outline _BINARY_OP_SUBTRACT_INT. */

        /* Cannot outline _GUARD_NOS_FLOAT. */

        /* Cannot outline _GUARD_TOS_FLOAT. */

        /* Cannot outline _BINARY_OP_MULTIPLY_FLOAT. */

        /* Cannot outline _BINARY_OP_ADD_FLOAT. */

        /* Cannot outline _BINARY_OP_SUBTRACT_FLOAT. */

        /* Cannot outline _BINARY_OP_ADD_UNICODE. */

        /* Cannot outline _BINARY_OP_INPLACE_ADD_UNICODE. */

        /* Cannot outline _GUARD_BINARY_OP_EXTEND. */

        extern _JITOutlinedReturnVal _BINARY_OP_EXTEND_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *descr = (PyObject *)CURRENT_OPERAND0();
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(INLINE_CACHE_ENTRIES_BINARY_OP == 5);
            _PyBinaryOpSpecializationDescr *d = (_PyBinaryOpSpecializationDescr*)descr;
            STAT_INC(BINARY_OP, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = d->action(left_o, right_o);
            _PyStackRef tmp = right;
            right = PyStackRef_NULL;
            stack_pointer[-1] = right;
            PyStackRef_CLOSE(tmp);
            tmp = left;
            left = PyStackRef_NULL;
            stack_pointer[-2] = left;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _BINARY_SLICE. */

        /* Cannot outline _STORE_SLICE. */

        /* Cannot outline _BINARY_OP_SUBSCR_LIST_INT. */

        /* Cannot outline _BINARY_OP_SUBSCR_LIST_SLICE. */

        /* Cannot outline _BINARY_OP_SUBSCR_STR_INT. */

        /* Cannot outline _GUARD_NOS_TUPLE. */

        /* Cannot outline _GUARD_TOS_TUPLE. */

        /* Cannot outline _BINARY_OP_SUBSCR_TUPLE_INT. */

        /* Cannot outline _GUARD_NOS_DICT. */

        /* Cannot outline _GUARD_TOS_DICT. */

        /* Cannot outline _BINARY_OP_SUBSCR_DICT. */

        /* Cannot outline _BINARY_OP_SUBSCR_CHECK_FUNC. */

        /* Cannot outline _BINARY_OP_SUBSCR_INIT_CALL. */

        /* Cannot outline _LIST_APPEND. */

        /* Cannot outline _SET_ADD. */

        /* Cannot outline _STORE_SUBSCR. */

        /* Cannot outline _STORE_SUBSCR_LIST_INT. */

        /* Cannot outline _STORE_SUBSCR_DICT. */

        /* Cannot outline _DELETE_SUBSCR. */

        /* Cannot outline _CALL_INTRINSIC_1. */

        /* Cannot outline _CALL_INTRINSIC_2. */

        extern _JITOutlinedReturnVal _RETURN_VALUE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef retval;
            _PyStackRef res;
            retval = stack_pointer[-1];
            assert(frame->owner != FRAME_OWNED_BY_INTERPRETER);
            _PyStackRef temp = PyStackRef_MakeHeapSafe(retval);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            assert(STACK_LEVEL() == 0);
            _Py_LeaveRecursiveCallPy(tstate);
            _PyInterpreterFrame *dying = frame;
            frame = tstate->current_frame = dying->previous;
            _PyEval_FrameClearAndPop(tstate, dying);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            LOAD_IP(frame->return_offset);
            res = temp;
            LLTRACE_RESUME_FRAME();
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _GET_AITER. */

        /* Cannot outline _GET_ANEXT. */

        /* Cannot outline _GET_AWAITABLE. */

        /* _SEND is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _SEND_GEN_FRAME. */

        extern _JITOutlinedReturnVal _YIELD_VALUE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef retval;
            _PyStackRef value;
            oparg = CURRENT_OPARG();
            retval = stack_pointer[-1];
            assert(frame->owner != FRAME_OWNED_BY_INTERPRETER);
            frame->instr_ptr++;
            PyGenObject *gen = _PyGen_GetGeneratorFromFrame(frame);
            assert(FRAME_SUSPENDED_YIELD_FROM == FRAME_SUSPENDED + 1);
            assert(oparg == 0 || oparg == 1);
            gen->gi_frame_state = FRAME_SUSPENDED + oparg;
            _PyStackRef temp = retval;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            tstate->exc_info = gen->gi_exc_state.previous_item;
            gen->gi_exc_state.previous_item = NULL;
            _Py_LeaveRecursiveCallPy(tstate);
            _PyInterpreterFrame *gen_frame = frame;
            frame = tstate->current_frame = frame->previous;
            gen_frame->previous = NULL;
            assert(INLINE_CACHE_ENTRIES_SEND == INLINE_CACHE_ENTRIES_FOR_ITER);
            #if TIER_ONE
            assert(frame->instr_ptr->op.code == INSTRUMENTED_LINE ||
                  frame->instr_ptr->op.code == INSTRUMENTED_INSTRUCTION ||
                  _PyOpcode_Deopt[frame->instr_ptr->op.code] == SEND ||
                  _PyOpcode_Deopt[frame->instr_ptr->op.code] == FOR_ITER ||
                  _PyOpcode_Deopt[frame->instr_ptr->op.code] == INTERPRETER_EXIT ||
                  _PyOpcode_Deopt[frame->instr_ptr->op.code] == ENTER_EXECUTOR);
            #endif
            stack_pointer = _PyFrame_GetStackPointer(frame);
            LOAD_IP(1 + INLINE_CACHE_ENTRIES_SEND);
            value = PyStackRef_MakeHeapSafe(temp);
            LLTRACE_RESUME_FRAME();
            stack_pointer[0] = value;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _POP_EXCEPT. */

        /* Cannot outline _LOAD_COMMON_CONSTANT. */

        /* Cannot outline _LOAD_BUILD_CLASS. */

        /* Cannot outline _STORE_NAME. */

        /* Cannot outline _DELETE_NAME. */

        /* Cannot outline _UNPACK_SEQUENCE. */

        /* Cannot outline _UNPACK_SEQUENCE_TWO_TUPLE. */

        /* Cannot outline _UNPACK_SEQUENCE_TUPLE. */

        /* Cannot outline _UNPACK_SEQUENCE_LIST. */

        /* Cannot outline _UNPACK_EX. */

        /* Cannot outline _STORE_ATTR. */

        /* Cannot outline _DELETE_ATTR. */

        /* Cannot outline _STORE_GLOBAL. */

        /* Cannot outline _DELETE_GLOBAL. */

        /* Cannot outline _LOAD_LOCALS. */

        /* _LOAD_FROM_DICT_OR_GLOBALS is not a viable micro-op for tier 2 because it has both popping and not-popping errors */

        /* Cannot outline _LOAD_NAME. */

        /* Cannot outline _LOAD_GLOBAL. */

        /* Cannot outline _PUSH_NULL_CONDITIONAL. */

        /* Cannot outline _GUARD_GLOBALS_VERSION. */

        /* Cannot outline _LOAD_GLOBAL_MODULE. */

        /* Cannot outline _LOAD_GLOBAL_BUILTINS. */

        /* Cannot outline _DELETE_FAST. */

        /* Cannot outline _MAKE_CELL. */

        /* Cannot outline _DELETE_DEREF. */

        /* Cannot outline _LOAD_FROM_DICT_OR_DEREF. */

        /* Cannot outline _LOAD_DEREF. */

        /* Cannot outline _STORE_DEREF. */

        extern _JITOutlinedReturnVal _COPY_FREE_VARS_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            oparg = CURRENT_OPARG();
            PyCodeObject *co = _PyFrame_GetCode(frame);
            assert(PyStackRef_FunctionCheck(frame->f_funcobj));
            PyFunctionObject *func = (PyFunctionObject *)PyStackRef_AsPyObjectBorrow(frame->f_funcobj);
            PyObject *closure = func->func_closure;
            assert(oparg == co->co_nfreevars);
            int offset = co->co_nlocalsplus - oparg;
            for (int i = 0; i < oparg; ++i) {
                PyObject *o = PyTuple_GET_ITEM(closure, i);
                frame->localsplus[offset + i] = PyStackRef_FromPyObjectNew(o);
            }
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _BUILD_STRING. */

        /* Cannot outline _BUILD_INTERPOLATION. */

        /* Cannot outline _BUILD_TEMPLATE. */

        /* Cannot outline _BUILD_TUPLE. */

        /* Cannot outline _BUILD_LIST. */

        /* Cannot outline _LIST_EXTEND. */

        /* Cannot outline _SET_UPDATE. */

        /* Cannot outline _BUILD_SET. */

        /* Cannot outline _BUILD_MAP. */

        /* Cannot outline _SETUP_ANNOTATIONS. */

        /* Cannot outline _DICT_UPDATE. */

        /* Cannot outline _DICT_MERGE. */

        /* Cannot outline _MAP_ADD. */

        /* Cannot outline _LOAD_SUPER_ATTR_ATTR. */

        /* Cannot outline _LOAD_SUPER_ATTR_METHOD. */

        /* Cannot outline _LOAD_ATTR. */

        /* Cannot outline _GUARD_TYPE_VERSION. */

        /* Cannot outline _GUARD_TYPE_VERSION_AND_LOCK. */

        /* Cannot outline _CHECK_MANAGED_OBJECT_HAS_VALUES. */

        /* Cannot outline _LOAD_ATTR_INSTANCE_VALUE. */

        /* Cannot outline _LOAD_ATTR_MODULE. */

        /* Cannot outline _LOAD_ATTR_WITH_HINT. */

        /* Cannot outline _LOAD_ATTR_SLOT. */

        /* Cannot outline _CHECK_ATTR_CLASS. */

        /* Cannot outline _LOAD_ATTR_CLASS. */

        /* Cannot outline _LOAD_ATTR_PROPERTY_FRAME. */

        /* _LOAD_ATTR_GETATTRIBUTE_OVERRIDDEN is not a viable micro-op for tier 2 because it has too many cache entries */

        /* Cannot outline _GUARD_DORV_NO_DICT. */

        extern _JITOutlinedReturnVal _STORE_ATTR_INSTANCE_VALUE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef owner;
            _PyStackRef value;
            owner = stack_pointer[-1];
            value = stack_pointer[-2];
            uint16_t offset = (uint16_t)CURRENT_OPERAND0();
            PyObject *owner_o = PyStackRef_AsPyObjectBorrow(owner);
            STAT_INC(STORE_ATTR, hit);
            assert(_PyObject_GetManagedDict(owner_o) == NULL);
            PyObject **value_ptr = (PyObject**)(((char *)owner_o) + offset);
            PyObject *old_value = *value_ptr;
            FT_ATOMIC_STORE_PTR_RELEASE(*value_ptr, PyStackRef_AsPyObjectSteal(value));
            if (old_value == NULL) {
                PyDictValues *values = _PyObject_InlineValues(owner_o);
                Py_ssize_t index = value_ptr - values->values;
                _PyDictValues_AddToInsertionOrder(values, index);
            }
            UNLOCK_OBJECT(owner_o);
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(owner);
            Py_XDECREF(old_value);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _STORE_ATTR_WITH_HINT. */

        /* Cannot outline _STORE_ATTR_SLOT. */

        /* Cannot outline _COMPARE_OP. */

        extern _JITOutlinedReturnVal _COMPARE_OP_FLOAT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            STAT_INC(COMPARE_OP, hit);
            double dleft = PyFloat_AS_DOUBLE(left_o);
            double dright = PyFloat_AS_DOUBLE(right_o);
            int sign_ish = COMPARISON_BIT(dleft, dright);
            PyStackRef_CLOSE_SPECIALIZED(left, _PyFloat_ExactDealloc);
            PyStackRef_CLOSE_SPECIALIZED(right, _PyFloat_ExactDealloc);
            res = (sign_ish & oparg) ? PyStackRef_True : PyStackRef_False;
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _COMPARE_OP_INT. */

        extern _JITOutlinedReturnVal _COMPARE_OP_STR_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            STAT_INC(COMPARE_OP, hit);
            int eq = _PyUnicode_Equal(left_o, right_o);
            assert((oparg >> 5) == Py_EQ || (oparg >> 5) == Py_NE);
            PyStackRef_CLOSE_SPECIALIZED(left, _PyUnicode_ExactDealloc);
            PyStackRef_CLOSE_SPECIALIZED(right, _PyUnicode_ExactDealloc);
            assert(eq == 0 || eq == 1);
            assert((oparg & 0xf) == COMPARISON_NOT_EQUALS || (oparg & 0xf) == COMPARISON_EQUALS);
            assert(COMPARISON_NOT_EQUALS + 1 == COMPARISON_EQUALS);
            res = ((COMPARISON_NOT_EQUALS + eq) & oparg) ? PyStackRef_True : PyStackRef_False;
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _IS_OP. */

        /* Cannot outline _CONTAINS_OP. */

        /* Cannot outline _GUARD_TOS_ANY_SET. */

        /* Cannot outline _CONTAINS_OP_SET. */

        /* Cannot outline _CONTAINS_OP_DICT. */

        /* Cannot outline _CHECK_EG_MATCH. */

        /* Cannot outline _CHECK_EXC_MATCH. */

        /* Cannot outline _IMPORT_NAME. */

        /* Cannot outline _IMPORT_FROM. */

        /* _POP_JUMP_IF_FALSE is not a viable micro-op for tier 2 because it is replaced */

        /* _POP_JUMP_IF_TRUE is not a viable micro-op for tier 2 because it is replaced */

        /* Cannot outline _IS_NONE. */

        /* Cannot outline _GET_LEN. */

        /* Cannot outline _MATCH_CLASS. */

        /* Cannot outline _MATCH_MAPPING. */

        /* Cannot outline _MATCH_SEQUENCE. */

        /* Cannot outline _MATCH_KEYS. */

        /* Cannot outline _GET_ITER. */

        /* Cannot outline _GET_YIELD_FROM_ITER. */

        /* _FOR_ITER is not a viable micro-op for tier 2 because it is replaced */

        /* Cannot outline _FOR_ITER_TIER_TWO. */

        /* _INSTRUMENTED_FOR_ITER is not a viable micro-op for tier 2 because it is instrumented */

        /* Cannot outline _ITER_CHECK_LIST. */

        /* _ITER_JUMP_LIST is not a viable micro-op for tier 2 because it is replaced */

        /* Cannot outline _GUARD_NOT_EXHAUSTED_LIST. */

        /* _ITER_NEXT_LIST is not a viable micro-op for tier 2 because it is replaced */

        /* Cannot outline _ITER_NEXT_LIST_TIER_TWO. */

        /* Cannot outline _ITER_CHECK_TUPLE. */

        /* _ITER_JUMP_TUPLE is not a viable micro-op for tier 2 because it is replaced */

        /* Cannot outline _GUARD_NOT_EXHAUSTED_TUPLE. */

        extern _JITOutlinedReturnVal _ITER_NEXT_TUPLE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef iter;
            _PyStackRef next;
            iter = stack_pointer[-1];
            PyObject *iter_o = PyStackRef_AsPyObjectBorrow(iter);
            _PyTupleIterObject *it = (_PyTupleIterObject *)iter_o;
            assert(Py_TYPE(iter_o) == &PyTupleIter_Type);
            PyTupleObject *seq = it->it_seq;
            #ifdef Py_GIL_DISABLED
            assert(_PyObject_IsUniquelyReferenced(iter_o));
            #endif
            assert(seq);
            assert(it->it_index < PyTuple_GET_SIZE(seq));
            next = PyStackRef_FromPyObjectNew(PyTuple_GET_ITEM(seq, it->it_index++));
            stack_pointer[0] = next;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _ITER_CHECK_RANGE. */

        /* _ITER_JUMP_RANGE is not a viable micro-op for tier 2 because it is replaced */

        /* Cannot outline _GUARD_NOT_EXHAUSTED_RANGE. */

        /* Cannot outline _ITER_NEXT_RANGE. */

        /* Cannot outline _FOR_ITER_GEN_FRAME. */

        /* Cannot outline _INSERT_NULL. */

        /* Cannot outline _LOAD_SPECIAL. */

        /* Cannot outline _WITH_EXCEPT_START. */

        extern _JITOutlinedReturnVal _PUSH_EXC_INFO_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef exc;
            _PyStackRef prev_exc;
            _PyStackRef new_exc;
            exc = stack_pointer[-1];
            _PyErr_StackItem *exc_info = tstate->exc_info;
            if (exc_info->exc_value != NULL) {
                prev_exc = PyStackRef_FromPyObjectSteal(exc_info->exc_value);
            }
            else {
                prev_exc = PyStackRef_None;
            }
            assert(PyStackRef_ExceptionInstanceCheck(exc));
            exc_info->exc_value = PyStackRef_AsPyObjectNew(exc);
            new_exc = exc;
            stack_pointer[-1] = prev_exc;
            stack_pointer[0] = new_exc;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _GUARD_DORV_VALUES_INST_ATTR_FROM_DICT. */

        /* Cannot outline _GUARD_KEYS_VERSION. */

        extern _JITOutlinedReturnVal _LOAD_ATTR_METHOD_WITH_VALUES_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef owner;
            _PyStackRef attr;
            _PyStackRef self;
            oparg = CURRENT_OPARG();
            owner = stack_pointer[-1];
            PyObject *descr = (PyObject *)CURRENT_OPERAND0();
            assert(oparg & 1);
            STAT_INC(LOAD_ATTR, hit);
            assert(descr != NULL);
            assert(_PyType_HasFeature(Py_TYPE(descr), Py_TPFLAGS_METHOD_DESCRIPTOR));
            attr = PyStackRef_FromPyObjectNew(descr);
            self = owner;
            stack_pointer[-1] = attr;
            stack_pointer[0] = self;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        extern _JITOutlinedReturnVal _LOAD_ATTR_METHOD_NO_DICT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef owner;
            _PyStackRef attr;
            _PyStackRef self;
            oparg = CURRENT_OPARG();
            owner = stack_pointer[-1];
            PyObject *descr = (PyObject *)CURRENT_OPERAND0();
            assert(oparg & 1);
            assert(Py_TYPE(PyStackRef_AsPyObjectBorrow(owner))->tp_dictoffset == 0);
            STAT_INC(LOAD_ATTR, hit);
            assert(descr != NULL);
            assert(_PyType_HasFeature(Py_TYPE(descr), Py_TPFLAGS_METHOD_DESCRIPTOR));
            attr = PyStackRef_FromPyObjectNew(descr);
            self = owner;
            stack_pointer[-1] = attr;
            stack_pointer[0] = self;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _LOAD_ATTR_NONDESCRIPTOR_WITH_VALUES. */

        extern _JITOutlinedReturnVal _LOAD_ATTR_NONDESCRIPTOR_NO_DICT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef owner;
            _PyStackRef attr;
            oparg = CURRENT_OPARG();
            owner = stack_pointer[-1];
            PyObject *descr = (PyObject *)CURRENT_OPERAND0();
            assert((oparg & 1) == 0);
            assert(Py_TYPE(PyStackRef_AsPyObjectBorrow(owner))->tp_dictoffset == 0);
            STAT_INC(LOAD_ATTR, hit);
            assert(descr != NULL);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(owner);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            attr = PyStackRef_FromPyObjectNew(descr);
            stack_pointer[0] = attr;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _CHECK_ATTR_METHOD_LAZY_DICT. */

        extern _JITOutlinedReturnVal _LOAD_ATTR_METHOD_LAZY_DICT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef owner;
            _PyStackRef attr;
            _PyStackRef self;
            oparg = CURRENT_OPARG();
            owner = stack_pointer[-1];
            PyObject *descr = (PyObject *)CURRENT_OPERAND0();
            assert(oparg & 1);
            STAT_INC(LOAD_ATTR, hit);
            assert(descr != NULL);
            assert(_PyType_HasFeature(Py_TYPE(descr), Py_TPFLAGS_METHOD_DESCRIPTOR));
            attr = PyStackRef_FromPyObjectNew(descr);
            self = owner;
            stack_pointer[-1] = attr;
            stack_pointer[0] = self;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _MAYBE_EXPAND_METHOD. */

        /* _DO_CALL is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* _MONITOR_CALL is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _PY_FRAME_GENERAL. */

        /* Cannot outline _CHECK_FUNCTION_VERSION. */

        /* Cannot outline _CHECK_FUNCTION_VERSION_INLINE. */

        /* Cannot outline _CHECK_METHOD_VERSION. */

        extern _JITOutlinedReturnVal _EXPAND_METHOD_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            oparg = CURRENT_OPARG();
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            assert(PyStackRef_IsNull(self_or_null));
            assert(Py_TYPE(callable_o) == &PyMethod_Type);
            self_or_null = PyStackRef_FromPyObjectNew(((PyMethodObject *)callable_o)->im_self);
            _PyStackRef temp = callable;
            callable = PyStackRef_FromPyObjectNew(((PyMethodObject *)callable_o)->im_func);
            assert(PyStackRef_FunctionCheck(callable));
            stack_pointer[-2 - oparg] = callable;
            stack_pointer[-1 - oparg] = self_or_null;
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(temp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _CHECK_IS_NOT_PY_CALLABLE. */

        /* Cannot outline _CALL_NON_PY_GENERAL. */

        /* Cannot outline _CHECK_CALL_BOUND_METHOD_EXACT_ARGS. */

        extern _JITOutlinedReturnVal _INIT_CALL_BOUND_METHOD_EXACT_ARGS_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            oparg = CURRENT_OPARG();
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            assert(PyStackRef_IsNull(self_or_null));
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            STAT_INC(CALL, hit);
            self_or_null = PyStackRef_FromPyObjectNew(((PyMethodObject *)callable_o)->im_self);
            _PyStackRef temp = callable;
            callable = PyStackRef_FromPyObjectNew(((PyMethodObject *)callable_o)->im_func);
            stack_pointer[-2 - oparg] = callable;
            stack_pointer[-1 - oparg] = self_or_null;
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(temp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _CHECK_PEP_523. */

        /* Cannot outline _CHECK_FUNCTION_EXACT_ARGS. */

        /* Cannot outline _CHECK_STACK_SPACE. */

        /* Cannot outline _CHECK_RECURSION_REMAINING. */

        extern _JITOutlinedReturnVal _INIT_CALL_PY_EXACT_ARGS_0_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = 0;
            assert(oparg == CURRENT_OPARG());
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            int has_self = !PyStackRef_IsNull(self_or_null);
            STAT_INC(CALL, hit);
            new_frame = _PyFrame_PushUnchecked(tstate, callable, oparg + has_self, frame);
            _PyStackRef *first_non_self_local = new_frame->localsplus + has_self;
            new_frame->localsplus[0] = self_or_null;
            for (int i = 0; i < oparg; i++) {
                first_non_self_local[i] = args[i];
            }
            stack_pointer[-2 - oparg].bits = (uintptr_t)new_frame;
            stack_pointer += -1 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        extern _JITOutlinedReturnVal _INIT_CALL_PY_EXACT_ARGS_1_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = 1;
            assert(oparg == CURRENT_OPARG());
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            int has_self = !PyStackRef_IsNull(self_or_null);
            STAT_INC(CALL, hit);
            new_frame = _PyFrame_PushUnchecked(tstate, callable, oparg + has_self, frame);
            _PyStackRef *first_non_self_local = new_frame->localsplus + has_self;
            new_frame->localsplus[0] = self_or_null;
            for (int i = 0; i < oparg; i++) {
                first_non_self_local[i] = args[i];
            }
            stack_pointer[-2 - oparg].bits = (uintptr_t)new_frame;
            stack_pointer += -1 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        extern _JITOutlinedReturnVal _INIT_CALL_PY_EXACT_ARGS_2_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = 2;
            assert(oparg == CURRENT_OPARG());
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            int has_self = !PyStackRef_IsNull(self_or_null);
            STAT_INC(CALL, hit);
            new_frame = _PyFrame_PushUnchecked(tstate, callable, oparg + has_self, frame);
            _PyStackRef *first_non_self_local = new_frame->localsplus + has_self;
            new_frame->localsplus[0] = self_or_null;
            for (int i = 0; i < oparg; i++) {
                first_non_self_local[i] = args[i];
            }
            stack_pointer[-2 - oparg].bits = (uintptr_t)new_frame;
            stack_pointer += -1 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        extern _JITOutlinedReturnVal _INIT_CALL_PY_EXACT_ARGS_3_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = 3;
            assert(oparg == CURRENT_OPARG());
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            int has_self = !PyStackRef_IsNull(self_or_null);
            STAT_INC(CALL, hit);
            new_frame = _PyFrame_PushUnchecked(tstate, callable, oparg + has_self, frame);
            _PyStackRef *first_non_self_local = new_frame->localsplus + has_self;
            new_frame->localsplus[0] = self_or_null;
            for (int i = 0; i < oparg; i++) {
                first_non_self_local[i] = args[i];
            }
            stack_pointer[-2 - oparg].bits = (uintptr_t)new_frame;
            stack_pointer += -1 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        extern _JITOutlinedReturnVal _INIT_CALL_PY_EXACT_ARGS_4_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = 4;
            assert(oparg == CURRENT_OPARG());
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            int has_self = !PyStackRef_IsNull(self_or_null);
            STAT_INC(CALL, hit);
            new_frame = _PyFrame_PushUnchecked(tstate, callable, oparg + has_self, frame);
            _PyStackRef *first_non_self_local = new_frame->localsplus + has_self;
            new_frame->localsplus[0] = self_or_null;
            for (int i = 0; i < oparg; i++) {
                first_non_self_local[i] = args[i];
            }
            stack_pointer[-2 - oparg].bits = (uintptr_t)new_frame;
            stack_pointer += -1 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        extern _JITOutlinedReturnVal _INIT_CALL_PY_EXACT_ARGS_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = CURRENT_OPARG();
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            int has_self = !PyStackRef_IsNull(self_or_null);
            STAT_INC(CALL, hit);
            new_frame = _PyFrame_PushUnchecked(tstate, callable, oparg + has_self, frame);
            _PyStackRef *first_non_self_local = new_frame->localsplus + has_self;
            new_frame->localsplus[0] = self_or_null;
            for (int i = 0; i < oparg; i++) {
                first_non_self_local[i] = args[i];
            }
            stack_pointer[-2 - oparg].bits = (uintptr_t)new_frame;
            stack_pointer += -1 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        extern _JITOutlinedReturnVal _PUSH_FRAME_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyInterpreterFrame *new_frame;
            new_frame = (_PyInterpreterFrame *)stack_pointer[-1].bits;
            assert(tstate->interp->eval_frame == NULL);
            _PyInterpreterFrame *temp = new_frame;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            assert(new_frame->previous == frame || new_frame->previous->previous == frame);
            CALL_STAT_INC(inlined_py_calls);
            frame = tstate->current_frame = temp;
            tstate->py_recursion_remaining--;
            LOAD_SP();
            LOAD_IP(0);
            LLTRACE_RESUME_FRAME();
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _GUARD_NOS_NULL. */

        /* Cannot outline _GUARD_CALLABLE_TYPE_1. */

        extern _JITOutlinedReturnVal _CALL_TYPE_1_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef arg;
            _PyStackRef null;
            _PyStackRef callable;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            arg = stack_pointer[-1];
            null = stack_pointer[-2];
            callable = stack_pointer[-3];
            PyObject *arg_o = PyStackRef_AsPyObjectBorrow(arg);
            assert(oparg == 1);
            (void)callable;
            (void)null;
            STAT_INC(CALL, hit);
            res = PyStackRef_FromPyObjectNew(Py_TYPE(arg_o));
            stack_pointer[-3] = res;
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(arg);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _GUARD_CALLABLE_STR_1. */

        /* Cannot outline _CALL_STR_1. */

        /* Cannot outline _GUARD_CALLABLE_TUPLE_1. */

        /* Cannot outline _CALL_TUPLE_1. */

        /* Cannot outline _CHECK_AND_ALLOCATE_OBJECT. */

        /* Cannot outline _CREATE_INIT_FRAME. */

        /* Cannot outline _EXIT_INIT_CHECK. */

        /* Cannot outline _CALL_BUILTIN_CLASS. */

        /* Cannot outline _CALL_BUILTIN_O. */

        /* Cannot outline _CALL_BUILTIN_FAST. */

        /* Cannot outline _CALL_BUILTIN_FAST_WITH_KEYWORDS. */

        /* Cannot outline _GUARD_CALLABLE_LEN. */

        /* Cannot outline _CALL_LEN. */

        /* Cannot outline _CALL_ISINSTANCE. */

        /* Cannot outline _CALL_LIST_APPEND. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_O. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_FAST_WITH_KEYWORDS. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_NOARGS. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_FAST. */

        /* _MONITOR_CALL_KW is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _MAYBE_EXPAND_METHOD_KW. */

        /* _DO_CALL_KW is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _PY_FRAME_KW. */

        /* Cannot outline _CHECK_FUNCTION_VERSION_KW. */

        /* Cannot outline _CHECK_METHOD_VERSION_KW. */

        extern _JITOutlinedReturnVal _EXPAND_METHOD_KW_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            oparg = CURRENT_OPARG();
            self_or_null = stack_pointer[-2 - oparg];
            callable = stack_pointer[-3 - oparg];
            assert(PyStackRef_IsNull(self_or_null));
            _PyStackRef callable_s = callable;
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            assert(Py_TYPE(callable_o) == &PyMethod_Type);
            self_or_null = PyStackRef_FromPyObjectNew(((PyMethodObject *)callable_o)->im_self);
            callable = PyStackRef_FromPyObjectNew(((PyMethodObject *)callable_o)->im_func);
            assert(PyStackRef_FunctionCheck(callable));
            stack_pointer[-3 - oparg] = callable;
            stack_pointer[-2 - oparg] = self_or_null;
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(callable_s);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _CHECK_IS_NOT_PY_CALLABLE_KW. */

        /* Cannot outline _CALL_KW_NON_PY. */

        /* Cannot outline _MAKE_CALLARGS_A_TUPLE. */

        /* _DO_CALL_FUNCTION_EX is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _MAKE_FUNCTION. */

        extern _JITOutlinedReturnVal _SET_FUNCTION_ATTRIBUTE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef func_in;
            _PyStackRef attr_st;
            _PyStackRef func_out;
            oparg = CURRENT_OPARG();
            func_in = stack_pointer[-1];
            attr_st = stack_pointer[-2];
            PyObject *func = PyStackRef_AsPyObjectBorrow(func_in);
            PyObject *attr = PyStackRef_AsPyObjectSteal(attr_st);
            func_out = func_in;
            assert(PyFunction_Check(func));
            size_t offset = _Py_FunctionAttributeOffsets[oparg];
            assert(offset != 0);
            PyObject **ptr = (PyObject **)(((char *)func) + offset);
            assert(*ptr == NULL);
            *ptr = attr;
            stack_pointer[-2] = func_out;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate};
            return _retval;
        }

        /* Cannot outline _RETURN_GENERATOR. */

        /* Cannot outline _BUILD_SLICE. */

        /* Cannot outline _CONVERT_VALUE. */

        /* Cannot outline _FORMAT_SIMPLE. */

        /* Cannot outline _FORMAT_WITH_SPEC. */

        /* Cannot outline _COPY. */

        /* Cannot outline _BINARY_OP. */

        /* Cannot outline _SWAP. */

        /* _INSTRUMENTED_LINE is not a viable micro-op for tier 2 because it is instrumented */

        /* _INSTRUMENTED_INSTRUCTION is not a viable micro-op for tier 2 because it is instrumented */

        /* _INSTRUMENTED_JUMP_FORWARD is not a viable micro-op for tier 2 because it is instrumented */

        /* _MONITOR_JUMP_BACKWARD is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* _INSTRUMENTED_NOT_TAKEN is not a viable micro-op for tier 2 because it is instrumented */

        /* _INSTRUMENTED_POP_JUMP_IF_TRUE is not a viable micro-op for tier 2 because it is instrumented */

        /* _INSTRUMENTED_POP_JUMP_IF_FALSE is not a viable micro-op for tier 2 because it is instrumented */

        /* _INSTRUMENTED_POP_JUMP_IF_NONE is not a viable micro-op for tier 2 because it is instrumented */

        /* _INSTRUMENTED_POP_JUMP_IF_NOT_NONE is not a viable micro-op for tier 2 because it is instrumented */

        /* Cannot outline _GUARD_IS_TRUE_POP. */

        /* Cannot outline _GUARD_IS_FALSE_POP. */

        /* Cannot outline _GUARD_IS_NONE_POP. */

        /* Cannot outline _GUARD_IS_NOT_NONE_POP. */

        /* Cannot outline _JUMP_TO_TOP. */

        /* Cannot outline _SET_IP. */

        /* Cannot outline _CHECK_STACK_SPACE_OPERAND. */

        /* Cannot outline _SAVE_RETURN_OFFSET. */

        /* Cannot outline _EXIT_TRACE. */

        /* Cannot outline _CHECK_VALIDITY. */

        /* Cannot outline _LOAD_CONST_INLINE. */

        /* Cannot outline _POP_TOP_LOAD_CONST_INLINE. */

        /* Cannot outline _LOAD_CONST_INLINE_BORROW. */

        /* Cannot outline _POP_TOP_LOAD_CONST_INLINE_BORROW. */

        /* Cannot outline _POP_TWO_LOAD_CONST_INLINE_BORROW. */

        /* Cannot outline _CHECK_FUNCTION. */

        /* Cannot outline _START_EXECUTOR. */

        /* Cannot outline _MAKE_WARM. */

        /* Cannot outline _FATAL_ERROR. */

        /* Cannot outline _DEOPT. */

        /* Cannot outline _ERROR_POP_N. */

        /* Cannot outline _TIER2_RESUME_CHECK. */

