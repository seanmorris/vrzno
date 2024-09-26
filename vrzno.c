/* vrzno extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "../pdo/php_pdo_driver.h"
#include "../json/php_json.h"
#include "../json/php_json_encoder.h"
#include "../json/php_json_parser.h"
#include "zend_API.h"
#include "zend_types.h"
#include "zend_closures.h"
#include <emscripten.h>
#include "zend_hash.h"

#if PHP_MAJOR_VERSION >= 8
# include "zend_attributes.h"
#else
# include <stdbool.h>
#endif

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

#include "php_vrzno.h"
#include "vrzno_object.c"
#include "vrzno_array.c"
#include "vrzno_expose.c"
#include "vrzno_functions.c"
#include "vrzno_db_statement.c"
#include "vrzno_db.c"

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
	vrzno_class_entry->get_iterator  = vrzno_array_get_iterator;
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
	vrzno_class_entry->ce_flags     |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
	zend_string *attribute_name_AllowDynamicProperties_class_vrzno = zend_string_init_interned("AllowDynamicProperties", sizeof("AllowDynamicProperties") - 1, 1);
	zend_add_class_attribute(vrzno_class_entry, attribute_name_AllowDynamicProperties_class_vrzno, 0);
	zend_string_release(attribute_name_AllowDynamicProperties_class_vrzno);
#endif

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
		Module.hasVrzno = true;
		Module.zvalMap = new WeakMap;
		Module.isTarget = Symbol('IS_TARGET');

		Module.tacked = new Set;

		const FinReg = globalThis.FinalizationRegistry || class { // Polyfill for cloudflare
			register(){};
			unregister(){};
		};

		const wRef = globalThis.WeakRef || class{ // Polyfill for cloudflare
			constructor(val){ this.val = val };
			deref() { return this.val };
		};

		Module.fRegistry = Module.fRegistry || new FinReg(zvalPtr => {
			// console.log('Garbage collecting zVal@'+zvalPtr);
			Module.ccall(
				'vrzno_expose_dec_refcount'
				, 'number'
				, ['number']
				, [zvalPtr]
			);
		});

		Module.bufferMaps = new WeakMap;

		Module.WeakerMap = Module.WeakerMap || (class WeakerMap
		{
			constructor(entries)
			{
				this.registry = new FinReg(held => this.delete(held));
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

				return this.map.set(key, new wRef(value));
			}

			values()
			{
				return [...this].map(v => v[1]);
			}
		});

		Module.classes  = Module.classes  || new WeakMap;
		Module._classes = Module._classes || new Module.WeakerMap;

		Module.marshalObject = ((zvalPtr) => {
			const nativeTarget = Module.ccall(
				'vrzno_expose_zval_is_target'
				, 'number'
				, ['number']
				, [zvalPtr]
			);

			if(nativeTarget && Module.targets.hasId(nativeTarget))
			{
				return Module.targets.get(nativeTarget);
			}

			const proxy = new Proxy({}, {
				ownKeys: (target) => {
					const keysLoc = Module.ccall(
						'vrzno_expose_object_keys'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					const keyJson = UTF8ToString(keysLoc);
					const keys = JSON.parse(keyJson);
					_free(keysLoc);

					keys.push(...Reflect.ownKeys(target));

					return keys;
				},
				has: (target, prop) => {
					switch(typeof prop)
					{
						case 'number':
							return !! Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, prop]
							);

						case 'string':
							const len = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);

							stringToUTF8(prop, namePtr, len);

							const propPtr = Module.ccall(
								'vrzno_expose_property_pointer'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, namePtr]
							);

							_free(namePtr);

							return propPtr;

						default:
							return false;
					}
				},
				get: (target, prop) => {
					let retPtr;
					switch(typeof prop)
					{
						case 'number':
							retPtr = Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, prop]
							);
							break;

						case 'string':
							const len = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);

							stringToUTF8(prop, namePtr, len);

							retPtr = Module.ccall(
								'vrzno_expose_property_pointer'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, namePtr]
							);

							_free(namePtr);

							break;

						default:
							return false;
					}

					const proxy = Module.zvalToJS(retPtr);

					if(proxy && ['function','object'].includes(typeof proxy))
					{
						Module.zvalMap.set(proxy, retPtr);
					}

					return proxy ?? Reflect.get(target, prop);
				},
				getOwnPropertyDescriptor: (target, prop) => {
					let retPtr;
					switch(typeof prop)
					{
						case 'number':
							retPtr = Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, prop]
							);
							break;

						case 'string':
							const len = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);

							stringToUTF8(prop, namePtr, len);

							retPtr = Module.ccall(
								'vrzno_expose_property_pointer'
								, 'number'
								, ['number', 'number']
								, [zvalPtr, namePtr]
							);

							_free(namePtr);

							break;

						default:
							return false;
					}

					const proxy = Module.zvalToJS(retPtr);

					if(proxy && ['function','object'].includes(typeof proxy))
					{
						Module.zvalMap.set(proxy, retPtr);
					}

					return {configurable: true, enumerable: true, value: target[prop]};
				},
			});

			Module.zvalMap.set(proxy, zvalPtr);
			Module.targets.add(proxy);

			Module.ccall(
				'vrzno_expose_inc_refcount'
				, 'number'
				, ['number']
				, [zvalPtr]
			);

			Module.fRegistry.register(proxy, zvalPtr, proxy);
			// console.log('freg a: ', zvalPtr);

			return proxy;
		});

		Module.callableToJs = Module.callableToJs || ( (funcPtr) => {
			if(Module.callables.has(funcPtr))
			{
				return Module.callables.get(funcPtr);
			}

			const wrapped = (...args) => {
				let paramPtrs = [], paramsPtr = null;

				if(args.length)
				{
					paramsPtr = Module.ccall(
						'vrzno_expose_create_params'
						, 'number'
						, ["number"]
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
					Module.ccall(
						'vrzno_expose_efree'
						, 'number'
						, ['number', 'number']
						, [paramsPtr, false]
					);
				}

				if(zvalPtr)
				{
					const result = Module.zvalToJS(zvalPtr);

					if(!result || !['function','object'].includes(typeof result))
					{
						Module.ccall(
							'vrzno_expose_efree'
							, 'number'
							, ['number']
							, [zvalPtr]
						);
					}

					return result;
				}
			};

			Object.defineProperty(wrapped, 'name', {value: `PHP_@{${funcPtr}}`});

			Module.ccall(
				'vrzno_expose_inc_crefcount'
				, 'number'
				, ['number']
				, [funcPtr]
			);

			Module.callables.set(funcPtr, wrapped);

			return wrapped;
		});

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

			zvalPtr = Module.ccall(
				'vrzno_expose_zval_deref'
				, 'number'
				, ['number']
				, [zvalPtr]
			);

			const type = Module.ccall(
				'vrzno_expose_type'
				, 'number'
				, ['number']
				, [zvalPtr]
			);

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

				if(!Module.targets.has(wrapped))
				{
					Module.targets.add(wrapped);
					Module.zvalMap.set(wrapped, zvalPtr);

					Module.ccall(
						'vrzno_expose_inc_refcount'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					Module.fRegistry.register(wrapped, zvalPtr, wrapped);
					// console.log('freg b: ', zvalPtr);
				}

				return wrapped;
			}

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
					return Module.ccall(
						'vrzno_expose_long'
						, 'number'
						, ['number']
						, [zvalPtr]
					);
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

				case IS_ARRAY:
				case IS_OBJECT:
					const proxy = Module.marshalObject(zvalPtr);

					Module.ccall(
						'vrzno_expose_inc_refcount'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					Module.fRegistry.register(proxy, zvalPtr, proxy);

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
				if(Module.zvalMap.has(value))
				{
					return Module.zvalMap.get(value);
				}
			}

			let zvalPtr;

			if(typeof value === 'undefined')
			{
				zvalPtr = Module.ccall(
					'vrzno_expose_create_undef'
					, 'number'
					, []
					, []
				);
			}
			else if(value === null)
			{
				zvalPtr = Module.ccall(
					'vrzno_expose_create_null'
					, 'number'
					, []
					, []
				);
			}
			else if([true, false].includes(value))
			{
				zvalPtr = Module.ccall(
					'vrzno_expose_create_bool'
					, 'number'
					, ["number"]
					, [value]
				);
			}
			else if(value && ['function','object'].includes(typeof value))
			{
				let index, existed;

				if(!Module.targets.has(value))
				{
					index = Module.targets.add(value);
					existed = false;
				}
				else
				{
					index = Module.targets.getId(value);
					existed = true;
				}

				const isFunction = typeof value === 'function' ? index : 0;
				const isConstructor = isFunction && !!(value.prototype && value.prototype.constructor);

				zvalPtr = Module.ccall(
					'vrzno_expose_create_object_for_target'
					, 'number'
					, ['number', 'number', 'number']
					, [index, isFunction, isConstructor]
				);

				Module.zvalMap.set(value, zvalPtr);

				if(!existed)
				{
					Module.ccall(
						'vrzno_expose_inc_refcount'
						, 'number'
						, ['number']
						, [zvalPtr]
					);

					Module.fRegistry.register(value, zvalPtr, value);
					// console.log('freg c: ', zvalPtr);
				}

			}
			else if(typeof value === "number")
			{
				if(Number.isInteger(value)) // Generate long zval
				{
					zvalPtr = Module.ccall(
						'vrzno_expose_create_long'
						, 'number'
						, ["number"]
						, [value]
					);
				}
				else if(Number.isFinite(value)) // Generate double zval
				{
					zvalPtr = Module.ccall(
						'vrzno_expose_create_double'
						, 'number'
						, ["number"]
						, [value]
					);
				}
			}
			else if(typeof value === "string") // Generate string zval
			{
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
			}

			return zvalPtr;
		});

		Module.UniqueIndex = Module.UniqueIndex || (class UniqueIndex
		{
			constructor()
			{
				this.byObject = new WeakMap();
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

		Module.callables = Module.callables || new Module.WeakerMap();

		Module.targets = Module.targets || new Module.UniqueIndex;

		Module.PdoParams = new WeakMap;
		Module.PdoD1Driver = Module.PdoD1Driver || (class PdoD1Driver
		{
			prepare(db, query)
			{
				console.log('prepare', {db, query});
				return db.prepare(query);
			}

			doer(db, query)
			{
				console.log('doer', {db, query});
			}
		});
		Module.pdoDriver = Module.pdoDriver || new Module.PdoD1Driver;
	});

#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return php_pdo_register_driver(&pdo_vrzno_driver);
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

	fci.size             = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);
	fci.object           = NULL;
	fci.retval           = (zval*) emalloc(sizeof(zval));
	fci.params           = params;
	fci.named_params     = 0;
	fci.param_count      = argc;

	fcc.function_handler = fptr;
	fcc.calling_scope    = NULL;
	fcc.called_scope     = NULL;
	fcc.object           = NULL;

	if(argc)
	{
		int i;
		for(i = 0; i < argc; i++)
		{
			params[i] = *argv[i];
		}
	}

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
