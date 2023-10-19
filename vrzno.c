/* vrzno extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "php_vrzno.h"
#include "../json/php_json.h"
#include "../json/php_json_encoder.h"
#include "../json/php_json_parser.h"
#include "zend_API.h"
#include "zend_types.h"
#include "zend_closures.h"
#include <emscripten.h>
#include "zend_attributes.h"

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

typedef struct {
	long targetId;
	zend_object zo;
} vrzno_object;

static inline vrzno_object *vrzno_fetch_object(zend_object *obj) {
	return (vrzno_object*)((char*)(obj) - XtOffsetOf(vrzno_object, zo));
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
		const timeout = Number(UTF8ToString($0));
		const funcPtr = $1;

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

#include "vrzno_expose.c"

static struct _zend_object *vrzno_create_object(zend_class_entry *class_type)
{
    zend_object  *retZObj  = zend_object_alloc(sizeof(vrzno_object), class_type);
	vrzno_object *retVrzno = vrzno_fetch_object(retZObj);

    zend_object_std_init(&retVrzno->zo, class_type);

    retVrzno->zo.handlers = &vrzno_object_handlers;
    retVrzno->targetId = (long) EM_ASM_INT({ return Module.targets.add(globalThis); });

    return &retVrzno->zo;
}

static void vrzno_object_free(zend_object *zobj)
{
	vrzno_object *vrzno = vrzno_fetch_object(zobj);

	vrzno->targetId = NULL;

	zend_object_std_dtor(zobj);
}

zval *vrzno_read_property(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv)
{
	char         *name     = ZSTR_VAL(member);
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	long          targetId = vrzno->targetId;

	EM_ASM({ console.log('READ1', $0, $1, $2); }, targetId, object, vrzno);

	char *scalar_result = EM_ASM_INT({

		const target = Module.targets.get($0) || globalThis;

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

	free(scalar_result);

	int obj_result = EM_ASM_INT({

		const target   = Module.targets.get($0) || globalThis;
		const property = UTF8ToString($1);
		const result   = target[property];

		console.log('READING', {aa: $0, target, property, result});

		if(['function','object'].includes(typeof result))
		{
			let index = Module.targets.getId(result);

			if(!Module.targets.has(result))
			{
				index = Module.targets.add(result);
			}

			console.log({index, result});

			return index;
		}

		return 0;

	}, targetId, name);

	if(!obj_result)
	{
		ZVAL_BOOL(rv, obj_result);

		return rv;
	}

	zend_object  *retZObj  = zend_object_alloc(sizeof(vrzno_object), vrzno_class_entry);
	vrzno_object *retVrzno = vrzno_fetch_object(retZObj);

    zend_object_std_init(&retVrzno->zo, vrzno_class_entry);

    retVrzno->zo.handlers = &vrzno_object_handlers;
    retVrzno->targetId = (long) obj_result;

	EM_ASM({ console.log('READ2', $0, $1, $2) }, obj_result, retZObj, retVrzno);

	ZVAL_OBJ(rv, &retVrzno->zo);
	return rv;
}

zval *vrzno_write_property(zend_object *object, zend_string *member, zval *newValue, void **cache_slot)
{
	char         *name     = ZSTR_VAL(member);
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	long          targetId = vrzno->targetId;

	EM_ASM({ (() =>{
		const target = Module.targets.get($0) || globalThis;
		const property = UTF8ToString($1);
		const newValue = $2;
		console.log('WRITING', {aa: $0, target, property, newValue});
	})() }, targetId, name, newValue);

	zend_fcall_info_cache fcc;
	char *errstr = NULL;

	if(zend_is_callable_ex(newValue, NULL, 0, NULL, &fcc, &errstr))
	{
		Z_ADDREF_P(newValue);
		EM_ASM({ console.log('INC_ZVAL_1a', $0, $1); }, newValue, Z_REFCOUNTED_P(newValue));

		EM_ASM({ (() =>{
			const target   = Module.targets.get($0) || globalThis;
			const property = UTF8ToString($1);
			const funcPtr  = $2;
			const zvalPtr  = $3;
			console.log('WRITING FUNCTION', {aa: $0, target, property, funcPtr});
			target[property] = (...args) => {
				console.log('CALLING FUNCTION', funcPtr, {targets: Module.targets});
				Module.ccall(
					'exec_callback'
					, 'number'
					, ["number"]
					, [funcPtr]
				);
			};
			Module.fRegistry.register(target[property], zvalPtr);
		})() }, targetId, name, fcc.function_handler, newValue);

		return newValue;
	}

	switch(Z_TYPE_P(newValue))
	{
		case IS_OBJECT:

			Z_ADDREF_P(newValue);
			EM_ASM({ console.log('INC_ZVAL_1b', $0, $1); }, newValue, Z_REFCOUNTED_P(newValue));

			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				const zvalPtr = $2;

				console.log('Inc refcount for zVal@' + zvalPtr);

				const marshalObject = zvalPtr => {

					return new Proxy({}, {
						get: (target, prop, receiver) => {
							console.log(target);

							if(target[prop])
							{
								return target[prop];
							}

							const len     = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);

							stringToUTF8(prop, namePtr, len);

							const IS_UNDEF  = 0;
							const IS_NULL   = 1;
							const IS_FALSE  = 2;
							const IS_TRUE   = 3;
							const IS_LONG   = 4;
							const IS_DOUBLE = 5;
							const IS_STRING = 6;
							const IS_ARRAY  = 7;
							const IS_OBJECT = 8;

							const callable = Module.ccall(
								'vrzno_expose_property_callable'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, namePtr]
							);

							const type = Module.ccall(
								'vrzno_expose_property_type'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, namePtr]
							);

							let valPtr, value;

							console.log({callable, type, zvalPtr, namePtr});

							if(callable)
							{
								target[prop] = (...args) => {
									console.log('CALLING FUNCTION', callable, {targets: Module.targets});
									Module.ccall(
										'exec_callback'
										, 'number'
										, ["number"]
										, [callable]
									);
								};

								return target[prop];
							}

							switch(type)
							{
								case IS_UNDEF:
									_free(namePtr);
									return undefined;
									break;

								case IS_NULL:
									_free(namePtr);
									return null;
									break;

								case IS_TRUE:
									_free(namePtr);
									return true;
									break;

								case IS_FALSE:
									_free(namePtr);
									return false;
									break;

								case IS_LONG:
									const longVal = Module.ccall(
										'vrzno_expose_property_long'
										, 'number'
										, ['number', 'number']
										, [zvalPtr, namePtr]
									);

									_free(namePtr);

									return longVal;
									break;

								case IS_DOUBLE:
									valPtr = Module.ccall(
										'vrzno_expose_property_double'
										, 'number'
										, ['number', 'number']
										, [zvalPtr, namePtr]
									);

									_free(namePtr);

									if(!valPtr)
									{
										return null;
									}

									return getValue(valPtr, 'double');
									break;

								case IS_STRING:
									valPtr = Module.ccall(
										'vrzno_expose_property_string'
										, 'number'
										, ['number', 'number']
										, [zvalPtr, namePtr]
									);

									_free(namePtr);

									if(!valPtr)
									{
										return null;
									}

									return UTF8ToString(valPtr);
									break;

								// case IS_ARRAY:
								// 	break;

								case IS_OBJECT:

									retZValPtr = Module.ccall(
										'vrzno_expose_property_pointer'
										, 'number'
										, ['number', 'number']
										, [zvalPtr, namePtr]
									);

									_free(namePtr);

									if(!retZValPtr)
									{
										return null;
									}

									target[prop] = marshalObject(retZValPtr);

									return target[prop];
									break;

								default:
									_free(namePtr);
									return null;
									break;
							}
						}
					});
				};

				target[property] = marshalObject(zvalPtr);

				Module.fRegistry.register(target[property], zvalPtr);

			})() }, targetId, name, newValue);
			break;

		case IS_UNDEF:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				delete target[property];
			})() }, targetId, name);
			break;

		case IS_NULL:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				target[property] = null;
			})() }, targetId, name);
			break;

		case IS_FALSE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				target[property] = false;
			})() }, targetId, name);
			break;

		case IS_TRUE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				target[property] = true;
			})() }, targetId, name);
			break;

		case IS_DOUBLE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				target[property] = $2;
			})() }, targetId, name, Z_DVAL_P(newValue));
			break;

		case IS_LONG:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				target[property] = $2;
			})() }, targetId, name, Z_LVAL_P(newValue));
			break;

		case IS_STRING:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0) || globalThis;
				const property = UTF8ToString($1);
				const newValue = UTF8ToString($2);
				target[property] = newValue;
			})() }, targetId, name, Z_STRVAL_P(newValue));
			break;
	}

	return newValue;
}

static zval * vrzno_read_dimension(zend_object *object, zval *offset, int type, zval *rv)
{
	convert_to_long(offset);
	long offsetL = (long) Z_LVAL_P(offset);

	// char         *name     = ZSTR_VAL(offset);
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	long          targetId = vrzno->targetId;

	EM_ASM({ console.log('READD1', $0) }, targetId);

	char *scalar_result = EM_ASM_INT({

		const target = Module.targets.get($0) || globalThis;

		const property = $1;
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

	}, targetId, offsetL);

	if(strlen(scalar_result) > 1 && *scalar_result == 'O')
	{
		ZVAL_STRING(rv, scalar_result + 2);
		free(scalar_result);

		return rv;
	}

	free(scalar_result);

	int obj_result = EM_ASM_INT({

		const target   = Module.targets.get($0) || globalThis;
		const property = UTF8ToString($1);
		const result   = target[property];

		console.log('READING', {aa: $0, target, property, result});

		if(['function','object'].includes(typeof result))
		{
			let index = Module.targets.getId(result);

			if(!Module.targets.has(result))
			{
				index = Module.targets.add(result);
			}

			console.log({index, result});

			return index;
		}

		return 0;

	}, targetId, offsetL);

	if(!obj_result)
	{
		ZVAL_BOOL(rv, obj_result);

		return rv;
	}

	zend_object  *retZObj  = zend_object_alloc(sizeof(vrzno_object), vrzno_class_entry);
	vrzno_object *retVrzno = vrzno_fetch_object(retZObj);

    zend_object_std_init(&retVrzno->zo, vrzno_class_entry);

    retVrzno->zo.handlers = &vrzno_object_handlers;
    retVrzno->targetId = (long) obj_result;

	EM_ASM({ console.log('READD2', $0, $1, $2) }, obj_result, (long) obj_result, retZObj);

	ZVAL_OBJ(rv, &retVrzno->zo);
	return rv;
}

// static void vrzno_write_dimension(zend_object *object, zval *offset, zval *value)
// {}

void vrzno_unset_property(zend_object *object, zend_string *member, void **cache_slot)
{
	char         *name     = ZSTR_VAL(member);
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	long          targetId = vrzno->targetId;

	// bool isScalar = false;

	EM_ASM({ (() =>{
		const target = Module.targets.get($0) || globalThis;
		const property = UTF8ToString($1);
		console.log('DELETING', {target, property});
		delete target[property];
	})() }, targetId, name);
}

static HashTable *vrzno_get_properties_for(zend_object *object, zend_prop_purpose purpose)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;

	EM_ASM({ console.log('SCAN1', $0, $1, $2); }, targetId, object, vrzno);

	char *js_ret = EM_ASM_INT({

		const target = Module.targets.get($0) || globalThis;

		if(typeof target === 'function')
		{
			json = JSON.stringify({__targetId:$0});
		}
		else
		{
			try{ json = JSON.stringify({__targetId:$0, ...target}); }
			catch { json = JSON.stringify({__targetId:$0}); }
		}

		console.log('SCANNING', {aa: $0, target});

		const jsRet  = String(json);
		const len    = lengthBytesUTF8(jsRet) + 1;
		const strLoc = _malloc(len);

		console.log(jsRet);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId);

	php_json_parser parser;
	zval            js_object;

	php_json_parser_init(&parser, &js_object, js_ret, strlen(js_ret), PHP_JSON_OBJECT_AS_ARRAY, PHP_JSON_PARSER_DEFAULT_DEPTH);

	if(php_json_yyparse(&parser))
	{
		// FAIL
	}

	free(js_ret);

	return Z_ARR(js_object);
}

int vrzno_has_property(zend_object *object, zend_string *member, int has_set_exists, void **cache_slot)
{
	char         *name     = ZSTR_VAL(member);
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	long          targetId = vrzno->targetId;

	return EM_ASM_INT({
		const target   = Module.targets.get($0) || globalThis;
		const property = UTF8ToString($1);
		console.log('CHECKING', {target, property, result});
		return  property in target;
	}, targetId, name);
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

// ZEND_BEGIN_ARG_INFO_EX(arginfo___set, 0, 0, 2)
// 	ZEND_ARG_INFO(0, property_name)
// 	ZEND_ARG_INFO(0, args)
// ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___get, 0, 0, 1)
	ZEND_ARG_INFO(0, property_name)
ZEND_END_ARG_INFO()

/* {{{ PHP_RINIT_FUNCTION
 */
