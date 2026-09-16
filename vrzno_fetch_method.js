/**
 * Sets the request method from a PHP stream-context string.
 *
 * @function vrzno_js_fetch_method
 * @param {number} contextId Handle of the retained fetch-options object.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @returns {void}
 */
{
	const context = Module.targets.get(contextId);
	const method = UTF8ToString(nameAddress);
	context.method = method;
}
