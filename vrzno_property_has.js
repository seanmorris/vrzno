/**
 * Checks a JS property using PHP existence, nullness, or truthiness semantics.
 * JavaScript errors are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_property_has
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} checkMode Zend property-check mode: 0 isset, 1 nonempty, or 2
 * existence.
 * @returns {boolean} Boolean result, coerced to a C integer.
 */
try
{
	const target = Module.targets.get(targetId);
	const property = UTF8ToString(nameAddress);
	const mode = checkMode;

	if(!Reflect.has(target, property))
	{
		return false;
	}

	if(mode === 2)
	{
		return true;
	}

	const value = target[property];
	return mode === 1
		? Module.vrznoPhpTruthy(value)
		: value !== null && value !== undefined;
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
	return false;
}
