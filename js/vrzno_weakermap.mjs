import {WeakerMap} from 'weakermap';

/**
 * Uses the npm weak-value cache while preserving Vrzno's public snapshot methods
 * and iterator shape. Make bundles this module inside vrzno_js_init(), whose
 * captured WeakRef and FinalizationRegistry constructors serve the package too.
 * PHP ownership and target reference counts remain separate from this cache.
 */
Module.WeakerMap = Module.WeakerMap || class extends WeakerMap {
	/**
	 * Walks live entries, pruning dead references through the package iterator.
	 *
	 * @returns {Iterator<Array<*>>} Iterator yielding [key, value] pairs.
	 */
	[Symbol.iterator]() {
		const iterator = super[Symbol.iterator]();
		return {next: () => iterator.next()};
	}

	/**
	 * Materializes the keys of currently live entries.
	 *
	 * @returns {Array<*>} Snapshot of keys.
	 */
	keys() {
		return Array.from(super.keys());
	}

	/**
	 * Materializes currently live values, retaining them in the returned array.
	 *
	 * @returns {Array<object|Function>} Snapshot of values.
	 */
	values() {
		return Array.from(super.values());
	}
};
