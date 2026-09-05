/* vrzno extension for PHP */

#include "vrzno_private.h"
#include "vrzno_arginfo.h"

PHP_RSHUTDOWN_FUNCTION(vrzno)
{
	EM_ASM({
		if(Module.ownedZvalRegistry)
		{
			Module.ownedZvalRegistry.releaseAll();
		}

		Module.vrznoGeneration++;
		Module.tacked.clear();
		Module.classes = new WeakMap();
		Module._classes = new Module.WeakerMap();
		Module._objects = new Module.WeakerMap();
		Module._arrays = new Module.WeakerMap();
		Module._callables = new Module.WeakerMap();
		Module.targets.clear();
		Module.targets.add(globalThis);
	});

	return SUCCESS;
}

PHP_MINIT_FUNCTION(vrzno)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Vrzno", class_Vrzno_methods);
	vrzno_class_entry = zend_register_internal_class(&ce);

	vrzno_class_entry->create_object = vrzno_create_object;
	vrzno_class_entry->get_iterator  = vrzno_array_get_iterator;

#if PHP_VERSION_ID >= 80100
	vrzno_class_entry->ce_flags |= ZEND_ACC_NOT_SERIALIZABLE;
#else
	vrzno_class_entry->serialize = zend_class_serialize_deny;
	vrzno_class_entry->unserialize = zend_class_unserialize_deny;
#endif

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
	vrzno_class_entry->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
	zend_string *attribute_name_AllowDynamicProperties_class_vrzno = zend_string_init_interned("AllowDynamicProperties", sizeof("AllowDynamicProperties") - 1, 1);
	zend_add_class_attribute(vrzno_class_entry, attribute_name_AllowDynamicProperties_class_vrzno, 0);
	zend_string_release(attribute_name_AllowDynamicProperties_class_vrzno);