static const zend_function_entry vrzno_vrzno_methods[] = {
	PHP_ME(Vrzno, addeventlistener,    arginfo_addeventlistener, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, removeeventlistener, arginfo_removeeventlistener, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, queryselector, arginfo_queryselector, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __call, arginfo___call, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __get, arginfo___get, ZEND_ACC_PUBLIC)
	// PHP_ME(Vrzno, __set, arginfo___set, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

PHP_RINIT_FUNCTION(vrzno)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Vrzno", vrzno_vrzno_methods);
	vrzno_class_entry = zend_register_internal_class(&ce);
	vrzno_class_entry->create_object = vrzno_create_object;

	vrzno_class_entry->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;

	zend_string *attribute_name_AllowDynamicProperties_class_vrzno = zend_string_init_interned("AllowDynamicProperties", sizeof("AllowDynamicProperties") - 1, 1);
	zend_add_class_attribute(vrzno_class_entry, attribute_name_AllowDynamicProperties_class_vrzno, 0);
	zend_string_release(attribute_name_AllowDynamicProperties_class_vrzno);

	memcpy(&vrzno_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	vrzno_object_handlers.get_properties_for = vrzno_get_properties_for;
	vrzno_object_handlers.unset_property     = vrzno_unset_property;
 	vrzno_object_handlers.write_property     = vrzno_write_property;
 	vrzno_object_handlers.read_property      = vrzno_read_property;
	vrzno_object_handlers.free_obj           = vrzno_object_free;
	vrzno_object_handlers.has_property       = vrzno_has_property;
	vrzno_object_handlers.read_dimension     = vrzno_read_dimension;

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

	EM_ASM({ console.log('exec', $0, $1, $2) }, &fci, &fcc, fptr);

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
	zend_object  *zObject        = object->value.obj;
	vrzno_object *vrzno =         vrzno_fetch_object(zObject);
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
	zend_object  *zObject        = object->value.obj;
	vrzno_object *vrzno =         vrzno_fetch_object(zObject);
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

		console.log({index, result});

		return index;

	}, targetId, query_selector);

	if(!obj_result)
	{
		RETURN_BOOL(0);
		return;
	}

	zend_object  *retZObj  = zend_object_alloc(sizeof(vrzno_object), vrzno_class_entry);
	vrzno_object *retVrzno = vrzno_fetch_object(retZObj);

    zend_object_std_init(&retVrzno->zo, vrzno_class_entry);

    retVrzno->zo.handlers = &vrzno_object_handlers;
    retVrzno->targetId = (long) obj_result;

    RETURN_OBJ(retZObj);
}

