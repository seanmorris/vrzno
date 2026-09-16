/**
 * Starts a dynamic module import and returns its promise as a PHP bridge object. This
 * synchronous body does not await that promise. JavaScript errors are forwarded to PHP
 * through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_import
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
try
{
	const name = UTF8ToString(nameAddress);
	Module.jsToZval(import(name), resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
