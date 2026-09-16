/**
 * Assigns a UTF-8 property name, viewing ArrayBuffers as byte arrays. JavaScript errors
 * are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_key_write
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} valueAddress Address of a borrowed PHP value, valid for this call.
 * @returns {void}
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	target[UTF8ToString(nameAddress)] = Module.zvalToJS(valueAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