PHP_METHOD(Vrzno, __call)
{
	zval         *object   = getThis();
	zend_object  *zObject  = object->value.obj;
	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
	long          targetId = vrzno->targetId;

	EM_ASM({ console.log('CALL1', $0) }, targetId);

	char   *js_method_name     = "";
	size_t  js_method_name_len = sizeof(js_method_name) - 1;
	zval   *js_argv;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(js_method_name, js_method_name_len)
		Z_PARAM_ARRAY(js_argv)
	ZEND_PARSE_PARAMETERS_END();

	php_json_encoder  encoder;
	smart_str buf = {0};

	php_json_encode_init(&encoder);
	encoder.max_depth = PHP_JSON_PARSER_DEFAULT_DEPTH;
	php_json_encode_zval(&buf, js_argv, 0, &encoder);

	char *js_args = ZSTR_VAL(buf.s);

	smart_str_0(&buf);

	char *js_ret = EM_ASM_INT({

		const target      = Module.targets.get($0) || globalThis;
		const method_name = UTF8ToString($1);
		const argJson     = UTF8ToString($2);

		const args     = JSON.parse(argJson || '[]') || [];

		console.log('CALLING', {aa: $0, target, method_name, args, targets: Module.targets});

		const jsRet    = String(target[method_name](...args));
		const len      = lengthBytesUTF8(jsRet) + 1;
		const strLoc   = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId, js_method_name, js_args);

	zend_string *retval;

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}

PHP_METHOD(Vrzno, __get)
{
	zend_string      *retval;

	zval         *object   = getThis();
	zend_object  *zObject  = object->value.obj;
	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
	long          targetId = vrzno->targetId;

	char   *js_property_name     = "";
	size_t  js_property_name_len = sizeof(js_property_name) - 1;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_property_name, js_property_name_len)
	ZEND_PARSE_PARAMETERS_END();

	char *js_ret = EM_ASM_INT({

		const target        = Module.targets.get($0) || globalThis;
		const property_name = UTF8ToString($1);

		console.log('GET1', $0, {target, property_name});

		target[property_name] = newValueJson;

		const jsRet    = String(target[property_name]);
		const len      = lengthBytesUTF8(jsRet) + 1;
		const strLoc   = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId, js_property_name);

	retval = strpprintf(0, "%s", js_ret);

	free(js_ret);

	RETURN_STR(retval);
}
