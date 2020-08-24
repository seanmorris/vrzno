/* vrzno extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_vrzno.h"
#include "../json/php_json.h"
#include "../json/php_json_encoder.h"
#include "zend_closures.h"
#include <emscripten.h>

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

/* {{{ string vrzno_eval( [ string $js_code ] )
 */
PHP_FUNCTION(vrzno_eval)
{
	zend_string *retval;
	char   *js_code = "";
	size_t  js_code_len = sizeof(js_code) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OPTIONAL
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
/* }}}*/

/* {{{ string vrzno_run( [ string $js_func, $js_argv ] )
 */
PHP_FUNCTION(vrzno_run)
{
	zend_string      *retval;
	php_json_encoder  encoder;
	zend_long         opt = 0;

	char   *js_funcname     = "";
	size_t  js_funcname_len = sizeof(js_funcname) - 1;
	zval   *js_argv;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STRING(js_funcname, js_funcname_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(js_argv)
	ZEND_PARSE_PARAMETERS_END();

	smart_str buf = {0};

	php_json_encode_init(&encoder);
	encoder.max_depth = PHP_JSON_PARSER_DEFAULT_DEPTH;
	php_json_encode_zval(&buf, js_argv, opt, &encoder);

	char *js_args = ZSTR_VAL(buf.s);

	smart_str_0(&buf);

	// char *js_ret  = call_js_func(js_funcname, js_args);

	char *js_ret = EM_ASM_INT({

		const funcName = UTF8ToString($0);
		const argJson  = UTF8ToString($1);

		const func     = window[funcName];
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
/* }}}*/


/* {{{ string vrzno_timeout( [ string $timeout, $callback ] )
 */
PHP_FUNCTION(vrzno_timeout)
{

	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;

	char   *timeout     = "";
	size_t  timeout_len = sizeof(timeout) - 1;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(timeout, timeout_len)
		Z_PARAM_FUNC(fci, fci_cache)
	ZEND_PARSE_PARAMETERS_END();

	EM_ASM({ console.log($0, $1) }, &fci, &fci_cache);

	EM_ASM({
		const timeout  = Number(UTF8ToString($0));
		const funcPtr  = $1;

		setTimeout(()=>{
			console.log(timeout, funcPtr);

			Module.ccall(
				'exec_callback'
				, 'number'
				, ["number"]
				, [funcPtr]
			);

			Module.ccall(
				'del_callback'
				, 'number'
				, ["number"]
				, [funcPtr]
			);

		}, timeout);

	}, timeout, fci_cache.function_handler);

	GC_ADDREF(ZEND_CLOSURE_OBJECT(fci_cache.function_handler));
}
/* }}}*/

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(vrzno)
{
#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(vrzno)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "vrzno support", "enabled");
	php_info_print_table_end();
}
/* }}} */

/* {{{ arginfo
 */
ZEND_BEGIN_ARG_INFO(arginfo_vrzno_eval, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_run, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_vrzno_timeout, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ vrzno_functions[]
 */
static const zend_function_entry vrzno_functions[] = {
	PHP_FE(vrzno_eval,		arginfo_vrzno_eval)
	PHP_FE(vrzno_run,		arginfo_vrzno_run)
	PHP_FE(vrzno_timeout,	arginfo_vrzno_timeout)
	PHP_FE_END
};
/* }}} */

/* {{{ vrzno_module_entry
 */
zend_module_entry vrzno_module_entry = {
	STANDARD_MODULE_HEADER,
	"vrzno",					/* Extension name */
	vrzno_functions,			/* zend_function_entry */
	NULL,							/* PHP_MINIT - Module initialization */
	NULL,							/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(vrzno),			/* PHP_RINIT - Request initialization */
	NULL,							/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(vrzno),			/* PHP_MINFO - Module info */
	PHP_VRZNO_VERSION,		/* Version */
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_VRZNO
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(vrzno)
#endif

int vrzno_exec_callback(zend_function *fptr)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	zval *params;
	zval retval;

	fci.size             = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);
	fci.object           = NULL;
	fci.retval           = &retval;
	fci.params           = params;
	fci.param_count      = 0;
	fci.no_separation    = 1;

	fcc.function_handler = fptr;
	fcc.called_scope     = NULL;
	fcc.object           = NULL;

	if (zend_call_function(&fci, &fcc) == SUCCESS && Z_TYPE(retval) != IS_UNDEF) {

		return 0;
		// if (Z_ISREF(retval)) {
		// 	zend_unwrap_reference(&retval);
		// }

		// ZVAL_COPY_VALUE(return_value, &retval);
	}

	return 1;
}

int vrzno_del_callback(zend_function *fptr)
{
	GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));
	return 0;
}
