ZEND_BEGIN_ARG_INFO(arginfo_vrzno_eval, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_run, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_timeout, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_await, 0)
	ZEND_ARG_INFO(0, vrzno_class)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_env, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_shared, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_import, 0)
	ZEND_ARG_INFO(0, vrzno_class)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_target, 0)
	ZEND_ARG_INFO(0, vrzno_class)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_zval, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

static const zend_function_entry vrzno_functions[] = {
	PHP_FE(vrzno_eval,    arginfo_vrzno_eval)
	PHP_FE(vrzno_run,     arginfo_vrzno_run)
	PHP_FE(vrzno_timeout, arginfo_vrzno_timeout)
	PHP_FE(vrzno_await,   arginfo_vrzno_await)
	PHP_FE(vrzno_env,     arginfo_vrzno_env)
	PHP_FE(vrzno_shared,  arginfo_vrzno_shared)
	PHP_FE(vrzno_import,  arginfo_vrzno_import)
	PHP_FE(vrzno_target,  arginfo_vrzno_target)
	PHP_FE(vrzno_zval,    arginfo_vrzno_zval)
	PHP_FE_END
};

/* Deprecate! */
PHP_FUNCTION(vrzno_eval)
{
	zend_string *retval;
	char   *js_code = "";
	size_t  js_code_len = sizeof(js_code) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_code, js_code_len)
	ZEND_PARSE_PARAMETERS_END();

	char *js_ret = EM_ASM_PTR({
		const str = String(eval(UTF8ToString($0)));
		const len = lengthBytesUTF8(str) + 1;
		const loc = _malloc(len);

		stringToUTF8(str, loc, len);

		return loc;

	}, js_code);

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}

/* Deprecate! */
PHP_FUNCTION(vrzno_run)
{
	zend_string *retval;
	zend_long opt = 0;

	char   *js_funcname     = "";
	size_t  js_funcname_len = sizeof(js_funcname) - 1;
	zval   *js_argv;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STRING(js_funcname, js_funcname_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(js_argv)
	ZEND_PARSE_PARAMETERS_END();

	smart_str buf = {0};

	php_json_encoder  encoder;
	php_json_encode_init(&encoder);
	encoder.max_depth = PHP_JSON_PARSER_DEFAULT_DEPTH;
	php_json_encode_zval(&buf, js_argv, opt, &encoder);

	char *js_args = ZSTR_VAL(buf.s);

	smart_str_0(&buf);

	char *js_ret = EM_ASM_PTR({

		const funcName = UTF8ToString($0);
		const argJson  = UTF8ToString($1);

		const func = globalThis[funcName];
		const args = JSON.parse(argJson || '[]') || [];

		const str = String(func(...args));
		const len = lengthBytesUTF8(str) + 1;
		const loc = _malloc(len);

		stringToUTF8(str, loc, len);

		return loc;

	}, js_funcname, js_args);

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}

/* Deprecate! */
PHP_FUNCTION(vrzno_timeout)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	char   *timeout     = "";
	size_t  timeout_len = sizeof(timeout) - 1;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(timeout, timeout_len)
		Z_PARAM_FUNC(fci, fcc)
	ZEND_PARSE_PARAMETERS_END();

	EM_ASM({
		const timeout = Number(UTF8ToString($0));
		const funcPtr = $1;

		setTimeout(()=>{
			const zv = Module.ccall(
				'vrzno_exec_callback'
				, 'number'
				, ['number','number','number','number']
				, [funcPtr, null, 0, 0]
			);

			Module.ccall(
				'vrzno_expose_efree'
				, 'number'
				, ["number"]
				, [zv]
			);

			Module.ccall(
				'vrzno_del_callback'
				, 'number'
				, ["number"]
				, [funcPtr]
			);

		}, timeout);

	}, timeout, fcc.function_handler);

	GC_ADDREF(ZEND_CLOSURE_OBJECT(fcc.function_handler));
}

EM_ASYNC_JS(void, vrzno_await_internal, (jstarget *targetId, zval *rv), {
	const target = Module.targets.get(targetId);
	const result = await target;
	Module.jsToZval(result, rv);
});

PHP_FUNCTION(vrzno_await)
{
	zval *zv;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zv, vrzno_class_entry)
	ZEND_PARSE_PARAMETERS_END();

	vrzno_await_internal(vrzno_fetch_object(Z_OBJ_P(zv))->targetId, return_value);
}

PHP_FUNCTION(vrzno_env)
{
	char   *name = "";
	size_t  name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	EM_ASM({
		const name = UTF8ToString($0);
		const rv = $1;
		Module.jsToZval(Module[name], rv);
	}, name, return_value);
}

PHP_FUNCTION(vrzno_shared)
{
	char   *name = "";
	size_t  name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	EM_ASM({
		const name = UTF8ToString($0);
		const rv = $1;
		Module.jsToZval(Module.shared[name], rv);
	}, name, return_value);
}

PHP_FUNCTION(vrzno_import)
{
	char *name;
	size_t name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	EM_ASM({
		const name = UTF8ToString($0);
		const rv = $1;
		Module.jsToZval(import(name), rv);
	}, name, return_value);
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

	if(Z_REFCOUNTED_P(zv))
	{
		Z_ADDREF_P(zv);
	}

	ZVAL_LONG(return_value, zv);
}
