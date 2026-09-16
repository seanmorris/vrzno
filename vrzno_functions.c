#include "vrzno_private.h"
#include <vrzno_functions_js.h>

/* Legacy compatibility helper. */
PHP_FUNCTION(vrzno_eval)
{
	zend_string *retval;
	char   *js_code = "";
	size_t  js_code_len = sizeof(js_code) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_code, js_code_len)
	ZEND_PARSE_PARAMETERS_END();

	char *js_ret = vrzno_js_eval(js_code);

	if(!js_ret)
	{
		RETURN_THROWS();
	}

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}

/* Legacy compatibility helper. */
PHP_FUNCTION(vrzno_run)
{
	zend_string *retval;
	zend_long opt = 0;

	char   *js_funcname     = "";
	size_t  js_funcname_len = sizeof(js_funcname) - 1;
	zval   *js_argv = NULL;
	zval empty_argv;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STRING(js_funcname, js_funcname_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(js_argv)
	ZEND_PARSE_PARAMETERS_END();

	if(!js_argv)
	{
		array_init(&empty_argv);
		js_argv = &empty_argv;
	}

	smart_str buf = {0};

	php_json_encoder  encoder;
	php_json_encode_init(&encoder);
	encoder.max_depth = PHP_JSON_PARSER_DEFAULT_DEPTH;
	php_json_encode_zval(&buf, js_argv, opt, &encoder);

	smart_str_0(&buf);
	char *js_args = ZSTR_VAL(buf.s);

	char *js_ret = vrzno_js_run(js_funcname, js_args);

	if(js_argv == &empty_argv)
	{
		zval_ptr_dtor(&empty_argv);
	}

	smart_str_free(&buf);

	if(!js_ret)
	{
		RETURN_THROWS();
	}

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}

/* Legacy compatibility helper. */
PHP_FUNCTION(vrzno_timeout)
{
	zval *callback;
	zend_long timeout;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(timeout)
		Z_PARAM_ZVAL(callback)
	ZEND_PARSE_PARAMETERS_END();

	if(timeout < 0)
	{
		zend_argument_value_error(1, "must be greater than or equal to 0");
		RETURN_THROWS();
	}

	if(!zend_is_callable(callback, 0, NULL))
	{
		zend_argument_type_error(2, "must be a valid callback, %s given", zend_zval_type_name(callback));
		RETURN_THROWS();
	}

	zval *owned_callback = vrzno_expose_copy_zval(callback);

	vrzno_js_timeout(timeout, owned_callback);
}

PHP_FUNCTION(vrzno_await)
{
	zval *zv;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zv, vrzno_class_entry)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_NULL(return_value);
	vrzno_await_internal(vrzno_fetch_object(Z_OBJ_P(zv))->targetId, return_value);

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_FUNCTION(vrzno_env)
{
	char   *name = "";
	size_t  name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_NULL(return_value);
	vrzno_js_env(name, return_value);

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_FUNCTION(vrzno_shared)
{
	char   *name = "";
	size_t  name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_NULL(return_value);
	vrzno_js_shared(name, return_value);

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_FUNCTION(vrzno_import)
{
	char *name;
	size_t name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_NULL(return_value);
	vrzno_js_import(name, return_value);

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_FUNCTION(vrzno_target)
{
	zval *zv;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zv, vrzno_class_entry)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_LONG(return_value, vrzno_fetch_object(Z_OBJ_P(zv))->targetId);
}

PHP_FUNCTION(vrzno_zval)
{
	zval *zv;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ZVAL(zv)
	ZEND_PARSE_PARAMETERS_END();

	zval *owned = vrzno_expose_copy_zval(zv);
	ZVAL_LONG(return_value, (zend_long) (uintptr_t) owned);
}