#endif

	memcpy(&vrzno_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	vrzno_object_handlers.offset = XtOffsetOf(vrzno_object, zo);
	vrzno_object_handlers.clone_obj = NULL;

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

#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	EM_ASM({
		Module.hasVrzno = true;

		const IS_UNDEF    = 0;
		const IS_NULL     = 1;
		const IS_FALSE    = 2;
		const IS_TRUE     = 3;
		const IS_LONG     = 4;
		const IS_DOUBLE   = 5;
		const IS_STRING   = 6;
		const IS_ARRAY    = 7;
		const IS_OBJECT   = 8;
		const IS_RESOURCE = 9;

		const IS_INDIRECT = 12;

		Module.vrznoGeneration = (Module.vrznoGeneration || 0) + 1;
		Module.tacked = new Set;
		Module.vrznoArrayView = target => {
			if(target instanceof ArrayBuffer)
			{
				if(!Module.bufferMaps.has(target))
				{
					Module.bufferMaps.set(target, new Uint8Array(target));
				}

				return Module.bufferMaps.get(target);
			}

			return target;
		};
		Module.vrznoPhpTruthy = value => !(
			value === undefined
			|| value === null
			|| value === false
			|| value === 0
			|| (typeof value === 'string' && value.length === 0)
			|| value === '0'
		);
		Module.vrznoNormalizeArrayKey = key => {
			if(typeof key !== 'string')
			{
				return key;
			}

			const numericKey = Number(key);
			return Number.isInteger(numericKey)
				&& numericKey >= -2147483648
				&& numericKey <= 2147483647
				&& String(numericKey) === key
					? numericKey
					: key;
		};

		const origZval = Symbol('origZval');
		const proxyGeneration = Symbol('vrznoGeneration');

		const _FinalizationRegistry = globalThis.FinalizationRegistry || class { // Polyfill for cloudflare
			register(){};
			unregister(){};
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

				if(this.has(key))
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

		const getRegistry = weakerMap => {
			const registry = new _FinalizationRegistry(key => {
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

		const ownedZvalRegistryWrapper = class {
			constructor()
			{
				this.unregisterTokens = new WeakMap;
				this.entries = new Map;
				this.allocations = 0;
				this.releases = 0;
				this.registry = new _FinalizationRegistry(zv => {
					if(!this.entries.has(zv))
					{
						return;
					}

					this.entries.delete(zv);
					this.destroy(zv);
				});
			}

			destroy(zv)
			{
				Module.ccall(
					'vrzno_expose_destroy_zval'
					, null
					, ['number']
					, [zv]
				);
				this.releases++;
			}

			register(target, zv, unregisterToken = target)
			{
				this.allocations++;

				if(this.unregisterTokens.has(unregisterToken))
				{
					this.destroy(zv);
					return;
				}

				const registryToken = {};
				this.registry.register(target, zv, registryToken);
				this.unregisterTokens.set(unregisterToken, {zv, registryToken});
				this.entries.set(zv, registryToken);
			}

			release(unregisterToken)
			{
				if(!this.unregisterTokens.has(unregisterToken))
				{
					return false;
				}

				const entry = this.unregisterTokens.get(unregisterToken);
				const zv = entry.zv;
				const registryToken = entry.registryToken;
				this.registry.unregister(registryToken);
				this.unregisterTokens.delete(unregisterToken);
				this.entries.delete(zv);
				this.destroy(zv);
				return true;
			}

			releaseAll()
			{
				for(const [zv, registryToken] of this.entries)
				{
					this.registry.unregister(registryToken);
					this.destroy(zv);
				}

				this.entries.clear();
				this.unregisterTokens = new WeakMap;
			}

			get outstanding()
			{
				return this.allocations - this.releases;
			}
		};

		const wRef = globalThis.WeakRef || class { // Polyfill for cloudflare
			constructor(val){ this.val = val };
			deref() { return this.val };
		};

		Module.ownedZvalRegistry = new ownedZvalRegistryWrapper;
		Module.vrznoCopyZval = zv => Module.ccall(
			'vrzno_expose_copy_zval'
			, 'number'
			, ['number']
			, [zv]
		);
		Module.vrznoDestroyZval = zv => Module.ccall(
			'vrzno_expose_destroy_zval'
			, null
			, ['number']
			, [zv]
		);
		Module.vrznoAssertGeneration = generation => {
			if(generation !== Module.vrznoGeneration)
			{
				throw new ReferenceError('Vrzno value belongs to a previous PHP runtime.');
			}
		};
		Module.vrznoThrowRuntimeError = error => {
			const message = error && error.message ? error.message : String(error);
			const len = lengthBytesUTF8(message) + 1;
			const loc = _malloc(len);
			stringToUTF8(message, loc, len);
			Module.ccall('vrzno_expose_runtime_error', null, ['number'], [loc]);
			_free(loc);
		};
		Module.vrznoOwnershipStats = () => ({
			allocations: Module.ownedZvalRegistry.allocations,
			releases: Module.ownedZvalRegistry.releases,
			outstanding: Module.ownedZvalRegistry.outstanding,
			targets: Module.targets ? Module.targets.references.size : 0,
		});

		Module.bufferMaps = new WeakMap;

		Module.marshalZObject = ((zo, zv = 0) => {
			const nativeTargetId = Module.ccall(
				'vrzno_expose_target'
				, 'number'
				, ['number']
				, [zo]
			);

			if(nativeTargetId)
			{
				return Module.targets.get(nativeTargetId);
			}

			if(Module._objects.has(zo))
			{
				return Module._objects.get(zo);
			}

			const generation = Module.vrznoGeneration;
			const ownedZval = zv
				? Module.vrznoCopyZval(zv)
				: Module.ccall('vrzno_expose_copy_object', 'number', ['number'], [zo]);
			const proxy = new Proxy({}, {
				ownKeys: (target) => {
					Module.vrznoAssertGeneration(generation);
					const keysLoc = Module.ccall(
						'vrzno_expose_object_keys'
						, 'number'
						, ['number']
						, [zo]
					);

					if(keysLoc)
					{
						const keyJson = UTF8ToString(keysLoc);
						const keys = JSON.parse(keyJson);
						_free(keysLoc);
						keys.push(...Reflect.ownKeys(target));
						return [...new Set(keys)];
					}

					return Reflect.ownKeys(target);
				},

				has: (target, prop) => {
					Module.vrznoAssertGeneration(generation);
					if(typeof prop === 'symbol')
					{
						return Reflect.has(target, prop);
					}
					const len = lengthBytesUTF8(prop) + 1;
					const namePtr = _malloc(len);

					stringToUTF8(prop, namePtr, len);

					const exists = Module.ccall(
						'vrzno_expose_has_property'
						, 'number'
						, ['number', 'number']
						, [zo, namePtr]
					);

					_free(namePtr);

					return Boolean(exists);
				},

				get: (target, prop) => {
					Module.vrznoAssertGeneration(generation);
					let retPtr;
					if(prop === origZval || prop === proxyGeneration)
					{
						return Reflect.get(target, prop);
					}
					if(prop === Symbol.iterator)
					{
						return;
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
							, [zo, loc]
						);

						_free(loc);

						if(!methodPtr)
						{
							return;
						}

						return Module.callableToJs(methodPtr, zo);
					}

					prop = String(prop);
					const len = lengthBytesUTF8(prop) + 1;
					const loc = _malloc(len);
					stringToUTF8(prop, loc, len);

					const methodPtr = Module.ccall(
						'vrzno_expose_method_pointer'
						, 'number'
						, ['number', 'number']
						, [zo, loc]
					);

					if(methodPtr)
					{
						_free(loc);
						const wrapped = Module.callableToJs(methodPtr, zo);
						return wrapped;
					}

					retPtr = Module.ccall(
						'vrzno_expose_read_property'
						, 'number'
						, ['number', 'number']
						, [zo, loc]
					);

					_free(loc);

					if(!retPtr)
					{
						return;
					}

					return Module.consumeZval(retPtr);
				},

				getOwnPropertyDescriptor: (target, prop) => {
					Module.vrznoAssertGeneration(generation);
					if(typeof prop === 'symbol')
					{
						return Reflect.getOwnPropertyDescriptor(target, prop);
					}
					prop = String(prop);
					const len = lengthBytesUTF8(prop) + 1;
					const namePtr = _malloc(len);
					stringToUTF8(prop, namePtr, len);

					const retPtr = Module.ccall(
						'vrzno_expose_read_property'
						, 'number'
						, ['number', 'number']
						, [zo, namePtr]
					);

					_free(namePtr);

					if(!retPtr)
					{
						return undefined;
					}

					return {configurable: true, enumerable: true, value: Module.consumeZval(retPtr)};
				},
			});

			Object.defineProperties(proxy, {
				[origZval]: {value: ownedZval},
				[proxyGeneration]: {value: generation},
			});
			Module.ownedZvalRegistry.register(proxy, ownedZval, proxy);
			Module._objects.set(zo, proxy);

			return proxy;
		});

		Module.marshalZArray = ((za, zv) => {
			if(Module._arrays.has(za))
			{
				return Module._arrays.get(za);
			}

			const generation = Module.vrznoGeneration;
			const ownedZval = Module.vrznoCopyZval(zv);
			const proxy = new Proxy({}, {
				ownKeys: (target) => {
					Module.vrznoAssertGeneration(generation);
					const keysLoc = Module.ccall(
						'vrzno_expose_array_keys'
						, 'number'
						, ['number']
						, [za]
					);

					if(keysLoc)
					{
						const keyJson = UTF8ToString(keysLoc);
						const keys = JSON.parse(keyJson);
						_free(keysLoc);
						keys.push(...Reflect.ownKeys(target));
						return [...new Set(keys)];
					}

					return [];
				},
				has: (target, prop) => {
					Module.vrznoAssertGeneration(generation);
					if(typeof prop === 'symbol')
					{
						return Reflect.has(target, prop);
					}
					prop = Module.vrznoNormalizeArrayKey(prop);
					switch(typeof prop)
					{
						case 'number':
							return !! Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [za, prop]
							);

						case 'string':
							const len = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);

							stringToUTF8(prop, namePtr, len);

							const propPtr = Module.ccall(
								'vrzno_expose_key_pointer'
								, 'number'
								, ['number', 'number']
								, [za, namePtr]
							);

							_free(namePtr);

							return propPtr;

						default:
							return false;
					}
				},
				get: (target, prop) => {
					Module.vrznoAssertGeneration(generation);
					let retPtr;
					if(prop === origZval || prop === proxyGeneration)
					{
						return Reflect.get(target, prop);
					}
					if(prop === 'length')
					{
						return Module.ccall(
							'vrzno_expose_array_length'
							, 'number'
							, ['number']
							, [za]
						);
					}
					prop = Module.vrznoNormalizeArrayKey(prop);
					if(prop === Symbol.iterator)
					{
						const max = Module.ccall(
							'vrzno_expose_array_length'
							, 'number'
							, ['number']
							, [za]
						);

						// A detached factory can outlive the proxy that exposed it.
						const factoryOwner = Module.vrznoCopyZval(ownedZval);
						const iterator = () => {
							Module.vrznoAssertGeneration(generation);
							let current = -1;
							const iteratorObject = {
								next() {
									Module.vrznoAssertGeneration(generation);
									const done = ++current >= max;
									if(done)
									{
										return {done: true, value: undefined};
									}

									return {done, value: Module.zvalToJS(Module.ccall(
										'vrzno_expose_array_value_at'
										, 'number'
										, ['number', 'number']
										, [za, current]
									))};
								}
							};

							const iteratorOwner = Module.vrznoCopyZval(factoryOwner);
							Module.ownedZvalRegistry.register(iteratorObject, iteratorOwner, iteratorObject);
							return iteratorObject;
						};

						Module.ownedZvalRegistry.register(iterator, factoryOwner, iterator);
						return iterator;
					}

					switch(typeof prop)
					{
						case 'symbol':
							return Reflect.get(target, prop);
						case 'number':
							retPtr = Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [za, prop]
							);
							break;

						case 'string':
							prop = String(prop);
							const len = lengthBytesUTF8(prop) + 1;
							const loc = _malloc(len);
							stringToUTF8(prop, loc, len);

							retPtr = Module.ccall(
								'vrzno_expose_key_pointer'
								, 'number'
								, ['number', 'number']
								, [za, loc]
							);

							_free(loc);

							break;

						default:
							return false;
					}

					if(!retPtr)
					{
						return;
					}

					return Module.zvalToJS(retPtr);
				},
				getOwnPropertyDescriptor: (target, prop) => {
					Module.vrznoAssertGeneration(generation);
					if(typeof prop === 'symbol')
					{
						return Reflect.getOwnPropertyDescriptor(target, prop);
					}
					if(prop === 'length')
					{
						const value = Module.ccall(
							'vrzno_expose_array_length'
							, 'number'
							, ['number']
							, [za]
						);

						return {
							value,
							writable: false,
							enumerable: false,
							configurable: true,
						};
					}
					prop = Module.vrznoNormalizeArrayKey(prop);
					let retPtr;
					switch(typeof prop)
					{
						case 'number':
							retPtr = Module.ccall(
								'vrzno_expose_dimension_pointer'
								, 'number'
								, ['number', 'number']
								, [za, prop]
							);
							break;

						case 'string':
							const len = lengthBytesUTF8(prop) + 1;
							const namePtr = _malloc(len);
							stringToUTF8(prop, namePtr, len);

							retPtr = Module.ccall(
								'vrzno_expose_key_pointer'
								, 'number'
								, ['number', 'number']
								, [za, namePtr]
							);

							_free(namePtr);

							break;

						default:
							return undefined;
					}

					if(!retPtr)
					{
						return undefined;
					}

					return {configurable: true, enumerable: true, value: Module.zvalToJS(retPtr)};
				},
			});

			Object.defineProperties(proxy, {
				[origZval]: {value: ownedZval},
				[proxyGeneration]: {value: generation},
			});
			Module.ownedZvalRegistry.register(proxy, ownedZval, proxy);
			Module._arrays.set(za, proxy);

			return proxy;
		});

		Module.callableToJs = ((funcPtr, zo = null, zv = 0, cacheKey = `method:${zo}:${funcPtr}`) => {
			const generation = Module.vrznoGeneration;
			cacheKey = `${generation}:${cacheKey}`;
			const cached = Module._callables.get(cacheKey);
			if(cached)
			{
				return cached;
			}

			// Only the first wrapper owns a reference. Cache hits must not copy it.
			const ownedZval = zv
				? Module.vrznoCopyZval(zv)
				: Module.ccall('vrzno_expose_copy_object', 'number', ['number'], [zo]);
			const callableZval = zv ? ownedZval : 0;
			const wrapped = (...args) => {
				Module.vrznoAssertGeneration(generation);

				let paramsPtr = 0;
				let zv = 0;
				try
				{
					if(args.length)
					{
						paramsPtr = Module.ccall(
							'vrzno_expose_create_params'
							, 'number'
							, ['number']
							, [args.length]
						);

						for(let i = 0; i < args.length; i++)
						{
							const paramPtr = Module.ccall(
								'vrzno_expose_param_at'
								, 'number'
								, ['number', 'number']
								, [paramsPtr, i]
							);
							Module.jsToZval(args[i], paramPtr);
						}
					}

					if(callableZval)
					{
						zv = Module.ccall(
							'vrzno_exec_zval_callback'
							, 'number'
							, ['number','number','number']
							, [callableZval, paramsPtr, args.length]
						);
					}
					else
					{
						zv = Module.ccall(
							'vrzno_exec_callback'
							, 'number'
							, ['number','number','number','number']
							, [funcPtr, paramsPtr, args.length, zo]
						);
					}
				}
				finally
				{
					if(paramsPtr)
					{
						Module.ccall(
							'vrzno_expose_destroy_params'
							, null
							, ['number', 'number']
							, [paramsPtr, args.length]
						);
					}
				}

				if(zv)
				{
					try
					{
						return Module.zvalToJS(zv);
					}
					finally
					{
						Module.vrznoDestroyZval(zv);
					}
				}
			};

			Object.defineProperty(wrapped, 'name', {value: `PHP_@{${funcPtr.toString(/*16*/)}}`});

			if(ownedZval)
			{
				if(callableZval)
				{
					Object.defineProperties(wrapped, {
						[origZval]: {value: callableZval},
						[proxyGeneration]: {value: generation},
					});
				}
				Module.ownedZvalRegistry.register(wrapped, ownedZval, wrapped);
			}

			Module._callables.set(cacheKey, wrapped);
			return wrapped;
		});

		Module.resourceToJs = ((zr, zv) => {
			const generation = Module.vrznoGeneration;
			const ownedZval = Module.vrznoCopyZval(zv);
			const proxy = {};
			Object.defineProperties(proxy, {
				[origZval]: {value: ownedZval},
				[proxyGeneration]: {value: generation},
			});
			Module.ownedZvalRegistry.register(proxy, ownedZval, proxy);
			Object.freeze(proxy);
			return proxy;
		});

		Module.zvalToJS = (zv => {
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

			const nativeTargetId = Module.ccall(
				'vrzno_expose_zval_target'
				, 'number'
				, ['number']
				, [zv]
			);

			if(nativeTargetId)
			{
				return Module.targets.get(nativeTargetId);
			}

			let type = Module.ccall(
				'vrzno_expose_type'
				, 'number'
				, ['number']
				, [zv]
			);

			if(type === IS_INDIRECT)
			{
				zv = Module.ccall(
					'vrzno_expose_zval_direct'
					, 'number'
					, ['number']
					, [zv]
				);

				// The indirect zval might have pointed to a reference
				// Deref it again just in case.
				zv = Module.ccall(
					'vrzno_expose_zval_deref'
					, 'number'
					, ['number']
					, [zv]
				);

				// Get the correct type of the zval
				type = Module.ccall(
					'vrzno_expose_type'
					, 'number'
					, ['number']
					, [zv]
				);
			}

			const zf = Module.ccall(
				'vrzno_expose_callable'
				, 'number'
				, ['number']
				, [zv]
			);

			if(zf && type !== IS_STRING)
			{
				// Keep distinct closures and callable arrays separate even when they
				// resolve to the same function. The wrapper keeps this identity alive.
				const identity = Module.ccall(
					type === IS_OBJECT ? 'vrzno_expose_object' : 'vrzno_expose_array'
					, 'number'
					, ['number']
					, [zv]
				);
				return Module.callableToJs(zf, null, zv, `${type}:${identity}`);
			}

			let valPtr;
			switch(type)
			{
				case IS_UNDEF:
					return undefined;

				case IS_NULL:
					return null;

				case IS_TRUE:
					return true;

				case IS_FALSE:
					return false;

				case IS_LONG:
					return Module.ccall(
						'vrzno_expose_long'
						, 'number'
						, ['number']
						, [zv]
					);

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

					const valueLength = Module.ccall(
						'vrzno_expose_string_length'
						, 'number'
						, ['number']
						, [zv]
					);

					return UTF8ArrayToString(HEAPU8, valPtr, valueLength, true);

				case IS_ARRAY:
					const za = Module.ccall(
						'vrzno_expose_array'
						, 'number'
						, ['number']
						, [zv]
					);
					return Module.marshalZArray(za, zv);

				case IS_OBJECT:
					const zo = Module.ccall(
						'vrzno_expose_object'
						, 'number'
						, ['number']
						, [zv]
					);
					return Module.marshalZObject(zo, zv);

				case IS_RESOURCE:
					const zp = Module.ccall(
						'vrzno_expose_resource'
						, 'number'
						, ['number']
						, [zv]
					);
					return Module.resourceToJs(zp, zv);

				default:
					console.warn(
						'ZVal at 0x%s has invalid type %d (0b%s)'
						, Number(zv).toString(16)
						, type
						, Number(type).toString(2)
					);
					return null;
			}
		});

		Module.consumeZval = zv => {
			if(!zv)
			{
				return undefined;
			}

			try
			{
				return Module.zvalToJS(zv);
			}
			finally
			{
				Module.vrznoDestroyZval(zv);
			}
		};

		Module.jsToZval = ((value, rv) => {
			Module.ccall(
				'vrzno_expose_create_null'
				, null
				, ['number']
				, [rv]
			);

			if(typeof value === 'undefined')
			{
				return;
			}
			else if(value === null)
			{
				return;
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
				if(value[origZval])
				{
					Module.vrznoAssertGeneration(value[proxyGeneration]);
					Module.ccall(
						'vrzno_expose_copy_into'
						, null
						, ['number', 'number',]
						, [rv, value[origZval]]
					);
					return;
				}

				const index = Module.targets.add(value);
				const isConstructor = typeof value === 'function'
					&& !!(value.prototype && value.prototype.constructor);

				Module.tacked.add(value);

				Module.ccall(
					'vrzno_expose_create_object_for_target'
					, 'number'
					, ['number', 'number', 'number']
					, [index, isConstructor, rv]
				);
			}
			else if(typeof value === 'number')
			{
				if(Number.isInteger(value) && value >= -2147483648 && value <= 2147483647)
				{
					Module.ccall(
						'vrzno_expose_create_long'
						, 'number'
						, ['number', 'number']
						, [value, rv]
					);
				}
				else
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
				const len = lengthBytesUTF8(value) + 1;
				const loc = _malloc(len);

				stringToUTF8(value, loc, len);

				Module.ccall(
					'vrzno_expose_create_string'
					, 'number'
					, ['number', 'number', 'number']
					, [loc, len - 1, rv]
				);

				_free(loc);
			}
			else if(typeof value === 'bigint' || typeof value === 'symbol')
			{
				const message = `Cannot convert JavaScript ${typeof value} to PHP`;
				const len = lengthBytesUTF8(message) + 1;
				const loc = _malloc(len);
				stringToUTF8(message, loc, len);
				Module.ccall('vrzno_expose_type_error', null, ['number'], [loc]);
				_free(loc);
			}
		});

		Module.UniqueIndex = Module.UniqueIndex || (class UniqueIndex
		{
			constructor()
			{
				this.byObject = new WeakMap();
				this.byInteger = new Module.WeakerMap();
				this.references = new Map();
				this.id = 0;

				Object.defineProperty(this, 'add', {
					configurable: false
					, writable:   false
					, value: (callback) => {

						if(this.byObject.has(callback))
						{
							const id = this.byObject.get(callback);
							this.references.set(id, 1 + (this.references.get(id) || 0));

							return id;
						}

						const newid = ++this.id;

						this.byObject.set(callback, newid);
						this.byInteger.set(newid, callback);
						this.references.set(newid, 1);

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
						const references = this.references.get(address) || 0;

						if(references > 1)
						{
							this.references.set(address, references - 1);
							return;
						}

						const obj = this.byInteger.get(address);

						if(obj)
						{
							this.byObject.delete(obj);
							this.byInteger.delete(address);
							Module.tacked.delete(obj);
						}

						this.references.delete(address);
					}
				});

				Object.defineProperty(this, 'clear', {
					configurable: false
					, writable:   false
					, value: () => {
						this.byObject = new WeakMap();
						this.byInteger = new Module.WeakerMap();
						this.references.clear();
					}
				});
			}
		});

		Module.classes = new WeakMap();
		Module._classes = new Module.WeakerMap();
		Module._objects = new Module.WeakerMap();
		Module._arrays = new Module.WeakerMap();
		Module._callables = new Module.WeakerMap();

		Module.targets = new Module.UniqueIndex;

			Module.targets.add(globalThis);
		Module.PdoParams = new WeakMap;
	});

	return SUCCESS;
}

