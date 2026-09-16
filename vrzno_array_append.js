/**
 * Assigns the converted PHP value at the JS target's current length. JavaScript errors
 * are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_array_append
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} valueAddress Address of a borrowed PHP value, valid for this call.
 * @returns {void}
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	target[target.length] = Module.zvalToJS(valueAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
