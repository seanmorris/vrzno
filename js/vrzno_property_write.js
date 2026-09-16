/**
 * Assigns a PHP value to a JS property, or deletes it for IS_UNDEF. JavaScript errors
 * are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_property_write
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} valueAddress Address of a borrowed PHP value, valid for this call.
 * @param {number} valueType Zend value tag; IS_UNDEF (zero) requests property deletion.
 * @returns {void}
 */
try
{
	const target = Module.targets.get(targetId);
	const property = UTF8ToString(nameAddress);

	if(valueType === 0)
	{
		delete target[property];
		return;
	}

	target[property] = Module.zvalToJS(valueAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
