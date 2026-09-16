/**
 * Caches a JS wrapper by runtime generation and callable identity. A new wrapper
 * retains its PHP callable or receiver; cache hits acquire no extra owner. Each call
 * converts arguments and releases temporary parameter/result zvals.
 *
 * @param {WasmAddress} funcPtr Native zend_function pointer for a direct method.
 * @param {WasmAddress|null} [zo=null] Receiver object for a direct method.
 * @param {WasmAddress} [zv=0] Callable specification copied instead of the receiver.
 * @param {string} [cacheKey] Identity distinguishing receiver, callable, or magic spelling.
 * @returns {Function} Wrapper valid only during its owning PHP generation.
 */
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
				[origZval]: {value: callableZval}
				, [proxyGeneration]: {value: generation}
			});
		}
		Module.ownedZvalRegistry.register(wrapped, ownedZval, wrapped);
	}

	Module._callables.set(cacheKey, wrapped);
	return wrapped;
});
