/**
 * Owns copied PHP zvals until their JS wrappers are collected, explicitly released, or
 * invalidated by request shutdown. Separate unregister tokens prevent callback-cache
 * finalization from releasing unrelated PHP owners.
 */
const ownedZvalRegistryWrapper = class {
	/**
	 * Creates weak unregister-token lookup, an enumerable owner list, and
	 * allocation/release counters.
	 */
	constructor() {
		this.unregisterTokens = new WeakMap;
		this.entries = new Map;
		this.allocations = 0;
		this.releases = 0;
		this.registry = new FinalizationRegistry(zv => {
			if(!this.entries.has(zv))
			{
				return;
			}

			this.entries.delete(zv);
			this.destroy(zv);
		});
	}

	/**
	 * Calls the native zval destructor and records one release. Callers must remove
	 * the ownership record before invoking this helper.
	 *
	 * @param {WasmAddress} zv Owned zval to destroy.
	 * @returns {void}
	 */
	destroy(zv) {
		Module.ccall(
			'vrzno_expose_destroy_zval'
			, null
			, ['number']
			, [zv]
		);
		this.releases++;
	}

	/**
	 * Takes ownership of a copied zval. If the token already owns a value, the new
	 * copy is destroyed immediately and the original ownership is retained.
	 *
	 * @param {object|Function} target JS wrapper whose collection should release the copy.
	 * @param {WasmAddress} zv Newly owned zval.
	 * @param {object|Function} [unregisterToken=target] Weak identity for explicit release.
	 * @returns {void}
	 */
	register(target, zv, unregisterToken = target) {
		this.allocations++;

		if(this.unregisterTokens.has(unregisterToken))
		{
			this.destroy(zv);
			return;
		}

		const registryToken = {};
		this.registry.register(target, zv, registryToken);
		this.unregisterTokens.set(unregisterToken, {zv, registryToken});
		this.entries.set(zv, registryToken);
	}

	/**
	 * Unregisters and destroys an owner exactly once, also cancelling its queued
	 * finalizer.
	 *
	 * @param {object|Function} unregisterToken Identity passed to register().
	 * @returns {boolean} True when an owner was released.
	 */
	release(unregisterToken) {
		if(!this.unregisterTokens.has(unregisterToken))
		{
			return false;
		}

		const entry = this.unregisterTokens.get(unregisterToken);
		const zv = entry.zv;
		const registryToken = entry.registryToken;
		this.registry.unregister(registryToken);
		this.unregisterTokens.delete(unregisterToken);
		this.entries.delete(zv);
		this.destroy(zv);
		return true;
	}

	/**
	 * Cancels all current finalizers and destroys their zvals before PHP request
	 * memory is reclaimed, regardless of whether garbage collection has run.
	 *
	 * @returns {void}
	 */
	releaseAll() {
		for(const [zv, registryToken] of this.entries)
		{
			this.registry.unregister(registryToken);
			this.destroy(zv);
		}

		this.entries.clear();
		this.unregisterTokens = new WeakMap;
	}

	/**
	 * Reports the number of owned zvals that have not yet been released.
	 *
	 * @returns {number} Allocation count minus release count.
	 */
	get outstanding() {
		return this.allocations - this.releases;
	}
};

Module.ownedZvalRegistry = new ownedZvalRegistryWrapper;
/**
 * Copies a borrowed zval and retains its underlying PHP value.
 *
 * @param {WasmAddress} zv Borrowed source zval.
 * @returns {WasmAddress} Owned copy that must be destroyed or registered.
 */
Module.vrznoCopyZval = zv => Module.ccall(
	'vrzno_expose_copy_zval'
	, 'number'
	, ['number']
	, [zv]
);
/**
 * Destroys and frees an owned native zval.
 *
 * @param {WasmAddress} zv Owned zval address, or zero.
 * @returns {void}
 */
Module.vrznoDestroyZval = zv => Module.ccall(
	'vrzno_expose_destroy_zval'
	, null
	, ['number']
	, [zv]
);
/**
 * Rejects a proxy created by an earlier PHP request before it can access stale native
 * memory.
 *
 * @param {number} generation Generation captured when the proxy was created.
 * @returns {void}
 * @throws {ReferenceError} The owning runtime generation has ended.
 */
Module.vrznoAssertGeneration = generation => {
	if(generation !== Module.vrznoGeneration)
	{
		throw new ReferenceError('Vrzno value belongs to a previous PHP runtime.');
	}
};
/**
 * Converts a caught JS value into a PHP runtime exception. The temporary UTF-8 message
 * is freed after the native bridge copies it.
 *
 * @param {*} error Caught value; message is used when available.
 * @returns {void}
 */
Module.vrznoThrowRuntimeError = error => {
	const message = error && error.message ? error.message : String(error);
	const len = lengthBytesUTF8(message) + 1;
	const loc = _malloc(len);
	stringToUTF8(message, loc, len);
	Module.ccall('vrzno_expose_runtime_error', null, ['number'], [loc]);
	_free(loc);
};
/**
 * Returns a snapshot for lifecycle diagnostics without retaining any JS wrappers.
 *
 * @returns {VrznoOwnershipStats} Ownership and target counts.
 */
Module.vrznoOwnershipStats = () => ({
	allocations: Module.ownedZvalRegistry.allocations
	, releases: Module.ownedZvalRegistry.releases
	, outstanding: Module.ownedZvalRegistry.outstanding
	, targets: Module.targets ? Module.targets.references.size : 0
});

Module.bufferMaps = new WeakMap;
