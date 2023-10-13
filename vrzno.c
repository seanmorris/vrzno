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

typedef struct {
	long        targetId;
	zend_object zo;
} vrzno_object;

static inline vrzno_object *vrzno_fetch_object(zend_object *obj) {
	return (vrzno_object *)((char*)(obj) - XtOffsetOf(vrzno_object, zo));
}

/* {{{ string vrzno_eval( [ string $js_code ] )
 */
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
/* }}}*/

zend_class_entry *vrzno_class_entry;
zend_object_handlers vrzno_object_handlers;

/* {{{ string vrzno_timeout( [ string $timeout, $callback ] )
 */
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
		const timeout  = Number(UTF8ToString($0));
		const funcPtr  = $1;

		setTimeout(()=>{
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

	}, timeout, fcc.function_handler);

	GC_ADDREF(ZEND_CLOSURE_OBJECT(fcc.function_handler));
}
/* }}}*/



static vrzno_object *vrzno_create_object(zend_class_entry *class_type)
{
    vrzno_object *intern = zend_object_alloc(sizeof(vrzno_object), class_type);
    intern->targetId     = 0;

    zend_object_std_init(&intern->zo, class_type);

    intern->zo.handlers = &vrzno_object_handlers;

    return &intern->zo;
}

zval *vrzno_read_property(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv)
{
	char         *name     = ZSTR_VAL(member);
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	long          targetId = vrzno->targetId;

	char *scalar_result = EM_ASM_INT({

		const target   = Module.targets.get($0) || globalThis;

		const property = UTF8ToString($1);
		const result = target[property];

		if(!result || !['function','object'].includes(typeof result))
		{
			const jsRet    = 'OK' + String(result);
			const len      = lengthBytesUTF8(jsRet) + 1;
			const strLoc   = _malloc(len);

			stringToUTF8(jsRet, strLoc, len);

			return strLoc;
		}

		const jsRet    = 'XX';
		const len      = lengthBytesUTF8(jsRet) + 1;
		const strLoc   = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId, name, type);

	if(strlen(scalar_result) > 1 && *scalar_result == 'O')
	{
		ZVAL_STRING(rv, scalar_result + 2);
		free(scalar_result);
		return rv;
	}

	int obj_result = EM_ASM_INT({

		const target   = Module.targets.get($0) || globalThis;
		const property = UTF8ToString($1);
		const result   = target[property];

		if(['function','object'].includes(typeof result))
		{
			let index = Module.targets.getId(result);

			if(!Module.targets.has(result))
			{
				index = Module.targets.add(result);
			}

			return index;
		}

		return 0;

	}, targetId, name);

	if(!obj_result)
	{
		ZVAL_BOOL(rv, obj_result);

		return rv;
	}

	vrzno_object *intern = zend_object_alloc(sizeof(vrzno_object), vrzno_class_entry);
    intern->targetId     = obj_result;

    zend_object_std_init(&intern->zo, vrzno_class_entry);

    intern->zo.handlers = &vrzno_object_handlers;

	ZVAL_OBJ(rv, &intern->zo);
	return rv;

}

ZEND_BEGIN_ARG_INFO_EX(arginfo_addeventlistener, 0, 0, 2)
	ZEND_ARG_INFO(0, eventName)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(1, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_removeeventlistener, 0, 0, 1)
	ZEND_ARG_INFO(0, callbackId)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_queryselector, 0, 0, 1)
	ZEND_ARG_INFO(0, selector)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___call, 0, 0, 2)
	ZEND_ARG_INFO(0, method_name)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

/* {{{ PHP_RINIT_FUNCTION
 */
static const zend_function_entry vrzno_vrzno_methods[] = {
	PHP_ME(Vrzno, addeventlistener,    arginfo_addeventlistener, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, removeeventlistener, arginfo_removeeventlistener, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __call, arginfo___call, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

PHP_RINIT_FUNCTION(vrzno)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Vrzno", vrzno_vrzno_methods);
	vrzno_class_entry = zend_register_internal_class(&ce);
	vrzno_class_entry->create_object = vrzno_create_object;

	memcpy(&vrzno_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

 	vrzno_object_handlers.read_property = vrzno_read_property;

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
	PHP_FE(vrzno_eval,    arginfo_vrzno_eval)
	PHP_FE(vrzno_run,     arginfo_vrzno_run)
	PHP_FE(vrzno_timeout, arginfo_vrzno_timeout)
	PHP_FE_END
};

/* }}} */

/* {{{ vrzno_module_entry
 */
zend_module_entry vrzno_module_entry = {
	STANDARD_MODULE_HEADER,
	"vrzno",				/* Extension name */
	vrzno_functions,		/* zend_function_entry */
	NULL,					/* PHP_MINIT - Module initialization */
	NULL,					/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(vrzno),		/* PHP_RINIT - Request initialization */
	NULL,					/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(vrzno),		/* PHP_MINFO - Module info */
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

	zval params;
	zval retval;

	fci.size             = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);
	fci.object           = NULL;
	fci.retval           = &retval;
	fci.params           = &params;
	fci.param_count      = 0;
	// fci.no_separation    = 1;

	fcc.function_handler = fptr;
	fcc.called_scope     = NULL;
	fcc.object           = NULL;

	if(zend_call_function(&fci, &fcc) == SUCCESS && Z_TYPE(retval) != IS_UNDEF)
	{

		// if (Z_ISREF(retval)) {
		// 	zend_unwrap_reference(&retval);
		// }
		// ZVAL_COPY_VALUE(return_value, &retval);
		return 0;
	}

	return 1;
}

int vrzno_del_callback(zend_function *fptr)
{
	GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));
	return 0;
}

