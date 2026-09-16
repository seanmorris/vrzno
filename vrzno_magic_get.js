/**
 * Implements an explicit Vrzno::__get() call by converting a named JS property.
 * JavaScript errors are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_magic_get
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
	const propertyName = UTF8ToString(nameAddress);
	Module.jsToZval(target[propertyName], resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