PHP_MINFO_FUNCTION(vrzno)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Vrzno support for php-wasm", "enabled");
	php_info_print_table_end();

	if (!sapi_module.phpinfo_as_text) {
		php_info_print_box_start(0);
		PUTS("Find <a target = \"_blank\" href=\"https://github.com/seanmorris/php-wasm\">php-wasm</a>");
		PUTS(" & <a target = \"_blank\" href=\"https://github.com/seanmorris/vrzno\">vrzno</a> on github!");
		PUTS("<a target = \"_blank\" href=\"https://github.com/seanmorris/php-wasm\">");
		PUTS("<img border=\"0\" src=\"");
		PUTS(VRZNO_ICON_DATA_URI "\" alt=\"Sean logo\" /></a>\n");
		php_info_print_box_end();
	}
}

zend_module_entry vrzno_module_entry = {
	STANDARD_MODULE_HEADER,
	"vrzno",
	ext_functions,             /* zend_function_entry */
	PHP_MINIT(vrzno),          /* PHP_MINIT - Module initialization */
	NULL,                      /* PHP_MSHUTDOWN - Module shutdown */
	NULL,                      /* PHP_RINIT - Request initialization */
	PHP_RSHUTDOWN(vrzno),      /* PHP_RSHUTDOWN - Request shutdown */
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

zval* EMSCRIPTEN_KEEPALIVE vrzno_exec_callback(zend_function *func, zval *argv, int argc, zend_object *zo)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	fci.size = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);

	fci.retval = emalloc(sizeof(zval));
	ZVAL_UNDEF(fci.retval);
	fci.params = argv;
	fci.named_params = 0;
	fci.param_count = argc;

	fcc.function_handler = func;
	fcc.calling_scope = NULL;
	fcc.called_scope = NULL;

	fci.object = NULL;
	fcc.object = NULL;

	if(zo)
	{
		fci.object = zo;
		fcc.object = zo;
	}

	if(zend_call_function(&fci, &fcc) == SUCCESS)
	{
		return fci.retval;
	}

	vrzno_expose_destroy_zval(fci.retval);
	return NULL;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_exec_zval_callback(zval *callback, zval *argv, int argc)
{
	zval *retval = emalloc(sizeof(zval));
	ZVAL_UNDEF(retval);

	if(call_user_function(EG(function_table), NULL, callback, retval, (uint32_t) argc, argv) == SUCCESS)
	{
		return retval;
	}

	vrzno_expose_destroy_zval(retval);
	return NULL;
}
