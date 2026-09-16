/**
 * Creates a frozen opaque wrapper that owns a copied PHP resource zval and can be
 * passed back into the same runtime generation.
 *
 * @param {WasmAddress} zr Native resource address, retained for call compatibility.
 * @param {WasmAddress} zv Source resource zval to copy.
 * @returns {object} Opaque resource wrapper.
 */
Module.resourceToJs = ((zr, zv) => {
	const generation = Module.vrznoGeneration;
	const ownedZval = Module.vrznoCopyZval(zv);
	const proxy = {};
	Object.defineProperties(proxy, {
		[origZval]: {value: ownedZval}
		, [proxyGeneration]: {value: generation}
	});
	Module.ownedZvalRegistry.register(proxy, ownedZval, proxy);
	Object.freeze(proxy);
	return proxy;
});

/**
 * Converts a borrowed zval to a JS value, resolving references and indirect values.
 * Object, array, resource, and callable wrappers acquire their own native owners;
 * scalar conversion does not consume the source.
 *
 * @param {WasmAddress} zv Borrowed zval, or zero for undefined.
 * @returns {*} Converted JS value or cached wrapper.
 */
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

	const isCallable = Module.ccall(
		'vrzno_expose_callable'
		, 'number'
		, ['number']
		, [zv]
	);

	if(isCallable && type !== IS_STRING)
	{
		// Keep distinct closures and callable arrays separate even when they
		// resolve to the same function. The wrapper keeps this identity alive.
		const identity = Module.ccall(
			type === IS_OBJECT ? 'vrzno_expose_object' : 'vrzno_expose_array'
			, 'number'
			, ['number']
			, [zv]
		);
		return Module.callableToJs(0, null, zv, `${type}:${identity}`);
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

/**
 * Converts a temporary owned zval and destroys that temporary in finally, including
 * when conversion fails. Returned wrappers retain separate copies.
 *
 * @param {WasmAddress} zv Owned temporary zval, or zero.
 * @returns {*} Converted JS value.
 */
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

/**
 * Initializes a caller-provided zval from JS. Existing same-generation PHP wrappers
 * are copied back; other objects/functions receive retained target handles. BigInt and
 * Symbol values raise PHP TypeError.
 *
 * @param {*} value Value to convert.
 * @param {WasmAddress} rv Writable caller-owned zval storage; its old value must already be released.
 * @returns {void}
 */
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
