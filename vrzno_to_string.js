/**
 * Converts a target with JavaScript String(). JavaScript errors are forwarded to PHP
 * through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_to_string
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @returns {number} Wasm address of allocated NUL-terminated UTF-8 text; C frees it with
 * free(). Zero indicates a forwarded PHP exception.
 */
try
{
	const target = Module.targets.get(targetId);
	const str = String(target);
	const len = 1 + lengthBytesUTF8(str);
	const loc = _malloc(len);

	stringToUTF8(str, loc, len);

	return loc;
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
	return 0;
}
