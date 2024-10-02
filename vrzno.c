/* vrzno extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "SAPI.h"

#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
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
#include "vrzno_fetch.c"
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
	// vrzno_object_handlers.get_class_name     = vrzno_get_class_name;

	php_register_url_stream_wrapper("http", &php_stream_fetch_wrapper);
	php_register_url_stream_wrapper("https", &php_stream_fetch_wrapper);

	EM_ASM({
		const IS_UNDEF  = 0;
		const IS_NULL   = 1;
		const IS_FALSE  = 2;
		const IS_TRUE   = 3;
		const IS_LONG   = 4;
		const IS_DOUBLE = 5;
		const IS_STRING = 6;
		const IS_ARRAY  = 7;
		const IS_OBJECT = 8;

		Module.hasVrzno = true;
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

		const getRegistry = weakerMap => {
			const registry = new FinalizationRegistry(key => {
				if(weakerMap.registry !== registry)
				{
					return;
				}

				if(weakerMap.map.has(key) && weakerMap.map.get(key).deref())
				{
					return;
				}

				weakerMap.delete(key);
			});

			return registry;
		};

		Module.WeakerMap = Module.WeakerMap || (class WeakerMap
		{
			constructor(entries)
			{
				this.map = new Map;
				this.registry = getRegistry(this);
				entries && entries.forEach(([key, value]) => this.set(key, value));
			}

			get size()
			{
				return this.map.size;
			}

			clear()
			{
				this.registry = getRegistry(this);
				this.map.clear();
			}

			delete(key)
			{
				if(!this.has(key))
				{
					return;
				}

				this.registry.unregister(this.get(key));
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

				const value = this.map.get(key).deref();

				if(!value)
				{
					this.map.delete(key);
				}

				return value;
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

				return Boolean(result);
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

		Module.marshalObject = ((zv, type) => {
			const nativeTarget = Module.ccall(
				'vrzno_expose_target'
				, 'number'
				, ['number']
				, [zv]
			);

			if(nativeTarget && Module.targets.hasId(nativeTarget))
			{
				return Module.targets.get(nativeTarget);
			}

			const proxy = new Proxy({}, {
				ownKeys: (target) => {
					let keysLoc;
					if(type === IS_ARRAY)
					{
						keysLoc = Module.ccall(
							'vrzno_expose_array_keys'
							, 'number'
							, ['number']
							, [zv]
						);
					}
					else if(type === IS_OBJECT)
					{
						keysLoc = Module.ccall(
							'vrzno_expose_object_keys'
							, 'number'
							, ['number']
							, [zv]
						);
					}

					if(keysLoc)
					{
						const keyJson = UTF8ToString(keysLoc);
						const keys = JSON.parse(keyJson);
						_free(keysLoc);
						keys.push(...Reflect.ownKeys(target));
						return keys;
					}

					return [];
				},
				has: (target, prop) => {
					switch(typeof prop)
					{
						case 'number':
							return !! Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [zv, prop]
							);

						case 'string':
							const len = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);

							stringToUTF8(prop, namePtr, len);

							const propPtr = Module.ccall(
								'vrzno_expose_property_pointer'
								, 'number'
								, ['number', 'number']
								, [zv, namePtr]
							);

							_free(namePtr);

							return propPtr;

						default:
							return false;
					}
				},
				get: (target, prop) => {
					let retPtr;
					if(prop === Symbol.iterator)
					{
						const max = Module.ccall(
							'vrzno_expose_array_length'
							, 'number'
							, ['number']
							, [zv]
						);

						const iterator = () => {
							let current = -1;
							return {
								next() {
									const done = ++current >= max;
									return {done, value: Module.zvalToJS(Module.ccall(
										'vrzno_expose_dimension_pointer'
										, 'number'
										, ['number', 'number']
										, [zv, current]
									))};
								}
							}
						};

						Module.ccall(
							'vrzno_expose_inc_refcount'
							, 'number'
							, ['number']
							, [zv]
						);

						Module.fRegistry.register(iterator, zv, iterator);
						// console.log('Freg %s, %d', 'a', zv);
						return iterator;
					}

					if(prop === Symbol.toPrimitive)
					{
						const method = '__toString';
						const len = lengthBytesUTF8(method) + 1;
						const loc = _malloc(len);
						stringToUTF8(method, loc, len);

						const methodPtr = Module.ccall(
							'vrzno_expose_method_pointer'
							, 'number'
							, ['number', 'number']
							, [zv, loc]
						);

						_free(loc);

						return () => Module.callableToJs(methodPtr, zv)();
					}

					switch(typeof prop)
					{
						case 'number':
							retPtr = Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [zv, prop]
							);
							break;

						case 'string':
							prop = String(prop);
							const len = lengthBytesUTF8(prop) + 1;
							const loc = _malloc(len);
							stringToUTF8(prop, loc, len);

							if(type === IS_OBJECT)
							{
								const methodPtr = Module.ccall(
									'vrzno_expose_method_pointer'
									, 'number'
									, ['number', 'number']
									, [zv, loc]
								);

								if(methodPtr)
								{
									return Module.callableToJs(methodPtr, zv);
								}

								retPtr = Module.ccall(
									'vrzno_expose_property_pointer'
									, 'number'
									, ['number', 'number']
									, [zv, loc]
								);
							}
							else if(type === IS_ARRAY)
							{
								retPtr = Module.ccall(
									'vrzno_expose_key_pointer'
									, 'number'
									, ['number', 'number']
									, [zv, loc]
								);
							}

							_free(loc);

							break;

						default:
							return false;
					}

					if(!retPtr)
					{
						return;
					}

					const proxy = Module.zvalToJS(retPtr);

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
								, [zv, prop]
							);
							break;

						case 'string':
							const len = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);
							stringToUTF8(prop, namePtr, len);

							if(type === IS_OBJECT)
							{
								retPtr = Module.ccall(
									'vrzno_expose_property_pointer'
									, 'number'
									, ['number', 'number']
									, [zv, namePtr]
								);
							}
							else if(type === IS_ARRAY)
							{
								retPtr = Module.ccall(
									'vrzno_expose_key_pointer'
									, 'number'
									, ['number', 'number']
									, [zv, namePtr]
								);
							}

							_free(namePtr);

							break;

						default:
							return false;
					}

					const proxy = Module.zvalToJS(retPtr);

					return {configurable: true, enumerable: true, value: target[prop]};
				},
			});

			Module.ccall(
				'vrzno_expose_inc_refcount'
				, 'number'
				, ['number']
				, [zv]
			);
			// Module.fRegistry.register(proxy, zv, proxy);
			// console.log('Freg %s, %d', 'b', zv);
			// Module.targets.add(proxy);

			return proxy;
		});

		Module.callableToJs = Module.callableToJs || ((funcPtr, objPtr = null) => {
			if(Module.callables.has(funcPtr))
			{
				return Module.callables.get(funcPtr);
			}

			const wrapped = (...args) => {
				let paramsPtr = null;

				if(args.length)
				{
					paramsPtr = Module.ccall(
						'vrzno_expose_create_params'
						, 'number'
						, ["number"]
						, [args.length]
					);

					for(let i = 0; i < args.length; i++)
					{
						Module.jsToZval(args[i], getValue(i * 4 + paramsPtr, '*'));
					}
				}

				const zv = Module.ccall(
					'vrzno_exec_callback'
					, 'number'
					, ['number','number','number','number']
					, [funcPtr, paramsPtr, args.length, objPtr]
				);

				if(args.length)
				{
					Module.ccall(
						'vrzno_expose_efree'
						, 'number'
						, ['number']
						, [paramsPtr]
					);
				}

				if(zv)
				{
					const result = Module.zvalToJS(zv);

					if(result && ['function','object'].includes(typeof result))
					{
						Module.ccall(
							'vrzno_expose_inc_refcount'
							, 'number'
							, ['number']
							, [zv]
						);

						Module.fRegistry.register(result, zv, result);
						// console.log('Freg %s, %d', 'c', zv);
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

		Module.zvalToJS = Module.zvalToJS || (zv => {
			if(!zv)
			{
				return;
			}

			zv = Module.ccall(
				'vrzno_expose_zval_deref'
				, 'number'
				, ['number']
				, [zv]
			);

			const isNative = Module.ccall(
				'vrzno_expose_target'
				, 'number'
				, ['number']
				, [zv]
			);

			if(isNative)
			{
				return Module.targets.get(isNative);
			}

			const callable = Module.ccall(
				'vrzno_expose_callable'
				, 'number'
				, ['number']
				, [zv]
			);

			let valPtr;

			if(callable)
			{
				const wrapped = Module.callableToJs(callable);

				if(!Module.targets.has(wrapped))
				{
					Module.targets.add(wrapped);

					Module.ccall(
						'vrzno_expose_inc_refcount'
						, 'number'
						, ['number']
						, [zv]
					);

					Module.fRegistry.register(wrapped, zv, wrapped);
					// console.log('Freg %s, %d', 'd', zv);
				}

				return wrapped;
			}

			const type = Module.ccall(
				'vrzno_expose_type'
				, 'number'
				, ['number']
				, [zv]
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
					return Module.ccall(
						'vrzno_expose_long'
						, 'number'
						, ['number']
						, [zv]
					);
					break;

				case IS_DOUBLE:
					valPtr = Module.ccall(
						'vrzno_expose_double'
						, 'number'
						, ['number']
						, [zv]
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
						, [zv]
					);

					if(!valPtr)
					{
						return null;
					}

					return UTF8ToString(valPtr);
					break;

				case IS_ARRAY:
				case IS_OBJECT:
					return Module.marshalObject(zv, type);
				default:
					return null;
					break;
			}
		});

		Module.jsToZval = Module.jsToZval || ((value, rv) => {
			if(typeof value === 'undefined')
			{
				Module.ccall(
					'vrzno_expose_create_undef'
					, 'number'
					, ['number']
					, [rv]
				);
			}
			else if(value === null)
			{
				Module.ccall(
					'vrzno_expose_create_null'
					, 'number'
					, ['number']
					, [rv]
				);
			}
			else if([true, false].includes(value))
			{
				Module.ccall(
					'vrzno_expose_create_bool'
					, 'number'
					, ['number', 'number']
					, [value, rv]
				);
			}
			else if(value && ['function','object'].includes(typeof value))
			{
				const index = Module.targets.add(value);
				const isFunction = typeof value === 'function' ? index : 0;
				const isConstructor = isFunction && !!(value.prototype && value.prototype.constructor);

				Module.ccall(
					'vrzno_expose_create_object_for_target'
					, 'number'
					, ['number', 'number', 'number', 'number']
					, [index, isFunction, isConstructor, rv]
				);

				Module.tacked.add(value);

				Module.ccall(
					'vrzno_expose_inc_refcount'
					, 'number'
					, ['number']
					, [rv]
				);

				Module.fRegistry.register(value, rv, value);
				// console.log('Freg %s, %d', 'e', rv);
			}
			else if(typeof value === "number")
			{
				if(Number.isInteger(value)) // Generate long zval
				{
					Module.ccall(
						'vrzno_expose_create_long'
						, 'number'
						, ['number', 'number']
						, [value, rv]
					);
				}
				else if(Number.isFinite(value)) // Generate double zval
				{
					Module.ccall(
						'vrzno_expose_create_double'
						, 'number'
						, ['number', 'number']
						, [value, rv]
					);
				}
			}
			else if(typeof value === "string") // Generate string zval
			{
				const len      = lengthBytesUTF8(value) + 1;
				const strLoc   = _malloc(len);

				stringToUTF8(value, strLoc, len);

				Module.ccall(
					'vrzno_expose_create_string'
					, 'number'
					, ['number', 'number']
					, [strLoc, rv]
				);

				_free(strLoc);
			}
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

		Module.classes = Module.classes || new WeakMap();
		Module._classes = Module._classes || new Module.WeakerMap();
		Module.callables = Module.callables || new Module.WeakerMap();
		Module.targets = Module.targets || new Module.UniqueIndex;

		Module.targets.add(globalThis);
		Module.PdoParams = new WeakMap;
	});

#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	// return php_pdo_register_driver(&pdo_vrzno_driver);
	return SUCCESS;
}

PHP_MINFO_FUNCTION(vrzno)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Vrzno support for php-wasm", "enabled");
	php_info_print_table_end();

	if (!sapi_module.phpinfo_as_text) {
		php_info_print_box_start(0);
		PUTS("<a href=\"https://github.com/seanmorris/php-wasm\">");
		PUTS("<img border=\"0\" src=\"");
		PUTS(VRZNO_ICON_DATA_URI "\" alt=\"Zend logo\" /></a>\n");
		php_info_print_box_end();
	}
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

zval* EMSCRIPTEN_KEEPALIVE vrzno_exec_callback(zend_function *func, zval **argv, int argc, zval *obj)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
	zval params[argc];

	fci.size             = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);
	fci.object           = Z_OBJ_P(obj);
	fci.retval           = (zval*) emalloc(sizeof(zval)); // @todo: Figure out when to clear this...
	fci.params           = params;
	fci.named_params     = 0;
	fci.param_count      = argc;

	fcc.function_handler = func;
	fcc.calling_scope    = NULL;
	fcc.called_scope     = NULL;
	fcc.object           = Z_OBJ_P(obj);

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