PHP_METHOD(Vrzno, addeventlistener)
{
	zval         *object         = getThis();
	vrzno_object *vrzno          = object->value.obj;
	long          targetId       = vrzno->targetId;
	char         *event_name     = "";
	size_t        event_name_len = sizeof(event_name) - 1;
	zval         *options;

	zend_fcall_info       fci;
	zend_fcall_info_cache fcc;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_STRING(event_name, event_name_len)
		Z_PARAM_FUNC(fci, fcc)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(options)
	ZEND_PARSE_PARAMETERS_END();

	GC_ADDREF(ZEND_CLOSURE_OBJECT(fcc.function_handler));

	int callbackId = EM_ASM_INT({

		const target    = Module.targets.get($0) || globalThis;
		const eventName = UTF8ToString($1);
		const funcPtr   = $2;
		const options   = {};

		const callback  = () => {
			Module.ccall(
				'exec_callback'
				, 'number'
				, ["number"]
				, [funcPtr]
			);
		};

		target.addEventListener(eventName, callback, options);

		const remover = () => {
			target.removeEventListener(eventName, callback, options);
			return $2;
		};

		return Module.callbacks.add(remover);

	}, targetId, event_name, fcc.function_handler);

	RETURN_LONG(callbackId);
}

PHP_METHOD(Vrzno, removeeventlistener)
{
	int callbackId;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(callbackId)
	ZEND_PARSE_PARAMETERS_END();

	zend_function *fptr = EM_ASM_INT({

		const remover = Module.callbacks.get($0);

		return remover();

	}, callbackId);

	if(fptr)
	{
		GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));
	}
}

PHP_METHOD(Vrzno, queryselector)
{
	zval         *object         = getThis();
	vrzno_object *vrzno          = object->value.obj;
	long          targetId       = vrzno->targetId;
	char         *query_selector = "";
	size_t        query_selector_len = sizeof(query_selector) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(query_selector, query_selector_len)
	ZEND_PARSE_PARAMETERS_END();

	int obj_result = EM_ASM_INT({

		const target        = Module.targets.get($0) || globalThis;
		const querySelector = UTF8ToString($1);

		const result        = target.querySelector(querySelector);

		if(!result)
		{
			return 0;
		}

		let index = Module.targets.getId(result);

		if(!Module.targets.has(result))
		{
			index = Module.targets.add(result);
		}

		return index;

	}, targetId, query_selector);

	if(!obj_result)
	{
		RETURN_BOOL(0);
		return;
	}

	vrzno_object *retObj = zend_object_alloc(sizeof(vrzno_object), vrzno_class_entry);
    retObj->targetId = obj_result;

    zend_object_std_init(retObj, vrzno_class_entry);

    retObj->zo.handlers = &vrzno_object_handlers;

    RETURN_OBJ(retObj);
}

PHP_METHOD(Vrzno, __call)
{
	zend_string      *retval;
	php_json_encoder  encoder;
	zend_long         opt = 0;

	zval         *object   = getThis();
	vrzno_object *vrzno    = object->value.obj;
	long          targetId = vrzno->targetId;

	char   *js_method_name     = "";
	size_t  js_method_name_len = sizeof(js_method_name) - 1;
	zval   *js_argv;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(js_method_name, js_method_name_len)
		Z_PARAM_ARRAY(js_argv)
	ZEND_PARSE_PARAMETERS_END();

	smart_str buf = {0};

	php_json_encode_init(&encoder);
	encoder.max_depth = PHP_JSON_PARSER_DEFAULT_DEPTH;
	php_json_encode_zval(&buf, js_argv, opt, &encoder);

	char *js_args = ZSTR_VAL(buf.s);

	smart_str_0(&buf);

	char *js_ret = EM_ASM_INT({

		const target      = Module.targets.get($0) || globalThis;
		const method_name = UTF8ToString($1);
		const argJson     = UTF8ToString($2);

		const args     = JSON.parse(argJson || '[]') || [];

		const jsRet    = String(target[method_name](...args));
		const len      = lengthBytesUTF8(jsRet) + 1;
		const strLoc   = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId, js_method_name, js_args);

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}
