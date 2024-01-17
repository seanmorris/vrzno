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
				'vrzno_expose_inc_zrefcount'
				, 'number'
				, ['number']
				, [zvalPtr]
			);

			Module.fRegistry.register(value, zvalPtr, value);
		}

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
	}

	return zvalPtr;
});
