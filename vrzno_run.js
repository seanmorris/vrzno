/**
 * Calls a named global JS function with JSON-decoded arguments and stringifies its
 * result. JavaScript errors are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_run
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} jsonAddress Address of the NUL-terminated UTF-8 JSON argument array.
 * @returns {number} Wasm address of allocated NUL-terminated UTF-8 text; C frees it with
 * free(). Zero indicates a forwarded PHP exception.
 */

const funcName = UTF8ToString(nameAddress);
const argJson  = UTF8ToString(jsonAddress);

try
{
	const func = globalThis[funcName];
	if(typeof func !== 'function')
	{
		throw new TypeError(`${funcName} is not a global JavaScript function`);
	}
	const args = JSON.parse(argJson || '[]') || [];

	const str = String(func(...args));
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
