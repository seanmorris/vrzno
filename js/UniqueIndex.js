Module.UniqueIndex = Module.UniqueIndex || (class UniqueIndex
{
	constructor()
	{
		this.byObject  = new WeakMap();
		this.byInteger = new Module.WeakerMap();
		// this.tacked    = new Map();

		this.id = 0;

		Object.defineProperty(this, 'clear', {
			configurable: false
			, writable:   false
			, value: () => {
				this.byInteger.clear();
			}
		});

		Object.defineProperty(this, 'add', {
			configurable: false
			, writable:   false
			, value: (callback) => {

				if(this.byObject.has(callback))
				{
					const id = this.byObject.get(callback);

					return id;
				}

				const newid = ++this.id;

				this.byObject.set(callback, newid);
				this.byInteger.set(newid, callback);

				return newid;
			}
		});

		Object.defineProperty(this, 'has', {
			configurable: false
			, writable:   false
			, value: (obj) => {
				if(this.byObject.has(obj))
				{
					return this.byObject.get(obj);
				}
			}
		});

		Object.defineProperty(this, 'hasId', {
			configurable: false
			, writable:   false
			, value: (address) => {
				if(this.byInteger.has(address))
				{
					return this.byInteger.get(address);
				}
			}
		});

		Object.defineProperty(this, 'get', {
			configurable: false
			, writable:   false
			, value: (address) => {
				if(this.byInteger.has(address))
				{
					return this.byInteger.get(address);
				}
			}
		});

		Object.defineProperty(this, 'getId', {
			configurable: false
			, writable:   false
			, value: (obj) => {
				if(this.byObject.has(obj))
				{
					return this.byObject.get(obj);
				}
			}
		});

		Object.defineProperty(this, 'remove', {
			configurable: false
			, writable:   false
			, value: (address) => {

				const obj = this.byInteger.get(address);

				if(obj)
				{
					this.byObject.delete(obj);
					this.byInteger.delete(address);
				}
			}
		});

		// Object.defineProperty(this, 'tack', {
		// 	configurable: false
		// 	, writable:   false
		// 	, value:      (target) => {
		// 		if(!this.tacked.has(target))
		// 		{
		// 			this.tacked.set(target, 1);
		// 		}
		// 		else
		// 		{
		// 			this.tacked.set(target, 1 + this.tacked.get(target));
		// 		}
		// 	}
		// });

		// Object.defineProperty(this, 'untack', {
		// 	configurable: false
		// 	, writable:   false
		// 	, value:      (target) => {
		// 		if(!this.tacked.has(target))
		// 		{
		// 			return;
		// 		}

		// 		this.tacked.set(target, -1 + this.tacked.get(target));

		// 		if(this.tacked.get(target) <= 0)
		// 		{
		// 			return this.tacked.delete(target);
		// 		}
		// 	}
		// });
	}
});
