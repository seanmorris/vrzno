/**
 * Enumerable cache with strong keys and weak object/function values. Uses the
 * initialization scope's finalization and reference fallbacks. This cache does not own
 * or destroy PHP values; ownedZvalRegistry handles that separately.
 */
Module.WeakerMap = Module.WeakerMap || (class WeakerMap {
	/**
	 * Creates the backing map and a generation-specific finalization registry.
	 *
	 * @param {Array<Array<*>>} [entries] Initial [key, object-or-function] pairs.
	 */
	constructor(entries) {
		this.map = new Map;
		this.registry = getRegistry(this);
		entries && entries.forEach(([key, value]) => this.set(key, value));
	}

	/**
	 * Returns the backing entry count before opportunistic dead-reference cleanup.
	 *
	 * @returns {number} Stored entry count.
	 */
	get size() {
		return this.map.size;
	}

	/**
	 * Drops all entries and replaces the registry so queued callbacks from the old
	 * registry cannot remove new entries.
	 *
	 * @returns {void}
	 */
	clear() {
		this.registry = getRegistry(this);
		this.map.clear();
	}

	/**
	 * Removes a live entry and unregisters its finalizer; has() also clears a dead
	 * entry encountered here.
	 *
	 * @param {*} key Entry key.
	 * @returns {void}
	 */
	delete(key) {
		if(!this.has(key))
		{
			return;
		}

		this.registry.unregister(this.get(key));
		this.map.delete(key);
	}

	/**
	 * Walks live entries and removes dead references encountered along the way.
	 *
	 * @returns {Iterator<Array<*>>} Iterator yielding [key, value] pairs.
	 */
	[Symbol.iterator]() {
		const mapIterator = this.map[Symbol.iterator]();
		return {
			next: () => {
				do
				{
					const entry = mapIterator.next();

					if(entry.done)
					{
						return {done: true};
					}

					const [key, ref] = entry.value;

					const value = ref.deref();

					if(!value)
					{
						this.map.delete(key);
						continue;
					}

					return {done: false, value: [key, value]};

				} while(true);
			}
		};
	}

	/**
	 * Exposes live entries to for-of and spread operations.
	 *
	 * @returns {Iterable<Array<*>>} Iterable of [key, value] pairs.
	 */
	entries() {
		return {[Symbol.iterator]: () => this[Symbol.iterator]()};
	}

	/**
	 * Visits live entries in map insertion order.
	 *
	 * @param {Function} callback Invoked with (value, key, this cache).
	 * @returns {void}
	 */
	forEach(callback) {
		for(const [k,v] of this)
		{
			callback(v, k, this);
		}
	}

	/**
	 * Retrieves a live value and removes a dead entry if encountered.
	 *
	 * @param {*} key Entry key.
	 * @returns {object|Function|undefined} Live value or undefined.
	 */
	get(key) {
		if(!this.has(key))
		{
			return;
		}

		const value = this.map.get(key).deref();

		if(!value)
		{
			this.map.delete(key);
		}

		return value;
	}

	/**
	 * Checks a reference and removes its entry when the value is gone.
	 *
	 * @param {*} key Entry key.
	 * @returns {boolean} Whether the value is live.
	 */
	has(key) {
		if(!this.map.has(key))
		{
			return false;
		}

		const result = this.map.get(key).deref();

		if(!result)
		{
			this.map.delete(key);
		}

		return Boolean(result);
	}

	/**
	 * Materializes the keys of currently live entries.
	 *
	 * @returns {Array<*>} Snapshot of keys.
	 */
	keys() {
		return [...this].map(v => v[0]);
	}

	/**
	 * Registers an object or function under a strong key, replacing any previous
	 * entry.
	 *
	 * @param {*} key Entry key.
	 * @param {object|Function} value Non-null referenceable value.
	 * @returns {Map<*, VrznoReference>} The backing map of references.
	 * @throws {Error} Invalid values or errors from the native weak-reference APIs.
	 */
	set(key, value) {
		if(typeof value !== 'function' && typeof value !== 'object')
		{
			throw new Error('WeakerMap values must be objects.');
		}

		if(this.has(key))
		{
			this.registry.unregister(this.get(key));
		}

		this.registry.register(value, key, value);

		return this.map.set(key, new wRef(value));
	}

	/**
	 * Materializes currently live values, retaining them in the returned array.
	 *
	 * @returns {Array<object|Function>} Snapshot of values.
	 */
	values() {
		return [...this].map(v => v[1]);
	}
});

/**
 * Creates cleanup for a particular cache generation. A late finalizer leaves a
 * replacement live entry or a newer registry untouched.
 *
 * @param {object} weakerMap Cache whose entries are being finalized.
 * @returns {object} Native registry or the no-op fallback.
 */
const getRegistry = weakerMap => {
	const registry = new _FinalizationRegistry(key => {
		if(weakerMap.registry !== registry)
		{
			return;
		}

		if(weakerMap.map.has(key) && weakerMap.map.get(key).deref())
		{
			return;
		}

		weakerMap.delete(key);
	});

	return registry;
};
