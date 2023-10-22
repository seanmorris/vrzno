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

PHP_RINIT_FUNCTION(vrzno)
{
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
	// vrzno_object_handlers.get_class_name     = vrzno_get_class_name;

	EM_ASM({
		console.log('Startup!');

		Module.hasZval = Symbol('HAS_ZVAL');
		Module.isTarget = Symbol('IS_TARGET');

		Module.fRegistry = Module.fRegistry || new FinalizationRegistry(zvalPtr => {
			console.log('Garbage collecting zVal@'+zvalPtr);
			Module.ccall(
				'vrzno_expose_dec_refcount'
				, 'number'
				, ['number']
				, [zvalPtr]
			);
		});

		Module.marshalObject = ((zvalPtr) => {
			const nativeTarget = Module.ccall(
				'vrzno_expose_zval_is_target'
				, 'number'
				, ['number']
				, [zvalPtr]
			);

			// console.log({nativeTarget, target: Module.targets.get(nativeTarget)});

			if(nativeTarget && Module.targets.hasId(nativeTarget))
			{
				return Module.targets.get(nativeTarget);
			}

			const proxy = new Proxy({}, {
				ownKeys: (target) => {

					// console.log('KEYS', target);

					const keysLoc = Module.zvalToJS(Module.ccall(
						'vrzno_expose_object_keys'
						, 'number'
						, ['number']
						, [zvalPtr]
					));

					const keys = [];

					if(keysLoc)
					{
						const json = UTF8ToString(keysLoc);

						// console.log(json);

						keys.push(...JSON.parse(json));
					}

					keys.push(...Reflect.ownKeys(target));

					return keys;
				},
				get: (target, prop, receiver) => {

					if(typeof prop !== 'strinf')
					{
						return target[prop];
					}

					// console.log(target);

					if(target[prop])
					{
						return target[prop];
					}

					const len     = lengthBytesUTF8(prop) + 1;
					const namePtr = _malloc(len);

					stringToUTF8(prop, namePtr, len);

					const retPtr = Module.ccall(
						'vrzno_expose_property_pointer'
						, 'number'
						, ['number', 'number']
						, [zvalPtr, namePtr]
					);

					proxy = Module.zvalToJS(retPtr);

					if(!Module.targets.has(proxy))
					{
						// Module.ccall(
						// 	'vrzno_expose_inc_refcount'
						// 	, 'number'
						// 	, ['number']
						// 	, [retPtr]
						// );

						proxy[Module.hasZval] = retPtr;

						Module.fRegistry.register(proxy, retPtr, proxy);
					}

					_free(namePtr);

					return proxy;
				}
			});

			if(!Module.targets.has(proxy))
			{
				// Module.ccall(
				// 	'vrzno_expose_inc_refcount'
				// 	, 'number'
				// 	, ['number']
				// 	, [zvalPtr]
				// );

				proxy[Module.hasZval] = zvalPtr;

				Module.fRegistry.register(proxy, zvalPtr, proxy);
			}
		});

		Module.callableToJs = Module.callableToJs || ((funcPtr) => {
			console.log('WRAPPING FUNCTION', funcPtr);
			return (...args) => {
				console.log('CALLING FUNCTION', funcPtr);

				let paramPtrs = [], paramsPtr = null;

				if(args.length)
				{
					paramsPtr = Module.ccall(
						'vrzno_expose_create_params'
						, 'number'
						, ["number","number"]
						, [args.length]
					);

					paramPtrs = args.map(a => Module.jsToZval(a));

					paramPtrs.forEach((paramPtr,i) => {
						Module.ccall(
							'vrzno_expose_set_param'
							, 'number'
							, ['number','number','number']
							, [paramsPtr, i, paramPtr]
						);
					});
				}

				const zvalPtr = Module.ccall(
					'vrzno_exec_callback'
					, 'number'
					, ['number','number','number']
					, [funcPtr, paramsPtr, args.length]
				);

				if(args.length)
				{
					paramPtrs.forEach((paramPtr,i) => {
						console.log({paramPtr, i});
						Module.ccall(
							'vrzno_expose_dec_refcount'
							, 'number'
							, ['number']
							, [paramPtr]
						);
					});

					console.log({paramsPtr});
					Module.ccall(
						'vrzno_expose_efree'
						, 'number'
						, ['number']
						, [paramsPtr]
					);
				}

				if(zvalPtr)
				{
					const marshalled = Module.zvalToJS(zvalPtr);
					return marshalled;
				}
		}});

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
				, ['number']
				, [zvalPtr]
			);

			let valPtr;

			if(callable)
			{
				const wrapped = Module.callableToJs(callable);

				console.log({callable});

				wrapped[ Module.hasZval ] = zvalPtr;

				Module.fRegistry.register(wrapped, zvalPtr, wrapped);

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

					// Module.fRegistry.register(proxy, zvalPtr, proxy);

					return proxy;
					break;

				default:
					return null;
					break;
			}
		});

		Module.jsToZval = Module.jsToZval || (value => {

			if(value && ['function','object'].includes(typeof value))
			{
				if(value[ Module.hasZval ])
				{
					// console.log('Using existing ZVAL FOR', value);

					return value[ Module.hasZval ];
				}
			}

			// console.log('CREATING ZVAL FOR', value);

			let zvalPtr;

			if(typeof value === 'undefined')
			{
				zvalPtr = Module.ccall(
					'vrzno_expose_create_undef'
					, 'number'
					, []
					, []
				);

				// console.log('Undef:', {zvalPtr});
			}
			else if(value === null)
			{
				zvalPtr = Module.ccall(
					'vrzno_expose_create_null'
					, 'number'
					, []
					, []
				);

				// console.log('Bool:', {zvalPtr});
			}
			else if([true, false].includes(value))
			{
				zvalPtr = Module.ccall(
					'vrzno_expose_create_bool'
					, 'number'
					, ["number"]
					, [value]
				);

				// console.log('Null:', {zvalPtr});
			}
			else if(value && ['function','object'].includes(typeof value))
			{
				let index;

				if(!Module.targets.has(value))
				{
					index = Module.targets.add(value);

					// Module.ccall(
					// 	'vrzno_expose_inc_refcount'
					// 	, 'number'
					// 	, ['number']
					// 	, [zvalPtr]
					// );
				}
				else
				{
					index = Module.targets.getId(value);
				}

				// console.log({index, value});

				zvalPtr = Module.ccall(
					'vrzno_expose_create_object_for_target'
					, 'number'
					, ['number', 'number']
					, [index, typeof value === 'function']
				);

				value[ Module.hasZval ] = zvalPtr;

				Module.fRegistry.register(value, zvalPtr, value);
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

					// console.log('Long:', {zvalPtr});
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

					// console.log('Double:', {zvalPtr});
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

				_free(strLoc);

				// console.log('String:', {zvalPtr});
			}

			return zvalPtr;
		});

		Module.WeakerMap = Module.WeakerMap || (class WeakerMap
		{
			constructor(entries)
			{
				this.registry = new FinalizationRegistry(held => this.delete(held));
				this.map = new Map;
				entries && entries.forEach(([key, value]) => this.set(key, value));
			}

			get size()
			{
				return this.map.size;
			}

			clear()
			{
				this.map.clear();
			}

			delete(key)
			{
				this.map.delete(key);
			}

			[Symbol.iterator]()
			{
				const mapIterator = this.map[Symbol.iterator]();

				return {
					next: () => {
						do
						{
							const entry = mapIterator.next();

							if(entry.done)
							{
								return {done:true};
							}

							const [key, ref] = entry.value;

							const value = ref.deref();

							if(!value)
							{
								this.map.delete(key);
								continue;
							}

							return {done: false, value: [key, value]};

						} while(true);
					}
				};
			}

			entries()
			{
				return {[Symbol.iterator]: () => this[Symbol.iterator]()};
			}

			forEach(callback)
			{
				for(const [k,v] of this)
				{
					callback(v, k, this);
				}
			}

			get(key)
			{
				if(!this.has(key))
				{
					return;
				}

				return this.map.get(key).deref();
			}

			has(key)
			{
				if(!this.map.has(key))
				{
					return false;
				}

				const result = this.map.get(key).deref();

				if(!result)
				{
					this.map.delete(key);
				}

				return result;
			}

			keys()
			{
				return [...this].map(v => v[0]);
			}

			set(key, value)
			{
				if(typeof value !== 'function' && typeof value !== 'object')
				{
					throw new Error('WeakerMap values must be objects.');
				}

				if(this.map.has(key))
				{
					this.registry.unregister(this.get(key));
				}

				this.registry.register(value, key, value);

				return this.map.set(key, new WeakRef(value));
			}

			values()
			{
				return [...this].map(v => v[1]);
			}
		});

		Module.UniqueIndex = Module.UniqueIndex || (class UniqueIndex
		{
			constructor()
			{
				this.byObject  = new WeakMap();
				this.byInteger = new Module.WeakerMap();

				this.id = 0;

				Object.defineProperty(this, 'add', {
					configurable: false
					, writable:   false
					, value: (callback) => {

						if(this.byObject.has(callback))
						{
							const id = this.byObject.get(callback);

							return id;
						}

						const newid = ++this.id;

						this.byObject.set(callback, newid);
						this.byInteger.set(newid, callback);

						return newid;
					}
				});

				Object.defineProperty(this, 'has', {
					configurable: false
					, writable:   false
					, value: (obj) => {
						if(this.byObject.has(obj))
						{
							return this.byObject.get(obj);
						}
					}
				});

				Object.defineProperty(this, 'hasId', {
					configurable: false
					, writable:   false
					, value: (address) => {
						if(this.byInteger.has(address))
						{
							return this.byInteger.get(address);
						}
					}
				});

				Object.defineProperty(this, 'get', {
					configurable: false
					, writable:   false
					, value: (address) => {
						if(this.byInteger.has(address))
						{
							return this.byInteger.get(address);
						}
					}
				});

				Object.defineProperty(this, 'getId', {
					configurable: false
					, writable:   false
					, value: (obj) => {
						if(this.byObject.has(obj))
						{
							return this.byObject.get(obj);
						}
					}
				});

				Object.defineProperty(this, 'remove', {
					configurable: false
					, writable:   false
					, value: (address) => {

						const obj = this.byInteger.get(address);

						if(obj)
						{
							this.byObject.delete(obj);
							this.byInteger.delete(address);
						}
					}
				});
			}
		});

		Module.targets = Module.targets || new Module.UniqueIndex;
	});

#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

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
	vrzno_functions,           /* zend_function_entry */
	PHP_MINIT(vrzno),          /* PHP_MINIT - Module initialization */
	NULL,                      /* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(vrzno),          /* PHP_RINIT - Request initialization */
	NULL,                      /* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(vrzno),          /* PHP_MINFO - Module info */
	PHP_VRZNO_VERSION,         /* Version */
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
	fci.named_params     = 0;
	fci.param_count      = argc;

	fcc.function_handler = fptr;
	fcc.calling_scope    = NULL;
	fcc.called_scope     = NULL;
	fcc.object           = NULL;

	if(argv)
	{
		int i;
		for(i = 0; i < argc; i++)
		{
			ZVAL_UNDEF(&params[i]);
			ZVAL_COPY(&params[i], argv[i]);
		}
	}

	// EM_ASM({ console.log('exec', $0, $1, $2) }, fptr, argv, argc);


	if(zend_call_function(&fci, &fcc) == SUCCESS)
	{
		zval *retZval = (zval*) emalloc(sizeof(zval));

		ZVAL_UNDEF(retZval);
		ZVAL_COPY(retZval, &retval);

		return retZval;
	}

	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_del_callback(zend_function *fptr)
{
	GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));
	return 0;
}
