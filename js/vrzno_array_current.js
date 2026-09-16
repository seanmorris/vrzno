/**
 * Copies the indexed JS value into the iterator's result zval, viewing ArrayBuffers as
 * bytes.
 *
 * @function vrzno_js_array_current
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} index Signed, zero-based numeric property or array index.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
let target = Module.targets.get(targetId);
const property = index;
const rv = resultAddress;

if(target instanceof ArrayBuffer)
{
	if(!Module.bufferMaps.has(target))
	{
		Module.bufferMaps.set(target, new Uint8Array(target));
	}

	target = Module.bufferMaps.get(target);
}

return Module.jsToZval(target[property], rv);
