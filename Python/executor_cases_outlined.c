
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

        extern _JITOutlinedReturnVal _BINARY_OP_MULTIPLY_INT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyLong_CheckExact(left_o));
            assert(PyLong_CheckExact(right_o));
            STAT_INC(BINARY_OP, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = _PyLong_Multiply((PyLongObject *)left_o, (PyLongObject *)right_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            PyStackRef_CLOSE_SPECIALIZED(right, _PyLong_ExactDealloc);
            PyStackRef_CLOSE_SPECIALIZED(left, _PyLong_ExactDealloc);
            if (res_o == NULL) {
                stack_pointer += -2;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BINARY_OP_ADD_INT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyLong_CheckExact(left_o));
            assert(PyLong_CheckExact(right_o));
            STAT_INC(BINARY_OP, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = _PyLong_Add((PyLongObject *)left_o, (PyLongObject *)right_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            PyStackRef_CLOSE_SPECIALIZED(right, _PyLong_ExactDealloc);
            PyStackRef_CLOSE_SPECIALIZED(left, _PyLong_ExactDealloc);
            if (res_o == NULL) {
                stack_pointer += -2;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BINARY_OP_SUBTRACT_INT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyLong_CheckExact(left_o));
            assert(PyLong_CheckExact(right_o));
            STAT_INC(BINARY_OP, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = _PyLong_Subtract((PyLongObject *)left_o, (PyLongObject *)right_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            PyStackRef_CLOSE_SPECIALIZED(right, _PyLong_ExactDealloc);
            PyStackRef_CLOSE_SPECIALIZED(left, _PyLong_ExactDealloc);
            if (res_o == NULL) {
                stack_pointer += -2;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _GUARD_NOS_FLOAT. */

        /* Cannot outline _GUARD_TOS_FLOAT. */

        extern _JITOutlinedReturnVal _BINARY_OP_MULTIPLY_FLOAT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyFloat_CheckExact(left_o));
            assert(PyFloat_CheckExact(right_o));
            STAT_INC(BINARY_OP, hit);
            double dres =
            ((PyFloatObject *)left_o)->ob_fval *
            ((PyFloatObject *)right_o)->ob_fval;
            res = _PyFloat_FromDouble_ConsumeInputs(left, right, dres);
            if (PyStackRef_IsNull(res)) {
                stack_pointer[-2] = res;
                stack_pointer += -1;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BINARY_OP_ADD_FLOAT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyFloat_CheckExact(left_o));
            assert(PyFloat_CheckExact(right_o));
            STAT_INC(BINARY_OP, hit);
            double dres =
            ((PyFloatObject *)left_o)->ob_fval +
            ((PyFloatObject *)right_o)->ob_fval;
            res = _PyFloat_FromDouble_ConsumeInputs(left, right, dres);
            if (PyStackRef_IsNull(res)) {
                stack_pointer[-2] = res;
                stack_pointer += -1;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BINARY_OP_SUBTRACT_FLOAT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyFloat_CheckExact(left_o));
            assert(PyFloat_CheckExact(right_o));
            STAT_INC(BINARY_OP, hit);
            double dres =
            ((PyFloatObject *)left_o)->ob_fval -
            ((PyFloatObject *)right_o)->ob_fval;
            res = _PyFloat_FromDouble_ConsumeInputs(left, right, dres);
            if (PyStackRef_IsNull(res)) {
                stack_pointer[-2] = res;
                stack_pointer += -1;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BINARY_OP_ADD_UNICODE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyUnicode_CheckExact(left_o));
            assert(PyUnicode_CheckExact(right_o));
            STAT_INC(BINARY_OP, hit);
            PyObject *res_o = PyUnicode_Concat(left_o, right_o);
            PyStackRef_CLOSE_SPECIALIZED(right, _PyUnicode_ExactDealloc);
            PyStackRef_CLOSE_SPECIALIZED(left, _PyUnicode_ExactDealloc);
            if (res_o == NULL) {
                stack_pointer += -2;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[-2] = res;
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BINARY_SLICE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef stop;
            _PyStackRef start;
            _PyStackRef container;
            _PyStackRef res;
            stop = stack_pointer[-1];
            start = stack_pointer[-2];
            container = stack_pointer[-3];
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *slice = _PyBuildSlice_ConsumeRefs(PyStackRef_AsPyObjectSteal(start),
                PyStackRef_AsPyObjectSteal(stop));
            stack_pointer = _PyFrame_GetStackPointer(frame);
            PyObject *res_o;
            if (slice == NULL) {
                res_o = NULL;
            }
            else {
                stack_pointer += -2;
                assert(WITHIN_STACK_BOUNDS());
                _PyFrame_SetStackPointer(frame, stack_pointer);
                res_o = PyObject_GetItem(PyStackRef_AsPyObjectBorrow(container), slice);
                Py_DECREF(slice);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += 2;
            }
            stack_pointer += -3;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(container);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _STORE_SLICE. */

        /* Cannot outline _BINARY_OP_SUBSCR_LIST_INT. */

        extern _JITOutlinedReturnVal _BINARY_OP_SUBSCR_LIST_SLICE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef sub_st;
            _PyStackRef list_st;
            _PyStackRef res;
            sub_st = stack_pointer[-1];
            list_st = stack_pointer[-2];
            PyObject *sub = PyStackRef_AsPyObjectBorrow(sub_st);
            PyObject *list = PyStackRef_AsPyObjectBorrow(list_st);
            assert(PySlice_Check(sub));
            assert(PyList_CheckExact(list));
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = _PyList_SliceSubscript(list, sub);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            STAT_INC(BINARY_OP, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp = sub_st;
            sub_st = PyStackRef_NULL;
            stack_pointer[-1] = sub_st;
            PyStackRef_CLOSE(tmp);
            tmp = list_st;
            list_st = PyStackRef_NULL;
            stack_pointer[-2] = list_st;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _BINARY_OP_SUBSCR_STR_INT. */

        /* Cannot outline _GUARD_NOS_TUPLE. */

        /* Cannot outline _GUARD_TOS_TUPLE. */

        /* Cannot outline _BINARY_OP_SUBSCR_TUPLE_INT. */

        /* Cannot outline _GUARD_NOS_DICT. */

        /* Cannot outline _GUARD_TOS_DICT. */

        extern _JITOutlinedReturnVal _BINARY_OP_SUBSCR_DICT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef sub_st;
            _PyStackRef dict_st;
            _PyStackRef res;
            sub_st = stack_pointer[-1];
            dict_st = stack_pointer[-2];
            PyObject *sub = PyStackRef_AsPyObjectBorrow(sub_st);
            PyObject *dict = PyStackRef_AsPyObjectBorrow(dict_st);
            assert(PyDict_CheckExact(dict));
            STAT_INC(BINARY_OP, hit);
            PyObject *res_o;
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int rc = PyDict_GetItemRef(dict, sub, &res_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (rc == 0) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyErr_SetKeyError(sub);
                stack_pointer = _PyFrame_GetStackPointer(frame);
            }
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp = sub_st;
            sub_st = PyStackRef_NULL;
            stack_pointer[-1] = sub_st;
            PyStackRef_CLOSE(tmp);
            tmp = dict_st;
            dict_st = PyStackRef_NULL;
            stack_pointer[-2] = dict_st;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            if (rc <= 0) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _BINARY_OP_SUBSCR_CHECK_FUNC. */

        /* Cannot outline _BINARY_OP_SUBSCR_INIT_CALL. */

        /* Cannot outline _LIST_APPEND. */

        /* Cannot outline _SET_ADD. */

        /* Cannot outline _STORE_SUBSCR. */

        /* Cannot outline _STORE_SUBSCR_LIST_INT. */

        extern _JITOutlinedReturnVal _STORE_SUBSCR_DICT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef sub;
            _PyStackRef dict_st;
            _PyStackRef value;
            sub = stack_pointer[-1];
            dict_st = stack_pointer[-2];
            value = stack_pointer[-3];
            PyObject *dict = PyStackRef_AsPyObjectBorrow(dict_st);
            assert(PyDict_CheckExact(dict));
            STAT_INC(STORE_SUBSCR, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int err = _PyDict_SetItem_Take2((PyDictObject *)dict,
                PyStackRef_AsPyObjectSteal(sub),
                PyStackRef_AsPyObjectSteal(value));
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -3;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(dict_st);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (err) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _DELETE_SUBSCR. */

        /* Cannot outline _CALL_INTRINSIC_1. */

        extern _JITOutlinedReturnVal _CALL_INTRINSIC_2_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef value1_st;
            _PyStackRef value2_st;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            value1_st = stack_pointer[-1];
            value2_st = stack_pointer[-2];
            assert(oparg <= MAX_INTRINSIC_2);
            PyObject *value1 = PyStackRef_AsPyObjectBorrow(value1_st);
            PyObject *value2 = PyStackRef_AsPyObjectBorrow(value2_st);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = _PyIntrinsics_BinaryFunctions[oparg].func(tstate, value2, value1);
            _PyStackRef tmp = value1_st;
            value1_st = PyStackRef_NULL;
            stack_pointer[-1] = value1_st;
            PyStackRef_CLOSE(tmp);
            tmp = value2_st;
            value2_st = PyStackRef_NULL;
            stack_pointer[-2] = value2_st;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _GET_AITER_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef obj;
            _PyStackRef iter;
            obj = stack_pointer[-1];
            unaryfunc getter = NULL;
            PyObject *obj_o = PyStackRef_AsPyObjectBorrow(obj);
            PyObject *iter_o;
            PyTypeObject *type = Py_TYPE(obj_o);
            if (type->tp_as_async != NULL) {
                getter = type->tp_as_async->am_aiter;
            }
            if (getter == NULL) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyErr_Format(tstate, PyExc_TypeError,
                              "'async for' requires an object with "
                              "__aiter__ method, got %.100s",
                              type->tp_name);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -1;
                assert(WITHIN_STACK_BOUNDS());
                _PyFrame_SetStackPointer(frame, stack_pointer);
                PyStackRef_CLOSE(obj);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            _PyFrame_SetStackPointer(frame, stack_pointer);
            iter_o = (*getter)(obj_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(obj);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (iter_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            if (Py_TYPE(iter_o)->tp_as_async == NULL ||
                Py_TYPE(iter_o)->tp_as_async->am_anext == NULL) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyErr_Format(tstate, PyExc_TypeError,
                              "'async for' received an object from __aiter__ "
                              "that does not implement __anext__: %.100s",
                              Py_TYPE(iter_o)->tp_name);
                Py_DECREF(iter_o);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            iter = PyStackRef_FromPyObjectSteal(iter_o);
            stack_pointer[0] = iter;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _POP_EXCEPT. */

        /* Cannot outline _LOAD_COMMON_CONSTANT. */

        /* Cannot outline _LOAD_BUILD_CLASS. */

        extern _JITOutlinedReturnVal _STORE_NAME_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef v;
            oparg = CURRENT_OPARG();
            v = stack_pointer[-1];
            PyObject *name = GETITEM(FRAME_CO_NAMES, oparg);
            PyObject *ns = LOCALS();
            int err;
            if (ns == NULL) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyErr_Format(tstate, PyExc_SystemError,
                              "no locals found when storing %R", name);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -1;
                assert(WITHIN_STACK_BOUNDS());
                _PyFrame_SetStackPointer(frame, stack_pointer);
                PyStackRef_CLOSE(v);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            if (PyDict_CheckExact(ns)) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                err = PyDict_SetItem(ns, name, PyStackRef_AsPyObjectBorrow(v));
                stack_pointer = _PyFrame_GetStackPointer(frame);
            }
            else {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                err = PyObject_SetItem(ns, name, PyStackRef_AsPyObjectBorrow(v));
                stack_pointer = _PyFrame_GetStackPointer(frame);
            }
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(v);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (err) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _DELETE_NAME_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            oparg = CURRENT_OPARG();
            PyObject *name = GETITEM(FRAME_CO_NAMES, oparg);
            PyObject *ns = LOCALS();
            int err;
            if (ns == NULL) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyErr_Format(tstate, PyExc_SystemError,
                              "no locals when deleting %R", name);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            _PyFrame_SetStackPointer(frame, stack_pointer);
            err = PyObject_DelItem(ns, name);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (err != 0) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyEval_FormatExcCheckArg(tstate, PyExc_NameError,
                    NAME_ERROR_MSG,
                    name);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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

        extern _JITOutlinedReturnVal _MAKE_CELL_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            oparg = CURRENT_OPARG();
            PyObject *initial = PyStackRef_AsPyObjectBorrow(GETLOCAL(oparg));
            PyObject *cell = PyCell_New(initial);
            if (cell == NULL) {_JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            _PyStackRef tmp = GETLOCAL(oparg);
            GETLOCAL(oparg) = PyStackRef_FromPyObjectSteal(cell);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_XCLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _DELETE_DEREF. */

        extern _JITOutlinedReturnVal _LOAD_FROM_DICT_OR_DEREF_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef class_dict_st;
            _PyStackRef value;
            oparg = CURRENT_OPARG();
            class_dict_st = stack_pointer[-1];
            PyObject *value_o;
            PyObject *name;
            PyObject *class_dict = PyStackRef_AsPyObjectBorrow(class_dict_st);
            assert(class_dict);
            assert(oparg >= 0 && oparg < _PyFrame_GetCode(frame)->co_nlocalsplus);
            name = PyTuple_GET_ITEM(_PyFrame_GetCode(frame)->co_localsplusnames, oparg);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int err = PyMapping_GetOptionalItem(class_dict, name, &value_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (err < 0) {_JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            if (!value_o) {
                PyCellObject *cell = (PyCellObject *)PyStackRef_AsPyObjectBorrow(GETLOCAL(oparg));
                value_o = PyCell_GetRef(cell);
                if (value_o == NULL) {
                    _PyFrame_SetStackPointer(frame, stack_pointer);
                    _PyEval_FormatExcUnbound(tstate, _PyFrame_GetCode(frame), oparg);
                    stack_pointer = _PyFrame_GetStackPointer(frame);
                    _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                    return _retval;
                    
                }
            }
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(class_dict_st);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            value = PyStackRef_FromPyObjectSteal(value_o);
            stack_pointer[0] = value;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BUILD_STRING_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *pieces;
            _PyStackRef str;
            oparg = CURRENT_OPARG();
            pieces = &stack_pointer[-oparg];
            STACKREFS_TO_PYOBJECTS(pieces, oparg, pieces_o);
            if (CONVERSION_FAILED(pieces_o)) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyStackRef tmp;
                for (int _i = oparg; --_i >= 0;) {
                    tmp = pieces[_i];
                    pieces[_i] = PyStackRef_NULL;
                    PyStackRef_CLOSE(tmp);
                }
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -oparg;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            PyObject *str_o = _PyUnicode_JoinArray(&_Py_STR(empty), pieces_o, oparg);
            STACKREFS_TO_PYOBJECTS_CLEANUP(pieces_o);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp;
            for (int _i = oparg; --_i >= 0;) {
                tmp = pieces[_i];
                pieces[_i] = PyStackRef_NULL;
                PyStackRef_CLOSE(tmp);
            }
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -oparg;
            assert(WITHIN_STACK_BOUNDS());
            if (str_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            str = PyStackRef_FromPyObjectSteal(str_o);
            stack_pointer[0] = str;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BUILD_INTERPOLATION_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *format;
            _PyStackRef str;
            _PyStackRef value;
            _PyStackRef interpolation;
            oparg = CURRENT_OPARG();
            format = &stack_pointer[-(oparg & 1)];
            str = stack_pointer[-1 - (oparg & 1)];
            value = stack_pointer[-2 - (oparg & 1)];
            PyObject *value_o = PyStackRef_AsPyObjectBorrow(value);
            PyObject *str_o = PyStackRef_AsPyObjectBorrow(str);
            int conversion = oparg >> 2;
            PyObject *format_o;
            if (oparg & 1) {
                format_o = PyStackRef_AsPyObjectBorrow(format[0]);
            }
            else {
                format_o = &_Py_STR(empty);
            }
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *interpolation_o = _PyInterpolation_Build(value_o, str_o, conversion, format_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (oparg & 1) {
                stack_pointer += -(oparg & 1);
                assert(WITHIN_STACK_BOUNDS());
                _PyFrame_SetStackPointer(frame, stack_pointer);
                PyStackRef_CLOSE(format[0]);
                stack_pointer = _PyFrame_GetStackPointer(frame);
            }
            else {
                stack_pointer += -(oparg & 1);
            }
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(str);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(value);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (interpolation_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            interpolation = PyStackRef_FromPyObjectSteal(interpolation_o);
            stack_pointer[0] = interpolation;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BUILD_TEMPLATE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef interpolations;
            _PyStackRef strings;
            _PyStackRef template;
            interpolations = stack_pointer[-1];
            strings = stack_pointer[-2];
            PyObject *strings_o = PyStackRef_AsPyObjectBorrow(strings);
            PyObject *interpolations_o = PyStackRef_AsPyObjectBorrow(interpolations);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *template_o = _PyTemplate_Build(strings_o, interpolations_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(interpolations);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(strings);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (template_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            template = PyStackRef_FromPyObjectSteal(template_o);
            stack_pointer[0] = template;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _BUILD_TUPLE. */

        /* Cannot outline _BUILD_LIST. */

        extern _JITOutlinedReturnVal _LIST_EXTEND_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef iterable_st;
            _PyStackRef list_st;
            oparg = CURRENT_OPARG();
            iterable_st = stack_pointer[-1];
            list_st = stack_pointer[-2 - (oparg-1)];
            PyObject *list = PyStackRef_AsPyObjectBorrow(list_st);
            PyObject *iterable = PyStackRef_AsPyObjectBorrow(iterable_st);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *none_val = _PyList_Extend((PyListObject *)list, iterable);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (none_val == NULL) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                int matches = _PyErr_ExceptionMatches(tstate, PyExc_TypeError);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                if (matches &&
                    (Py_TYPE(iterable)->tp_iter == NULL && !PySequence_Check(iterable)))
                {
                    _PyFrame_SetStackPointer(frame, stack_pointer);
                    _PyErr_Clear(tstate);
                    _PyErr_Format(tstate, PyExc_TypeError,
                                  "Value after * must be an iterable, not %.200s",
                                  Py_TYPE(iterable)->tp_name);
                    stack_pointer = _PyFrame_GetStackPointer(frame);
                }
                stack_pointer += -1;
                assert(WITHIN_STACK_BOUNDS());
                _PyFrame_SetStackPointer(frame, stack_pointer);
                PyStackRef_CLOSE(iterable_st);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            assert(Py_IsNone(none_val));
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(iterable_st);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _SET_UPDATE. */

        extern _JITOutlinedReturnVal _BUILD_SET_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *values;
            _PyStackRef set;
            oparg = CURRENT_OPARG();
            values = &stack_pointer[-oparg];
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *set_o = PySet_New(NULL);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (set_o == NULL) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyStackRef tmp;
                for (int _i = oparg; --_i >= 0;) {
                    tmp = values[_i];
                    values[_i] = PyStackRef_NULL;
                    PyStackRef_CLOSE(tmp);
                }
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -oparg;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            int err = 0;
            for (Py_ssize_t i = 0; i < oparg; i++) {
                _PyStackRef value = values[i];
                values[i] = PyStackRef_NULL;
                if (err == 0) {
                    _PyFrame_SetStackPointer(frame, stack_pointer);
                    err = _PySet_AddTakeRef((PySetObject *)set_o, PyStackRef_AsPyObjectSteal(value));
                    stack_pointer = _PyFrame_GetStackPointer(frame);
                }
                else {
                    _PyFrame_SetStackPointer(frame, stack_pointer);
                    PyStackRef_CLOSE(value);
                    stack_pointer = _PyFrame_GetStackPointer(frame);
                }
            }
            if (err) {
                stack_pointer += -oparg;
                assert(WITHIN_STACK_BOUNDS());
                _PyFrame_SetStackPointer(frame, stack_pointer);
                Py_DECREF(set_o);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            set = PyStackRef_FromPyObjectStealMortal(set_o);
            stack_pointer[-oparg] = set;
            stack_pointer += 1 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BUILD_MAP_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *values;
            _PyStackRef map;
            oparg = CURRENT_OPARG();
            values = &stack_pointer[-oparg*2];
            STACKREFS_TO_PYOBJECTS(values, oparg*2, values_o);
            if (CONVERSION_FAILED(values_o)) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyStackRef tmp;
                for (int _i = oparg*2; --_i >= 0;) {
                    tmp = values[_i];
                    values[_i] = PyStackRef_NULL;
                    PyStackRef_CLOSE(tmp);
                }
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -oparg*2;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *map_o = _PyDict_FromItems(
                values_o, 2,
                values_o+1, 2,
                oparg);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            STACKREFS_TO_PYOBJECTS_CLEANUP(values_o);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp;
            for (int _i = oparg*2; --_i >= 0;) {
                tmp = values[_i];
                values[_i] = PyStackRef_NULL;
                PyStackRef_CLOSE(tmp);
            }
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -oparg*2;
            assert(WITHIN_STACK_BOUNDS());
            if (map_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            map = PyStackRef_FromPyObjectStealMortal(map_o);
            stack_pointer[0] = map;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _SETUP_ANNOTATIONS. */

        /* Cannot outline _DICT_UPDATE. */

        extern _JITOutlinedReturnVal _DICT_MERGE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef update;
            _PyStackRef dict;
            _PyStackRef callable;
            oparg = CURRENT_OPARG();
            update = stack_pointer[-1];
            dict = stack_pointer[-2 - (oparg - 1)];
            callable = stack_pointer[-5 - (oparg - 1)];
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            PyObject *dict_o = PyStackRef_AsPyObjectBorrow(dict);
            PyObject *update_o = PyStackRef_AsPyObjectBorrow(update);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int err = _PyDict_MergeEx(dict_o, update_o, 2);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (err < 0) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyEval_FormatKwargsError(tstate, callable_o, update_o);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -1;
                assert(WITHIN_STACK_BOUNDS());
                _PyFrame_SetStackPointer(frame, stack_pointer);
                PyStackRef_CLOSE(update);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(update);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _STORE_ATTR_WITH_HINT. */

        /* Cannot outline _STORE_ATTR_SLOT. */

        extern _JITOutlinedReturnVal _COMPARE_OP_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert((oparg >> 5) <= Py_GE);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = PyObject_RichCompare(left_o, right_o, oparg >> 5);
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
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            if (oparg & 16) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                int res_bool = PyObject_IsTrue(res_o);
                Py_DECREF(res_o);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                if (res_bool < 0) {
                    _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                    return _retval;

                }
                res = res_bool ? PyStackRef_True : PyStackRef_False;
            }
            else {
                res = PyStackRef_FromPyObjectSteal(res_o);
            }
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _IS_OP. */

        extern _JITOutlinedReturnVal _CONTAINS_OP_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef b;
            oparg = CURRENT_OPARG();
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int res = PySequence_Contains(right_o, left_o);
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
            if (res < 0) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            b = (res ^ oparg) ? PyStackRef_True : PyStackRef_False;
            stack_pointer[0] = b;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _GUARD_TOS_ANY_SET. */

        extern _JITOutlinedReturnVal _CONTAINS_OP_SET_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef b;
            oparg = CURRENT_OPARG();
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyAnySet_CheckExact(right_o));
            STAT_INC(CONTAINS_OP, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int res = _PySet_Contains((PySetObject *)right_o, left_o);
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
            if (res < 0) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            b = (res ^ oparg) ? PyStackRef_True : PyStackRef_False;
            stack_pointer[0] = b;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _CONTAINS_OP_DICT_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef b;
            oparg = CURRENT_OPARG();
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyDict_CheckExact(right_o));
            STAT_INC(CONTAINS_OP, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int res = PyDict_Contains(right_o, left_o);
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
            if (res < 0) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            b = (res ^ oparg) ? PyStackRef_True : PyStackRef_False;
            stack_pointer[0] = b;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _CHECK_EG_MATCH_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef match_type_st;
            _PyStackRef exc_value_st;
            _PyStackRef rest;
            _PyStackRef match;
            match_type_st = stack_pointer[-1];
            exc_value_st = stack_pointer[-2];
            PyObject *exc_value = PyStackRef_AsPyObjectBorrow(exc_value_st);
            PyObject *match_type = PyStackRef_AsPyObjectBorrow(match_type_st);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int err = _PyEval_CheckExceptStarTypeValid(tstate, match_type);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (err < 0) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyStackRef tmp = match_type_st;
                match_type_st = PyStackRef_NULL;
                stack_pointer[-1] = match_type_st;
                PyStackRef_CLOSE(tmp);
                tmp = exc_value_st;
                exc_value_st = PyStackRef_NULL;
                stack_pointer[-2] = exc_value_st;
                PyStackRef_CLOSE(tmp);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -2;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            PyObject *match_o = NULL;
            PyObject *rest_o = NULL;
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int res = _PyEval_ExceptionGroupMatch(frame, exc_value, match_type,
                &match_o, &rest_o);
            _PyStackRef tmp = match_type_st;
            match_type_st = PyStackRef_NULL;
            stack_pointer[-1] = match_type_st;
            PyStackRef_CLOSE(tmp);
            tmp = exc_value_st;
            exc_value_st = PyStackRef_NULL;
            stack_pointer[-2] = exc_value_st;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            if (res < 0) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            assert((match_o == NULL) == (rest_o == NULL));
            if (match_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            if (!Py_IsNone(match_o)) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                PyErr_SetHandledException(match_o);
                stack_pointer = _PyFrame_GetStackPointer(frame);
            }
            rest = PyStackRef_FromPyObjectSteal(rest_o);
            match = PyStackRef_FromPyObjectSteal(match_o);
            stack_pointer[0] = rest;
            stack_pointer[1] = match;
            stack_pointer += 2;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _CHECK_EXC_MATCH_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef right;
            _PyStackRef left;
            _PyStackRef b;
            right = stack_pointer[-1];
            left = stack_pointer[-2];
            PyObject *left_o = PyStackRef_AsPyObjectBorrow(left);
            PyObject *right_o = PyStackRef_AsPyObjectBorrow(right);
            assert(PyExceptionInstance_Check(left_o));
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int err = _PyEval_CheckExceptTypeValid(tstate, right_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (err < 0) {_JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            _PyFrame_SetStackPointer(frame, stack_pointer);
            int res = PyErr_GivenExceptionMatches(left_o, right_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(right);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            b = res ? PyStackRef_True : PyStackRef_False;
            stack_pointer[0] = b;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _ITER_CHECK_RANGE. */

        /* _ITER_JUMP_RANGE is not a viable micro-op for tier 2 because it is replaced */

        /* Cannot outline _GUARD_NOT_EXHAUSTED_RANGE. */

        extern _JITOutlinedReturnVal _ITER_NEXT_RANGE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef iter;
            _PyStackRef next;
            iter = stack_pointer[-1];
            _PyRangeIterObject *r = (_PyRangeIterObject *)PyStackRef_AsPyObjectBorrow(iter);
            assert(Py_TYPE(r) == &PyRangeIter_Type);
            #ifdef Py_GIL_DISABLED
            assert(_PyObject_IsUniquelyReferenced((PyObject *)r));
            #endif
            assert(r->len > 0);
            long value = r->start;
            r->start = value + r->step;
            r->len--;
            PyObject *res = PyLong_FromLong(value);
            if (res == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            next = PyStackRef_FromPyObjectSteal(res);
            stack_pointer[0] = next;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _FOR_ITER_GEN_FRAME. */

        /* Cannot outline _INSERT_NULL. */

        /* Cannot outline _LOAD_SPECIAL. */

        extern _JITOutlinedReturnVal _WITH_EXCEPT_START_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef val;
            _PyStackRef lasti;
            _PyStackRef exit_self;
            _PyStackRef exit_func;
            _PyStackRef res;
            val = stack_pointer[-1];
            lasti = stack_pointer[-3];
            exit_self = stack_pointer[-4];
            exit_func = stack_pointer[-5];
            PyObject *exc, *tb;
            PyObject *val_o = PyStackRef_AsPyObjectBorrow(val);
            PyObject *exit_func_o = PyStackRef_AsPyObjectBorrow(exit_func);
            assert(val_o && PyExceptionInstance_Check(val_o));
            exc = PyExceptionInstance_Class(val_o);
            PyObject *original_tb = tb = PyException_GetTraceback(val_o);
            if (tb == NULL) {
                tb = Py_None;
            }
            assert(PyStackRef_IsTaggedInt(lasti));
            (void)lasti;
            PyObject *stack[5] = {NULL, PyStackRef_AsPyObjectBorrow(exit_self), exc, val_o, tb};
            int has_self = !PyStackRef_IsNull(exit_self);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = PyObject_Vectorcall(exit_func_o, stack + 2 - has_self,
                (3 + has_self) | PY_VECTORCALL_ARGUMENTS_OFFSET, NULL);
            Py_XDECREF(original_tb);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _MAYBE_EXPAND_METHOD. */

        /* _DO_CALL is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* _MONITOR_CALL is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        extern _JITOutlinedReturnVal _PY_FRAME_GENERAL_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = CURRENT_OPARG();
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            int total_args = oparg;
            if (!PyStackRef_IsNull(self_or_null)) {
                args--;
                total_args++;
            }
            assert(Py_TYPE(callable_o) == &PyFunction_Type);
            int code_flags = ((PyCodeObject*)PyFunction_GET_CODE(callable_o))->co_flags;
            PyObject *locals = code_flags & CO_OPTIMIZED ? NULL : Py_NewRef(PyFunction_GET_GLOBALS(callable_o));
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyInterpreterFrame *temp = _PyEvalFramePushAndInit(
                tstate, callable, locals,
                args, total_args, NULL, frame
            );
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            if (temp == NULL) {_JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            new_frame = temp;
            stack_pointer[0].bits = (uintptr_t)new_frame;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _CHECK_IS_NOT_PY_CALLABLE. */

        extern _JITOutlinedReturnVal _CALL_NON_PY_GENERAL_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            args = &stack_pointer[-oparg];
            self_or_null = stack_pointer[-1 - oparg];
            callable = stack_pointer[-2 - oparg];
            #if TIER_ONE
            assert(opcode != INSTRUMENTED_CALL);
            #endif
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            int total_args = oparg;
            _PyStackRef *arguments = args;
            if (!PyStackRef_IsNull(self_or_null)) {
                arguments--;
                total_args++;
            }
            STACKREFS_TO_PYOBJECTS(arguments, total_args, args_o);
            if (CONVERSION_FAILED(args_o)) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyStackRef tmp;
                for (int _i = oparg; --_i >= 0;) {
                    tmp = args[_i];
                    args[_i] = PyStackRef_NULL;
                    PyStackRef_CLOSE(tmp);
                }
                tmp = self_or_null;
                self_or_null = PyStackRef_NULL;
                stack_pointer[-1 - oparg] = self_or_null;
                PyStackRef_XCLOSE(tmp);
                tmp = callable;
                callable = PyStackRef_NULL;
                stack_pointer[-2 - oparg] = callable;
                PyStackRef_CLOSE(tmp);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -2 - oparg;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = PyObject_Vectorcall(
                callable_o, args_o,
                total_args | PY_VECTORCALL_ARGUMENTS_OFFSET,
                NULL);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            STACKREFS_TO_PYOBJECTS_CLEANUP(args_o);
            assert((res_o != NULL) ^ (_PyErr_Occurred(tstate) != NULL));
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp;
            for (int _i = oparg; --_i >= 0;) {
                tmp = args[_i];
                args[_i] = PyStackRef_NULL;
                PyStackRef_CLOSE(tmp);
            }
            tmp = self_or_null;
            self_or_null = PyStackRef_NULL;
            stack_pointer[-1 - oparg] = self_or_null;
            PyStackRef_XCLOSE(tmp);
            tmp = callable;
            callable = PyStackRef_NULL;
            stack_pointer[-2 - oparg] = callable;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _GUARD_CALLABLE_STR_1. */

        extern _JITOutlinedReturnVal _CALL_STR_1_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
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
            STAT_INC(CALL, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = PyObject_Str(arg_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            (void)callable;
            (void)null;
            stack_pointer += -3;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(arg);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _GUARD_CALLABLE_TUPLE_1. */

        extern _JITOutlinedReturnVal _CALL_TUPLE_1_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
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
            STAT_INC(CALL, hit);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = PySequence_Tuple(arg_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            (void)callable;
            (void)null;
            stack_pointer += -3;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(arg);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _CHECK_AND_ALLOCATE_OBJECT. */

        extern _JITOutlinedReturnVal _CREATE_INIT_FRAME_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef self;
            _PyStackRef init;
            _PyInterpreterFrame *init_frame;
            oparg = CURRENT_OPARG();
            args = &stack_pointer[-oparg];
            self = stack_pointer[-1 - oparg];
            init = stack_pointer[-2 - oparg];
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyInterpreterFrame *shim = _PyFrame_PushTrampolineUnchecked(
                tstate, (PyCodeObject *)&_Py_InitCleanup, 1, frame);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            assert(_PyFrame_GetBytecode(shim)[0].op.code == EXIT_INIT_CHECK);
            assert(_PyFrame_GetBytecode(shim)[1].op.code == RETURN_VALUE);
            shim->localsplus[0] = PyStackRef_DUP(self);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyInterpreterFrame *temp = _PyEvalFramePushAndInit(
                tstate, init, NULL, args-1, oparg+1, NULL, shim);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            if (temp == NULL) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyEval_FrameClearAndPop(tstate, shim);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            init_frame = temp;
            frame->return_offset = 1 + INLINE_CACHE_ENTRIES_CALL;
            tstate->py_recursion_remaining--;
            stack_pointer[0].bits = (uintptr_t)init_frame;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _EXIT_INIT_CHECK. */

        /* Cannot outline _CALL_BUILTIN_CLASS. */

        /* Cannot outline _CALL_BUILTIN_O. */

        /* Cannot outline _CALL_BUILTIN_FAST. */

        /* Cannot outline _CALL_BUILTIN_FAST_WITH_KEYWORDS. */

        /* Cannot outline _GUARD_CALLABLE_LEN. */

        extern _JITOutlinedReturnVal _CALL_LEN_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef arg;
            _PyStackRef null;
            _PyStackRef callable;
            _PyStackRef res;
            arg = stack_pointer[-1];
            null = stack_pointer[-2];
            callable = stack_pointer[-3];
            (void)null;
            STAT_INC(CALL, hit);
            PyObject *arg_o = PyStackRef_AsPyObjectBorrow(arg);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            Py_ssize_t len_i = PyObject_Length(arg_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (len_i < 0) {_JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            PyObject *res_o = PyLong_FromSsize_t(len_i);
            assert((res_o != NULL) ^ (_PyErr_Occurred(tstate) != NULL));
            if (res_o == NULL) {_JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(arg);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(callable);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _CALL_ISINSTANCE. */

        /* Cannot outline _CALL_LIST_APPEND. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_O. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_FAST_WITH_KEYWORDS. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_NOARGS. */

        /* Cannot outline _CALL_METHOD_DESCRIPTOR_FAST. */

        /* _MONITOR_CALL_KW is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        /* Cannot outline _MAYBE_EXPAND_METHOD_KW. */

        /* _DO_CALL_KW is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        extern _JITOutlinedReturnVal _PY_FRAME_KW_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef kwnames;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyInterpreterFrame *new_frame;
            oparg = CURRENT_OPARG();
            kwnames = stack_pointer[-1];
            args = &stack_pointer[-1 - oparg];
            self_or_null = stack_pointer[-2 - oparg];
            callable = stack_pointer[-3 - oparg];
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            int total_args = oparg;
            _PyStackRef *arguments = args;
            if (!PyStackRef_IsNull(self_or_null)) {
                arguments--;
                total_args++;
            }
            PyObject *kwnames_o = PyStackRef_AsPyObjectBorrow(kwnames);
            int positional_args = total_args - (int)PyTuple_GET_SIZE(kwnames_o);
            assert(Py_TYPE(callable_o) == &PyFunction_Type);
            int code_flags = ((PyCodeObject*)PyFunction_GET_CODE(callable_o))->co_flags;
            PyObject *locals = code_flags & CO_OPTIMIZED ? NULL : Py_NewRef(PyFunction_GET_GLOBALS(callable_o));
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyInterpreterFrame *temp = _PyEvalFramePushAndInit(
                tstate, callable, locals,
                arguments, positional_args, kwnames_o, frame
            );
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(kwnames);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            if (temp == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            new_frame = temp;
            stack_pointer[0].bits = (uintptr_t)new_frame;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _CHECK_IS_NOT_PY_CALLABLE_KW. */

        extern _JITOutlinedReturnVal _CALL_KW_NON_PY_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef kwnames;
            _PyStackRef *args;
            _PyStackRef self_or_null;
            _PyStackRef callable;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            kwnames = stack_pointer[-1];
            args = &stack_pointer[-1 - oparg];
            self_or_null = stack_pointer[-2 - oparg];
            callable = stack_pointer[-3 - oparg];
            #if TIER_ONE
            assert(opcode != INSTRUMENTED_CALL);
            #endif
            PyObject *callable_o = PyStackRef_AsPyObjectBorrow(callable);
            int total_args = oparg;
            _PyStackRef *arguments = args;
            if (!PyStackRef_IsNull(self_or_null)) {
                arguments--;
                total_args++;
            }
            STACKREFS_TO_PYOBJECTS(arguments, total_args, args_o);
            if (CONVERSION_FAILED(args_o)) {
                _PyFrame_SetStackPointer(frame, stack_pointer);
                _PyStackRef tmp = kwnames;
                kwnames = PyStackRef_NULL;
                stack_pointer[-1] = kwnames;
                PyStackRef_CLOSE(tmp);
                for (int _i = oparg; --_i >= 0;) {
                    tmp = args[_i];
                    args[_i] = PyStackRef_NULL;
                    PyStackRef_CLOSE(tmp);
                }
                tmp = self_or_null;
                self_or_null = PyStackRef_NULL;
                stack_pointer[-2 - oparg] = self_or_null;
                PyStackRef_XCLOSE(tmp);
                tmp = callable;
                callable = PyStackRef_NULL;
                stack_pointer[-3 - oparg] = callable;
                PyStackRef_CLOSE(tmp);
                stack_pointer = _PyFrame_GetStackPointer(frame);
                stack_pointer += -3 - oparg;
                assert(WITHIN_STACK_BOUNDS());
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            PyObject *kwnames_o = PyStackRef_AsPyObjectBorrow(kwnames);
            int positional_args = total_args - (int)PyTuple_GET_SIZE(kwnames_o);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = PyObject_Vectorcall(
                callable_o, args_o,
                positional_args | PY_VECTORCALL_ARGUMENTS_OFFSET,
                kwnames_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(kwnames);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            STACKREFS_TO_PYOBJECTS_CLEANUP(args_o);
            assert((res_o != NULL) ^ (_PyErr_Occurred(tstate) != NULL));
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp;
            for (int _i = oparg; --_i >= 0;) {
                tmp = args[_i];
                args[_i] = PyStackRef_NULL;
                PyStackRef_CLOSE(tmp);
            }
            tmp = self_or_null;
            self_or_null = PyStackRef_NULL;
            stack_pointer[-1 - oparg] = self_or_null;
            PyStackRef_XCLOSE(tmp);
            tmp = callable;
            callable = PyStackRef_NULL;
            stack_pointer[-2 - oparg] = callable;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -2 - oparg;
            assert(WITHIN_STACK_BOUNDS());
            if (res_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _MAKE_CALLARGS_A_TUPLE. */

        /* _DO_CALL_FUNCTION_EX is not a viable micro-op for tier 2 because it uses the 'this_instr' variable */

        extern _JITOutlinedReturnVal _MAKE_FUNCTION_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef codeobj_st;
            _PyStackRef func;
            codeobj_st = stack_pointer[-1];
            PyObject *codeobj = PyStackRef_AsPyObjectBorrow(codeobj_st);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyFunctionObject *func_obj = (PyFunctionObject *)
            PyFunction_New(codeobj, GLOBALS());
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(codeobj_st);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (func_obj == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            _PyFunction_SetVersion(
                                   func_obj, ((PyCodeObject *)codeobj)->co_version);
            func = PyStackRef_FromPyObjectSteal((PyObject *)func_obj);
            stack_pointer[0] = func;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _RETURN_GENERATOR_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef res;
            assert(PyStackRef_FunctionCheck(frame->f_funcobj));
            PyFunctionObject *func = (PyFunctionObject *)PyStackRef_AsPyObjectBorrow(frame->f_funcobj);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyGenObject *gen = (PyGenObject *)_Py_MakeCoro(func);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (gen == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            assert(STACK_LEVEL() == 0);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyInterpreterFrame *gen_frame = &gen->gi_iframe;
            frame->instr_ptr++;
            _PyFrame_Copy(frame, gen_frame);
            assert(frame->frame_obj == NULL);
            gen->gi_frame_state = FRAME_CREATED;
            gen_frame->owner = FRAME_OWNED_BY_GENERATOR;
            _Py_LeaveRecursiveCallPy(tstate);
            _PyInterpreterFrame *prev = frame->previous;
            _PyThreadState_PopFrame(tstate, frame);
            frame = tstate->current_frame = prev;
            LOAD_IP(frame->return_offset);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            res = PyStackRef_FromPyObjectStealMortal((PyObject *)gen);
            LLTRACE_RESUME_FRAME();
            stack_pointer[0] = res;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _BUILD_SLICE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef *args;
            _PyStackRef slice;
            oparg = CURRENT_OPARG();
            args = &stack_pointer[-oparg];
            PyObject *start_o = PyStackRef_AsPyObjectBorrow(args[0]);
            PyObject *stop_o = PyStackRef_AsPyObjectBorrow(args[1]);
            PyObject *step_o = oparg == 3 ? PyStackRef_AsPyObjectBorrow(args[2]) : NULL;
            PyObject *slice_o = PySlice_New(start_o, stop_o, step_o);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp;
            for (int _i = oparg; --_i >= 0;) {
                tmp = args[_i];
                args[_i] = PyStackRef_NULL;
                PyStackRef_CLOSE(tmp);
            }
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -oparg;
            assert(WITHIN_STACK_BOUNDS());
            if (slice_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            slice = PyStackRef_FromPyObjectStealMortal(slice_o);
            stack_pointer[0] = slice;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        extern _JITOutlinedReturnVal _CONVERT_VALUE_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef value;
            _PyStackRef result;
            oparg = CURRENT_OPARG();
            value = stack_pointer[-1];
            conversion_func conv_fn;
            assert(oparg >= FVC_STR && oparg <= FVC_ASCII);
            conv_fn = _PyEval_ConversionFuncs[oparg];
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *result_o = conv_fn(PyStackRef_AsPyObjectBorrow(value));
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyStackRef_CLOSE(value);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (result_o == NULL) {
                _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;

            }
            result = PyStackRef_FromPyObjectSteal(result_o);
            stack_pointer[0] = result;
            stack_pointer += 1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

        /* Cannot outline _FORMAT_SIMPLE. */

        /* Cannot outline _FORMAT_WITH_SPEC. */

        /* Cannot outline _COPY. */

        extern _JITOutlinedReturnVal _BINARY_OP_outlined(JIT_PARAMS, int _oparg, uint64_t _operand0, uint64_t _operand1, _PyExecutorObject *current_executor) {
            int oparg;
            _PyStackRef rhs;
            _PyStackRef lhs;
            _PyStackRef res;
            oparg = CURRENT_OPARG();
            rhs = stack_pointer[-1];
            lhs = stack_pointer[-2];
            PyObject *lhs_o = PyStackRef_AsPyObjectBorrow(lhs);
            PyObject *rhs_o = PyStackRef_AsPyObjectBorrow(rhs);
            assert(_PyEval_BinaryOps[oparg]);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            PyObject *res_o = _PyEval_BinaryOps[oparg](lhs_o, rhs_o);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            if (res_o == NULL) {_JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 1};
                return _retval;
                
            }
            res = PyStackRef_FromPyObjectSteal(res_o);
            _PyFrame_SetStackPointer(frame, stack_pointer);
            _PyStackRef tmp = lhs;
            lhs = res;
            stack_pointer[-2] = lhs;
            PyStackRef_CLOSE(tmp);
            tmp = rhs;
            rhs = PyStackRef_NULL;
            stack_pointer[-1] = rhs;
            PyStackRef_CLOSE(tmp);
            stack_pointer = _PyFrame_GetStackPointer(frame);
            stack_pointer += -1;
            assert(WITHIN_STACK_BOUNDS());
            _JITOutlinedReturnVal _retval = {frame, stack_pointer, tstate, 0};
            return _retval;
        }

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

