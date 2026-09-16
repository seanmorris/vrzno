/**
 * Schedules a PHP callback, rejects callbacks from an older runtime generation, and
 * releases the owned callback after the attempt.
 *
 * @function vrzno_js_timeout
 * @param {number} milliseconds Nonnegative delay in milliseconds, already validated by
 * C.
 * @param {number} callbackAddress Owned callback zval; ownership transfers to the
 * registry until timeout or shutdown.
 * @returns {void}
 */
const timeout = milliseconds;
const ownedCallback = callbackAddress;
const generation = Module.vrznoGeneration;
const token = {};
Module.ownedZvalRegistry.register(token, ownedCallback, token);

setTimeout(()=>{
	try
	{
		if(generation !== Module.vrznoGeneration)
		{
			return;
		}

		const zv = Module.ccall(
			'vrzno_exec_zval_callback'
			, 'number'
			, ['number','number','number']
			, [ownedCallback, 0, 0]
		);

		Module.vrznoDestroyZval(zv);
	}
	finally
	{
		Module.ownedZvalRegistry.release(token);
	}
}, timeout);
