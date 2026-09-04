/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 1d950f3a00b3031a874231f9f6287a1a2486bd00 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vrzno_eval, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, code, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vrzno_run, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, global_function_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, args, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vrzno_timeout, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, milliseconds, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vrzno_await, 0, 1, IS_MIXED, 0)
	ZEND_ARG_OBJ_INFO(0, promise_like, Vrzno, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vrzno_env, 0, 1, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_vrzno_shared arginfo_vrzno_env

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_vrzno_import, 0, 1, Vrzno, 0)
	ZEND_ARG_TYPE_INFO(0, module_url, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vrzno_target, 0, 1, IS_LONG, 0)
	ZEND_ARG_OBJ_INFO(0, value, Vrzno, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_vrzno_zval, 0, 1, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Vrzno___construct, 0, 0, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, args, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Vrzno___get, 0, 1, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, property_name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Vrzno___call, 0, 2, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, method_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, args, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Vrzno___invoke, 0, 0, IS_MIXED, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, args, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Vrzno___toString, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()


ZEND_FUNCTION(vrzno_eval);
ZEND_FUNCTION(vrzno_run);
ZEND_FUNCTION(vrzno_timeout);
ZEND_FUNCTION(vrzno_await);
ZEND_FUNCTION(vrzno_env);
ZEND_FUNCTION(vrzno_shared);
ZEND_FUNCTION(vrzno_import);
ZEND_FUNCTION(vrzno_target);
ZEND_FUNCTION(vrzno_zval);
ZEND_METHOD(Vrzno, __construct);
ZEND_METHOD(Vrzno, __get);
ZEND_METHOD(Vrzno, __call);
ZEND_METHOD(Vrzno, __invoke);
ZEND_METHOD(Vrzno, __toString);


static const zend_function_entry ext_functions[] = {
	ZEND_FE(vrzno_eval, arginfo_vrzno_eval)
	ZEND_FE(vrzno_run, arginfo_vrzno_run)
	ZEND_FE(vrzno_timeout, arginfo_vrzno_timeout)
	ZEND_FE(vrzno_await, arginfo_vrzno_await)
	ZEND_FE(vrzno_env, arginfo_vrzno_env)
	ZEND_FE(vrzno_shared, arginfo_vrzno_shared)
	ZEND_FE(vrzno_import, arginfo_vrzno_import)
	ZEND_FE(vrzno_target, arginfo_vrzno_target)
	ZEND_FE(vrzno_zval, arginfo_vrzno_zval)
	ZEND_FE_END
};


static const zend_function_entry class_Vrzno_methods[] = {
	ZEND_ME(Vrzno, __construct, arginfo_class_Vrzno___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(Vrzno, __get, arginfo_class_Vrzno___get, ZEND_ACC_PUBLIC)
	ZEND_ME(Vrzno, __call, arginfo_class_Vrzno___call, ZEND_ACC_PUBLIC)
	ZEND_ME(Vrzno, __invoke, arginfo_class_Vrzno___invoke, ZEND_ACC_PUBLIC)
	ZEND_ME(Vrzno, __toString, arginfo_class_Vrzno___toString, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};
