/**
 * Assigns a numeric property, viewing ArrayBuffers as byte arrays. JavaScript errors are
 * forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_index_write
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} index Signed, zero-based numeric property or array index.
 * @param {number} valueAddress Address of a borrowed PHP value, valid for this call.
 * @returns {void}
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	target[index] = Module.zvalToJS(valueAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
