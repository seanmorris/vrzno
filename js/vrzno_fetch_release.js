/**
 * Releases a fetch response target reference and its strong retention when the final
 * reference is removed.
 *
 * @function vrzno_js_fetch_release
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @returns {void}
 */
Module.targets.remove(targetId);
