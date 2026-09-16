/**
 * Releases owned PHP values before request shutdown, invalidates old proxies, and resets
 * caches and target references. The global target is registered again for the next
 * request.
 *
 * @function vrzno_js_shutdown
 * @returns {void}
 */
if(Module.ownedZvalRegistry)
{
	Module.ownedZvalRegistry.releaseAll();
}

Module.vrznoGeneration++;
Module.tacked.clear();
Module.classes = new WeakMap();
Module._classes = new Module.WeakerMap();
Module._objects = new Module.WeakerMap();
Module._arrays = new Module.WeakerMap();
Module._callables = new Module.WeakerMap();
Module.targets.clear();
Module.targets.add(globalThis);
