/**
 * Reads a JS property into a caller-owned PHP result. JavaScript errors are forwarded to
 * PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_property_read
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
try
{
	const target = Module.targets.get(targetId);
	const property = UTF8ToString(nameAddress);
	Module.jsToZval(target[property], resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
