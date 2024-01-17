Module.WeakerMap = Module.WeakerMap || (class WeakerMap
{
	constructor(entries)
	{
		this.registry = new FinReg(held => this.delete(held));
		this.map = new Map;
		entries && entries.forEach(([key, value]) => this.set(key, value));
	}

	get size()
	{
		return this.map.size;
	}

	clear()
	{
		this.map.clear();
	}

	delete(key)
	{
		this.map.delete(key);
	}

	[Symbol.iterator]()
	{
		const mapIterator = this.map[Symbol.iterator]();

		return {
			next: () => {
				do
				{
					const entry = mapIterator.next();

					if(entry.done)
					{
						return {done:true};
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

	entries()
	{
		return {[Symbol.iterator]: () => this[Symbol.iterator]()};
	}

	forEach(callback)
	{
		for(const [k,v] of this)
		{
			callback(v, k, this);
		}
	}

	get(key)
	{
		if(!this.has(key))
		{
			return;
		}

		return this.map.get(key).deref();
	}

	has(key)
	{
		if(!this.map.has(key))
		{
			return false;
		}

		const result = this.map.get(key).deref();

		if(!result)
		{
			this.map.delete(key);
		}

		return result;
	}

	keys()
	{
		return [...this].map(v => v[0]);
	}

	set(key, value)
	{
		if(typeof value !== 'function' && typeof value !== 'object')
		{
			throw new Error('WeakerMap values must be objects.');
		}

		if(this.map.has(key))
		{
			this.registry.unregister(this.get(key));
		}

		this.registry.register(value, key, value);

		return this.map.set(key, new wRef(value));
	}

	values()
	{
		return [...this].map(v => v[1]);
	}
});
