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

			// _free(keysLoc);

			keys.push(...Reflect.ownKeys(target));

			return keys;
		},
		has: (target, prop) => {
			if(typeof prop === 'symbol')
			{
				return false;
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

			return !!retPtr;
		},
		get: (target, prop, receiver) => {
			if(typeof prop === 'symbol')
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

			const proxy = Module.zvalToJS(retPtr);

			if(proxy && ['function','object'].includes(typeof proxy))
			{
				Module.zvalMap.set(proxy, retPtr);
			}

			_free(namePtr);

			return proxy ?? Reflect.get(target, prop);
		},
		getOwnPropertyDescriptor: (target, prop) => {
			if(typeof prop === 'symbol' || prop in target)
			{
				return Reflect.getOwnPropertyDescriptor(target, prop);
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

			const proxy = Module.zvalToJS(retPtr);

			if(proxy && ['function','object'].includes(typeof proxy))
			{
				Module.zvalMap.set(proxy, retPtr);
			}

			_free(namePtr);

			return {configurable: true, enumerable: true, value: target[prop]};

		},
	});

	if(proxy && ['function','object'].includes(typeof proxy))
	{
		Module.zvalMap.set(proxy, zvalPtr);
	}

	if(!Module.targets.has(proxy))
	{
		Module.targets.add(proxy);

		Module.ccall(
			'vrzno_expose_inc_zrefcount'
			, 'number'
			, ['number']
			, [zvalPtr]
		);

		Module.fRegistry.register(proxy, zvalPtr, proxy);
	}

	return proxy;
});
