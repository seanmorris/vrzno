/**
 * Reads a UTF-8 property name, viewing ArrayBuffers as byte arrays. JavaScript errors
 * are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_key_read
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	Module.jsToZval(target[UTF8ToString(nameAddress)], resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
