/**
 * Looks up the cached PHP class entry for a JS constructor.
 *
 * @function vrzno_js_class_lookup
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @returns {number|undefined} Native class address, or undefined, which Wasm converts to
 * NULL.
 */
const target = Module.targets.get(targetId);
return Module.classes.get(target);
