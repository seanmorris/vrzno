/**
 * Resolves a PHP method to a cached JS callback. Magic methods cache the requested
 * spelling after Zend's temporary lookup trampoline has been released.
 *
 * @param {WasmAddress} zo Borrowed zend_object address.
 * @param {string} method Requested PHP method name.
 * @returns {Function|undefined} Callback retaining its PHP owner, or undefined.
 */
Module.methodToJs = (zo, method) => {
	const length = lengthBytesUTF8(method);
	const namePtr = _malloc(length + 1);
	const magicPtr = _malloc(4);
	let callable = 0;
	try
	{
		stringToUTF8(method, namePtr, length + 1);
		const funcPtr = Module.ccall(
			'vrzno_expose_method_pointer', 'number',
			['number', 'number', 'number', 'number'],
			[zo, namePtr, length, magicPtr]
		);
		if(!getValue(magicPtr, 'i32'))
		{
			return funcPtr ? Module.callableToJs(funcPtr, zo) : undefined;
		}

		// Zend's lookup trampoline has already been released. Cache by
		// the requested spelling, which is observable inside __call.
		const cacheKey = `magic:${zo}:${method}`;
		const cached = Module._callables.get(`${Module.vrznoGeneration}:${cacheKey}`);
		if(cached)
		{
			return cached;
		}
		callable = Module.ccall(
			'vrzno_expose_method_callable', 'number',
			['number', 'number', 'number'], [zo, namePtr, length]
		);
		// callableToJs copies the specification; the temporary is ours.
		return Module.callableToJs(0, null, callable, cacheKey);
	}
	finally
	{
		if(callable)
		{
			Module.vrznoDestroyZval(callable);
		}
		_free(magicPtr);
		_free(namePtr);
	}
};

/**
 * Returns a cached proxy for a PHP object, or its original JS target when already
 * bridged. A new proxy owns a copied zval and checks its request generation before
 * native access. Missing properties become undefined; explicit PHP null and
 * values returned by magic getters retain their PHP meaning.
 *
 * @param {WasmAddress} zo Borrowed zend_object address.
 * @param {WasmAddress} [zv=0] Source zval to copy, or zero to copy the object directly.
 * @returns {object|Function} Cached proxy or original bridged target.
 */
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
		}

		, has: (target, prop) => {
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
		}

		, get: (target, prop) => {
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
				return Module.methodToJs(zo, '__toString');
			}

			prop = String(prop);
			const method = Module.methodToJs(zo, prop);
			if(method)
			{
				return method;
			}
			const len = lengthBytesUTF8(prop) + 1;
			const loc = _malloc(len);
			stringToUTF8(prop, loc, len);

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
		}

		, getOwnPropertyDescriptor: (target, prop) => {
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
		}
	});

	Object.defineProperties(proxy, {
		[origZval]: {value: ownedZval}
		, [proxyGeneration]: {value: generation}
	});
	Module.ownedZvalRegistry.register(proxy, ownedZval, proxy);
	Module._objects.set(zo, proxy);

	return proxy;
});
