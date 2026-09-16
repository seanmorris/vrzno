/**
 * Deletes a numeric property, viewing ArrayBuffers as byte arrays. JavaScript errors are
 * forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_index_unset
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} index Signed, zero-based numeric property or array index.
 * @returns {void}
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	delete target[index];
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
