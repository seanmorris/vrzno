/**
 * Checks a named offset for PHP isset()/empty() semantics. JavaScript errors are
 * forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_key_has
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} checkEmpty Nonzero selects PHP truthiness; zero selects
 * non-null/non-undefined membership.
 * @returns {boolean|number} Truth value, coerced to a C integer; zero after a forwarded
 * exception.
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	const value = target[UTF8ToString(nameAddress)];
	return checkEmpty ? Module.vrznoPhpTruthy(value) : value !== null && value !== undefined;
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
	return 0;
}
