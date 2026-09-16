/**
 * Selects the initial target when PHP constructs a Vrzno object.
 *
 * @function vrzno_js_object_create
 * @param {number} classAddress Wasm address of the PHP class entry.
 * @returns {number} Zero for a cached JS constructor class; otherwise a retained
 * globalThis target handle.
 */
const _class = Module._classes.get(classAddress);

if(_class)
{
	return 0;
}

return Module.targets.add(globalThis);
