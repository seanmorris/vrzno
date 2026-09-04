#include "vrzno_private.h"

/* Legacy compatibility helper. */
PHP_FUNCTION(vrzno_eval)
{
	zend_string *retval;
	char   *js_code = "";
	size_t  js_code_len = sizeof(js_code) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_code, js_code_len)
	ZEND_PARSE_PARAMETERS_END();

	char *js_ret = EM_ASM_PTR({
		try
		{
			const str = String(eval(UTF8ToString($0)));
			const len = lengthBytesUTF8(str) + 1;
			const loc = _malloc(len);

			stringToUTF8(str, loc, len);

			return loc;
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
			return 0;
		}

	}, js_code);

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

	char *js_ret = EM_ASM_PTR({

		const funcName = UTF8ToString($0);
		const argJson  = UTF8ToString($1);

		try
		{
			const func = globalThis[funcName];
			if(typeof func !== 'function')
			{
				throw new TypeError(`${funcName} is not a global JavaScript function`);
			}
			const args = JSON.parse(argJson || '[]') || [];

			const str = String(func(...args));
			const len = lengthBytesUTF8(str) + 1;
			const loc = _malloc(len);

			stringToUTF8(str, loc, len);

			return loc;
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
			return 0;
		}

	}, js_funcname, js_args);

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
	zend_fcall_info_cache fcc;
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

	if(!zend_is_callable_ex(callback, NULL, 0, NULL, &fcc, NULL))
	{
		zend_argument_type_error(2, "must be a valid callback, %s given", zend_zval_type_name(callback));
		RETURN_THROWS();
	}

	zval *owned_callback = vrzno_expose_copy_zval(callback);

	EM_ASM({
		const timeout = $0;
		const ownedCallback = $1;
		const generation = Module.vrznoGeneration;
		const token = {};
		Module.ownedZvalRegistry.register(token, ownedCallback, token);

		setTimeout(()=>{
			try
			{
				if(generation !== Module.vrznoGeneration)
				{
					return;
				}

				const zv = Module.ccall(
					'vrzno_exec_zval_callback'
					, 'number'
					, ['number','number','number']
					, [ownedCallback, 0, 0]
				);

				Module.vrznoDestroyZval(zv);
			}
			finally
			{
				Module.ownedZvalRegistry.release(token);
			}

		}, timeout);

	}, timeout, owned_callback);
}

EM_ASYNC_JS(void, vrzno_await_internal, (vrzno_target_id targetId, zval *rv), {
	try
	{
		const target = Module.targets.get(targetId);
		const result = await target;
		Module.jsToZval(result, rv);
	}
	catch(error)
	{
		Module.vrznoThrowRuntimeError(error);
	}
});

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
	EM_ASM({
		try
		{
			const name = UTF8ToString($0);
			Module.jsToZval(Module[name], $1);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}
	}, name, return_value);

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
	EM_ASM({
		try
		{
			const name = UTF8ToString($0);
			Module.jsToZval(Module.shared[name], $1);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}
	}, name, return_value);

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
	EM_ASM({
		try
		{
			const name = UTF8ToString($0);
			Module.jsToZval(import(name), $1);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}
	}, name, return_value);

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
