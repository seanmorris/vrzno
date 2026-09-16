/**
 * Maintains stable numeric handles for JS objects and functions with explicit PHP-side
 * reference counts. Weak lookup and Module.tacked strong retention serve different
 * purposes; removing the final reference releases both associations.
 */
Module.UniqueIndex = Module.UniqueIndex || (class UniqueIndex {
	constructor() {
		this.byObject = new WeakMap();
		this.byInteger = new Module.WeakerMap();
		this.references = new Map();
		this.id = 0;

		/**
		 * Retains one target reference, reusing the handle for an already registered
		 * object.
		 *
		 * @function UniqueIndex#add
		 * @param {object|Function} callback Target to register.
		 * @returns {VrznoTargetId} Stable nonzero handle.
		 */
		Object.defineProperty(this, 'add', {
			configurable: false
			, writable:   false
			, value:      (callback) => {

				if(this.byObject.has(callback))
				{
					const id = this.byObject.get(callback);
					this.references.set(id, 1 + (this.references.get(id) || 0));

					return id;
				}

				const newid = ++this.id;

				this.byObject.set(callback, newid);
				this.byInteger.set(newid, callback);
				this.references.set(newid, 1);

				return newid;
			}
		});

		/**
		 * Looks up the handle associated with a JS target.
		 *
		 * @function UniqueIndex#has
		 * @param {object|Function} obj Target identity.
		 * @returns {VrznoTargetId|undefined} Existing handle or undefined.
		 */
		Object.defineProperty(this, 'has', {
			configurable: false
			, writable:   false
			, value:      (obj) => {
				if(this.byObject.has(obj))
				{
					return this.byObject.get(obj);
				}
			}
		});

		/**
		 * Looks up the live target associated with a handle.
		 *
		 * @function UniqueIndex#hasId
		 * @param {VrznoTargetId} address Target handle, despite the historical parameter name.
		 * @returns {object|Function|undefined} Live target or undefined.
		 */
		Object.defineProperty(this, 'hasId', {
			configurable: false
			, writable:   false
			, value:      (address) => {
				if(this.byInteger.has(address))
				{
					return this.byInteger.get(address);
				}
			}
		});

		/**
		 * Retrieves a live target by handle.
		 *
		 * @function UniqueIndex#get
		 * @param {VrznoTargetId} address Target handle.
		 * @returns {object|Function|undefined} Live target or undefined.
		 */
		Object.defineProperty(this, 'get', {
			configurable: false
			, writable:   false
			, value:      (address) => {
				if(this.byInteger.has(address))
				{
					return this.byInteger.get(address);
				}
			}
		});

		/**
		 * Retrieves a handle without incrementing its reference count.
		 *
		 * @function UniqueIndex#getId
		 * @param {object|Function} obj Target identity.
		 * @returns {VrznoTargetId|undefined} Existing handle or undefined.
		 */
		Object.defineProperty(this, 'getId', {
			configurable: false
			, writable:   false
			, value:      (obj) => {
				if(this.byObject.has(obj))
				{
					return this.byObject.get(obj);
				}
			}
		});

		/**
		 * Releases one reference; the final release removes both indexes and strong
		 * retention.
		 *
		 * @function UniqueIndex#remove
		 * @param {VrznoTargetId} address Target handle to release.
		 * @returns {void}
		 */
		Object.defineProperty(this, 'remove', {
			configurable: false
			, writable:   false
			, value:      (address) => {
				const references = this.references.get(address) || 0;

				if(references > 1)
				{
					this.references.set(address, references - 1);
					return;
				}

				const obj = this.byInteger.get(address);

				if(obj)
				{
					this.byObject.delete(obj);
					this.byInteger.delete(address);
					Module.tacked.delete(obj);
				}

				this.references.delete(address);
			}
		});

		/**
		 * Replaces lookup indexes and clears counts while preserving the monotonically
		 * increasing ID counter. Request shutdown clears Module.tacked separately.
		 *
		 * @function UniqueIndex#clear
		 * @returns {void}
		 */
		Object.defineProperty(this, 'clear', {
			configurable: false
			, writable:   false
			, value:      () => {
				this.byObject = new WeakMap();
				this.byInteger = new Module.WeakerMap();
				this.references.clear();
			}
		});
	}
});

Module.classes = new WeakMap();
Module._classes = new Module.WeakerMap();
Module._objects = new Module.WeakerMap();
Module._arrays = new Module.WeakerMap();
Module._callables = new Module.WeakerMap();

Module.targets = new Module.UniqueIndex;

Module.targets.add(globalThis);
Module.PdoParams = new WeakMap;
