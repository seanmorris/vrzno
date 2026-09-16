/**
 * Checks an indexed value for PHP isset()/empty() semantics. JavaScript errors are
 * forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_index_has
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} index Signed, zero-based numeric property or array index.
 * @param {number} checkEmpty Nonzero selects PHP truthiness; zero selects
 * non-null/non-undefined membership.
 * @returns {boolean|number} Truth value, coerced to a C integer; zero after a forwarded
 * exception.
 */
try
{
	const target = Module.vrznoArrayView(Module.targets.get(targetId));
	const value = target[index];
	return checkEmpty ? Module.vrznoPhpTruthy(value) : value !== null && value !== undefined;
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
	return 0;
}
