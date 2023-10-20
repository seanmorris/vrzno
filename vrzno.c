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
#include "zend_hash.h"

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

#include "vrzno_object.c"
#include "vrzno_expose.c"
#include "vrzno_functions.c"

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
#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}

PHP_MINIT_FUNCTION(vrzno)
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
	vrzno_object_handlers.write_dimension    = vrzno_write_dimension;
	vrzno_object_handlers.has_dimension      = vrzno_has_dimension;
	vrzno_object_handlers.unset_dimension    = vrzno_unset_dimension;
	vrzno_object_handlers.get_class_name     = vrzno_get_class_name;

	EM_ASM({
		console.log('Startup!');
		Module.marshalObject = ((zvalPtr) => new Proxy({}, {
			get: (target, prop, receiver) => {
				console.log(target);

				if(target[prop])
				{
					return target[prop];
				}

				const len     = lengthBytesUTF8(prop) + 1;
				const namePtr = _malloc(len);

				stringToUTF8(prop, namePtr, len);

				proxy = Module.zvalToJS(Module.ccall(
					'vrzno_expose_property_pointer'
					, 'number'
					, ['number', 'number']
					, [zvalPtr, namePtr]
				));

				if(typeof proxy === 'object')
				{
					Module.fRegistry.register(proxy, zvalPtr);
				}

				return proxy;
			}
		}));

		Module.zvalToJS = Module.zvalToJS || (zvalPtr => {
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
				'vrzno_expose_callable'
				, 'number'
				, ['number', 'number']
				, [zvalPtr]
			);

			let valPtr;

			if(callable)
			{
				const wrapped = (...args) => {
					console.log('CALLING FUNCTION', callable, {targets: Module.targets});

					const ptrs = args.map(a => Module.jsToZval(a));

					console.log(ptrs);

					const zvalPtr = Module.ccall(
						'vrzno_exec_callback'
						, 'number'
						, ['number','number','number']
						, [callable, null, 0]
					);

					const marshalled = Module.zvalToJS(zvalPtr);

					console.log({zvalPtr, marshalled});

					return marshalled;
				};

				return wrapped;
			}

			const type = Module.ccall(
				'vrzno_expose_type'
				, 'number'
				, ['number']
				, [zvalPtr]
			);

			switch(type)
			{
				case IS_UNDEF:
					return undefined;
					break;

				case IS_NULL:
					return null;
					break;

				case IS_TRUE:
					return true;
					break;

				case IS_FALSE:
					return false;
					break;

				case IS_LONG:
					value = Module.ccall(
						'vrzno_expose_long'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					return value;
					break;

				case IS_DOUBLE:
					valPtr = Module.ccall(
						'vrzno_expose_double'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					if(!valPtr)
					{
						return null;
					}

					return getValue(valPtr, 'double');
					break;

				case IS_STRING:
					valPtr = Module.ccall(
						'vrzno_expose_string'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					if(!valPtr)
					{
						return null;
					}

					return UTF8ToString(valPtr);
					break;

				// case IS_ARRAY:
				// 	break;

				case IS_OBJECT:
					const proxy = Module.marshalObject(zvalPtr);

					Module.ccall(
						'vrzno_expose_inc_refcount'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					Module.fRegistry.register(proxy, zvalPtr);

					return proxy;
					break;

				default:
					return null;
					break;
			}
		});

		Module.jsToZval = Module.jsToZval || (value => {

			console.log('CREATING ZVAL FOR', value);

			let zvalPtr;

			if([true, false].includes(value))
			{
				zvalPtr = Module.ccall(
					'vrzno_expose_create_bool'
					, 'number'
					, ["number"]
					, [value]
				);

				console.log('Bool:', {zvalPtr});
			}
			else if(['function','object'].includes(typeof value))
			{
				let index;

				if(!Module.targets.has(value))
				{
					index = Module.targets.add(value);
				}
				else
				{
					index = Module.targets.getId(value);
				}

				console.log({index});

				zvalPtr = Module.ccall(
					'vrzno_expose_create_object_for_target'
					, 'number'
					, ["number"]
					, [index]
				);

				Module.fRegistry.register(value, zvalPtr);
			}
			else if(typeof value === "number")
			{
				if(Number.isInteger(value))
				{
					// Generate long
					zvalPtr = Module.ccall(
						'vrzno_expose_create_long'
						, 'number'
						, ["number"]
						, [value]
					);

					console.log('Long:', {zvalPtr});
				}
				else if(Number.isFinite(value))
				{
					// Generate double
					zvalPtr = Module.ccall(
						'vrzno_expose_create_double'
						, 'number'
						, ["number"]
						, [value]
					);

					console.log('Double:', {zvalPtr});
				}
			}
			else if(typeof value === "string")
			{
				// Generate string zval
				const len      = lengthBytesUTF8(value) + 1;
				const strLoc   = _malloc(len);

				stringToUTF8(value, strLoc, len);

				zvalPtr = Module.ccall(
					'vrzno_expose_create_string'
					, 'number'
					, ["number"]
					, [strLoc]
				);

				console.log('String:', {zvalPtr});
			}

			return zvalPtr;
		});

		Module.UniqueIndex = Module.UniqueIndex || (class UniqueIndex
		{
			constructor()
			{
				this.byInteger = new Map();
				this.byObject  = new Map();

				let id = 0;

				Object.defineProperty(this, 'add', {
					configurable: false
					, writable:   false
					, value: (callback) => {

						if(this.byObject.has(callback))
						{
							const id = this.byObject.get(callback);

							return id;
						}

						const newid = ++id;

						this.byObject.set(callback, newid);
						this.byInteger.set(newid, callback);

						return newid;
					}
				});

				Object.defineProperty(this, 'has', {
					configurable: false
					, writable:   false
					, value: (callback) => {
						if(this.byObject.has(callback))
						{
							return this.byObject.get(callback);
						}
					}
				});

				Object.defineProperty(this, 'get', {
					configurable: false
					, writable:   false
					, value: (id) => {
						if(this.byInteger.has(id))
						{
							return this.byInteger.get(id);
						}
					}
				});

				Object.defineProperty(this, 'getId', {
					configurable: false
					, writable:   false
					, value: (callback) => {
						if(this.byObject.has(callback))
						{
							return this.byObject.get(callback);
						}
					}
				});

				Object.defineProperty(this, 'remove', {
					configurable: false
					, writable:   false
					, value: (id) => {

						const callback = this.byInteger.get(id);

						if(callback)
						{
							this.byObject.delete(callback);
							this.byInteger.delete(id);
						}
					}
				});
			}
		});

		Module.targets = new Module.UniqueIndex;
	});

	return SUCCESS;
}

PHP_MINFO_FUNCTION(vrzno)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "vrzno support", "enabled");
	php_info_print_table_end();
}

