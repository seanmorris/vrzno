/**
 * Stores both directions of the JS-constructor/PHP-class association.
 *
 * @function vrzno_js_class_register
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} classAddress Wasm address of the PHP class entry.
 * @returns {void}
 */
const target = Module.targets.get(targetId);
Module.classes.set(target, classAddress);
Module._classes.set(classAddress, target);
