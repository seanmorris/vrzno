/**
 * Releases a nonzero target handle when its PHP object is destroyed.
 *
 * @function vrzno_js_object_release
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @returns {void}
 */
if(targetId)
{
	Module.targets.remove(targetId);
}
