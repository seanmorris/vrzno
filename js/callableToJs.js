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
			paramPtrs.forEach((p,i) => {
				// console.log({p,i});
				// if(!args[i] || !['function','object'].includes(typeof args[i]))
				// {
				// 	console.log({arg:args[i], p, i});
				// 	Module.fRegistry.unregister(args[i]);
				// }
				// Module.ccall(
				// 	'vrzno_expose_dec_zrefcount'
				// 	, 'number'
				// 	, ['number']
				// 	, [p]
				// );
			});

			Module.ccall(
				'vrzno_expose_efree'
				, 'number'
				, ['number', 'number']
				, [paramsPtr, false]
			);
		}

		if(zvalPtr)
		{
			return Module.zvalToJS(zvalPtr);
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
