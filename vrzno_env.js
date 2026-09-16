/**
 * Copies a named Emscripten Module option into PHP. JavaScript errors are forwarded to
 * PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_env
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
try
{
	const name = UTF8ToString(nameAddress);
	Module.jsToZval(Module[name], resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
