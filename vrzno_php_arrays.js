/**
 * Returns a cached PHP-array proxy owning a copy of its source zval. Detached iterator
 * factories and each iterator obtain separate copies so their lifetimes do not depend
 * on the proxy or one another.
 *
 * @param {WasmAddress} za Borrowed zend_array address.
 * @param {WasmAddress} zv Source zval retained by a new proxy.
 * @returns {object} Array-like proxy with PHP key normalization and iteration.
 */
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
		}
		, has: (target, prop) => {
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
		}
		, get: (target, prop) => {
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

							return {
								done
								, value: Module.zvalToJS(Module.ccall(
									'vrzno_expose_array_value_at'
									, 'number'
									, ['number', 'number']
									, [za, current]
								))
							};
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
		}
		, getOwnPropertyDescriptor: (target, prop) => {
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
					value
					, writable: false
					, enumerable: false
					, configurable: true
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
		}
	});

	Object.defineProperties(proxy, {
		[origZval]: {value: ownedZval}
		, [proxyGeneration]: {value: generation}
	});
	Module.ownedZvalRegistry.register(proxy, ownedZval, proxy);
	Module._arrays.set(za, proxy);

	return proxy;
});
