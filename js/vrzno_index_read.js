/**
 * Reads a numeric property, viewing ArrayBuffers as byte arrays. JavaScript errors are
 * forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_index_read
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} index Signed, zero-based numeric property or array index.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	Module.jsToZval(target[index], resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
