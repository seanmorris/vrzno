/**
 * Checks the current numeric position of a JS array, typed array, or ArrayBuffer view.
 *
 * @function vrzno_js_array_valid
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} index Signed, zero-based numeric property or array index.
 * @returns {number} One for an in-range position; zero otherwise.
 */
let target = Module.targets.get(targetId);
const property = index;

if(target instanceof ArrayBuffer)
{
	if(!Module.bufferMaps.has(target))
	{
		Module.bufferMaps.set(target, new Uint8Array(target));
	}

	target = Module.bufferMaps.get(target);
}

if(Array.isArray(target) || ArrayBuffer.isView(target))
{
	if(property >=0 && property < target.length)
	{
		return 1;
	}
}

return 0;
