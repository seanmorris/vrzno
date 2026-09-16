/**
 * Deletes a named JS property. JavaScript errors are forwarded to PHP through
 * vrznoThrowRuntimeError().
 *
 * @function vrzno_js_property_unset
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @returns {void}
 */
(() =>{
	try
	{
		const target = Module.targets.get(targetId);
		const property = UTF8ToString(nameAddress);
		delete target[property];
	}
	catch(error)
	{
		Module.vrznoThrowRuntimeError(error);
	}
})();
