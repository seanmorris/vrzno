Module.zvalToJS = Module.zvalToJS || (zvalPtr => {

	if(Module._zvalMap.has(zvalPtr))
	{
		return Module._zvalMap.get(zvalPtr);
	}

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
		const nativeTarget = Module.ccall(
			'vrzno_expose_zval_is_target'
			, 'number'
			, ['number']
			, [zvalPtr]
		);

		if(nativeTarget)
		{
			return Module.targets.get(nativeTarget);
		}

		const wrapped = nativeTarget
			? Module.targets.get(nativeTarget)
			: Module.callableToJs(callable);

		if(!Module.targets.has(wrapped))
		{
			Module.targets.add(wrapped);
			Module.ccall(
				'vrzno_expose_inc_zrefcount'
				, 'number'
				, ['number']
				, [zvalPtr]
			);
		}

		Module.zvalMap.set(wrapped, zvalPtr);
		Module._zvalMap.set(zvalPtr, wrapped);
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

		// case IS_ARRAY:
		// 	break;

		case IS_OBJECT:
			const proxy = Module.marshalObject(zvalPtr);
			return proxy;
			break;

		default:
			return null;
			break;
	}
});
