/**
 * Evaluates the legacy vrzno_eval() input and stringifies its result. JavaScript errors
 * are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_eval
 * @param {number} sourceAddress Address of NUL-terminated UTF-8 JavaScript source.
 * @returns {number} Wasm address of allocated NUL-terminated UTF-8 text; C frees it with
 * free(). Zero indicates a forwarded PHP exception.
 */
try
{
	const str = String(eval(UTF8ToString(sourceAddress)));
	const len = lengthBytesUTF8(str) + 1;
	const loc = _malloc(len);

	stringToUTF8(str, loc, len);

	return loc;
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
	return 0;
}
