ZEND_BEGIN_ARG_INFO(arginfo_vrzno_eval, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_run, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_timeout, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_new, 0)
	ZEND_ARG_INFO(0, vrzno_class)
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
	PHP_FE(vrzno_new,     arginfo_vrzno_new)
	PHP_FE(vrzno_await,   arginfo_vrzno_await)
	PHP_FE(vrzno_env,     arginfo_vrzno_env)
	PHP_FE(vrzno_shared,  arginfo_vrzno_shared)
	PHP_FE(vrzno_import,  arginfo_vrzno_import)
	PHP_FE(vrzno_target,  arginfo_vrzno_target)
	PHP_FE(vrzno_zval,    arginfo_vrzno_zval)
	PHP_FE_END
};

PHP_FUNCTION(vrzno_eval)
{
	zend_string *retval;
	char   *js_code = "";
	size_t  js_code_len = sizeof(js_code) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_code, js_code_len)
	ZEND_PARSE_PARAMETERS_END();

	char *js_ret = EM_ASM_INT({
		const jsRet  = String(eval(UTF8ToString($0)));
		const len    = lengthBytesUTF8(jsRet) + 1;
		const strLoc = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, js_code);

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}

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

	char *js_ret = EM_ASM_INT({

		const funcName = UTF8ToString($0);
		const argJson  = UTF8ToString($1);

		const func     = globalThis[funcName];
		const args     = JSON.parse(argJson || '[]') || [];

		const jsRet    = String(func(...args));
		const len      = lengthBytesUTF8(jsRet) + 1;
		const strLoc   = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, js_funcname, js_args);

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}

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
			Module.ccall(
				'vrzno_exec_callback'
				, 'number'
				, ['number','number','number']
				, [funcPtr, null, 0]
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

EM_ASYNC_JS(zval*, vrzno_await_internal, (long targetId), {
	const target = Module.targets.get(targetId);
	const result = await target;
	return Module.jsToZval(result);
});

PHP_FUNCTION(vrzno_await)
{
	zval *zv;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zv, vrzno_class_entry)
	ZEND_PARSE_PARAMETERS_END();

	long targetId = vrzno_fetch_object(Z_OBJ_P(zv))->targetId;

	zval *js_ret = vrzno_await_internal(targetId);

	ZVAL_NULL(return_value);
	ZVAL_COPY(return_value, js_ret);
}

PHP_FUNCTION(vrzno_env)
{
	char   *name = "";
	size_t  name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	zval *js_ret = EM_ASM_PTR({
		const name = UTF8ToString($0);
		return Module.jsToZval(Module[name]);
	}, name);

	ZVAL_NULL(return_value);
	ZVAL_COPY(return_value, js_ret);
}

PHP_FUNCTION(vrzno_shared)
{
	char   *name = "";
	size_t  name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	zval *js_ret = EM_ASM_PTR({
		const name = UTF8ToString($0);
		return Module.jsToZval(Module.shared[name]);
	}, name);

	ZVAL_NULL(return_value);
	ZVAL_COPY(return_value, js_ret);
}

PHP_FUNCTION(vrzno_new)
{
	zval *zv;
	zval *argv;
	int argc = 0;

	ZEND_PARSE_PARAMETERS_START(1, -1)
		Z_PARAM_OBJECT_OF_CLASS(zv, vrzno_class_entry)
		Z_PARAM_VARIADIC('*', argv, argc)
	ZEND_PARSE_PARAMETERS_END();

	long targetId = vrzno_fetch_object(Z_OBJ_P(zv))->targetId;

	int size = sizeof(zval);

	zval *zvalPtrs = (zval*) emalloc(argc * sizeof(zval));
	int i = 0;

	for(i = 0; i < argc; i++)
	{
		ZVAL_NULL(&zvalPtrs[i]);
        ZVAL_COPY(&zvalPtrs[i], &argv[i]);
	}

	zval *js_ret = EM_ASM_PTR({
		const _class = Module.targets.get($0);
		const argv   = $1;
		const argc   = $2;
		const size   = $3;
		const args   = [];

		for(let i = 0; i < argc; i++)
		{
			args.push(Module.zvalToJS(argv + i * size));
		}

		const _object = new _class(...args);

		return Module.jsToZval(_object);
	}, targetId, zvalPtrs, argc, size);

	ZVAL_NULL(return_value);
	ZVAL_COPY(return_value, js_ret);
}

PHP_FUNCTION(vrzno_import)
{
	zval *js_ret;
	char *name;
	size_t name_len = sizeof(name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(name, name_len)
	ZEND_PARSE_PARAMETERS_END();

	js_ret = EM_ASM_PTR({
		const name = UTF8ToString($0);

		return Module.jsToZval(import(name));

	}, name);

	ZVAL_NULL(return_value);
	ZVAL_COPY(return_value, js_ret);
}

PHP_FUNCTION(vrzno_target)
{
	zval *zv;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zv, vrzno_class_entry)
	ZEND_PARSE_PARAMETERS_END();

	long targetId = vrzno_fetch_object(Z_OBJ_P(zv))->targetId;

	ZVAL_LONG(return_value, targetId);
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