zend_module_entry vrzno_module_entry = {
	STANDARD_MODULE_HEADER,
	"vrzno",
	vrzno_functions,		   /* zend_function_entry */
	PHP_MINIT(vrzno),		   /* PHP_MINIT - Module initialization */
	NULL,					   /* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(vrzno),		   /* PHP_RINIT - Request initialization */
	NULL,                      /* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(vrzno),		   /* PHP_MINFO - Module info */
	PHP_VRZNO_VERSION,		   /* Version */
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_VRZNO
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(vrzno)
#endif

int EMSCRIPTEN_KEEPALIVE vrzno_exec_callback(zend_function *fptr, zval **argv, int argc)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	zval params[argc];
	zval retval;

	fci.size             = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);
	fci.object           = NULL;
	fci.retval           = &retval;
	fci.params           = params;
	fci.param_count      = argc;

	fcc.function_handler = fptr;
	fcc.called_scope     = NULL;
	fcc.object           = NULL;

	if(argv)
	{
		int i;
		for(i = 0; i < argc; i++)
		{
			params[i] = *argv[i];
		}
	}

	EM_ASM({ console.log('exec', $0, $1, $2, $3, $4) }, &fci, &fcc, fptr, argv, argc);

	if(zend_call_function(&fci, &fcc) == SUCCESS)
	{
		return fci.retval;
	}

	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_del_callback(zend_function *fptr)
{
	GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));
	return 0;
}
