import assert from 'node:assert/strict';
import {test} from 'node:test';
import {WeakerMap} from 'weakermap';
import {bridge} from './lib/js-bridge.mjs';

// Deterministic finalization lets the compatibility audit exercise late callbacks
// and unregister-token behavior without depending on GC scheduling.
function collector()
{
	const references = [], registries = [], pending = [];
	class Reference
	{
		constructor(value) { this.value = value; references.push(this); }
		deref() { return this.value; }
	}
	class Registry
	{
		constructor(callback) { this.callback = callback; this.entries = new Set(); registries.push(this); }
		register(target, held, token) { this.entries.add({target, held, token}); }
		unregister(token)
		{
			let removed = false;
			for(const entry of this.entries)
			{
				if(entry.token === token)
				{ this.entries.delete(entry); removed = true; }
			}
			return removed;
		}
	}
	return {
		WeakRef: Reference, FinalizationRegistry: Registry
		, queue(target) {
			for(const ref of references) if(ref.value === target) ref.value = undefined;
			for(const registry of registries)
				for(const entry of registry.entries)
					if(entry.target === target) pending.push({registry, entry});
		}
		, flush() {
			for(const {registry, entry} of pending.splice(0))
				if(registry.entries.delete(entry)) registry.callback(entry.held);
		}
	};
}

function withApis(apis, callback)
{
	const previous = {WeakRef: globalThis.WeakRef, FinalizationRegistry: globalThis.FinalizationRegistry};
	try
	{
		for(const name of ['WeakRef', 'FinalizationRegistry'])
			if(Object.hasOwn(apis, name)) globalThis[name] = apis[name];
		return callback();
	}
	finally
	{
		Object.assign(globalThis, previous);
	}
}

for(const source of ['bundled adapter', 'npm'])
{
	test(`${source}: primitive keys, object/function values, and live replacement satisfy Vrzno's cache operations`, () => {
		const Cache = source === 'npm' ? WeakerMap : bridge().module.WeakerMap;
		const object = {}, callback = () => 42, next = {};
		const cache = new Cache([[1, object]]);
		assert.equal(cache.get(1), object);
		cache.set('callback', callback);
		assert.equal(cache.get('callback'), callback);
		assert.deepEqual(Array.from(cache, pair => Array.from(pair)), [[1, object], ['callback', callback]]);
		cache.set(1, next);
		assert.equal(cache.get(1), next);
		cache.delete('callback');
		assert.equal(cache.has('callback'), false);
		assert.throws(() => cache.set('invalid', 1), /values must be objects/);
		cache.clear();
		assert.equal(cache.size, 0);
	});

	test(`${source}: queued collection cannot erase replacements or entries after clear`, () => {
		const gc = collector();
		withApis(gc, () => {
			const Cache = source === 'npm' ? WeakerMap : bridge({globals: gc}).module.WeakerMap;
			const cache = new Cache(), old = {}, replacement = {}, current = {};
			cache.set(1, old);
			gc.queue(old);
			cache.set(1, replacement);
			gc.flush();
			assert.equal(cache.get(1), replacement);
			gc.queue(replacement);
			cache.clear();
			cache.set(1, current);
			gc.flush();
			assert.equal(cache.get(1), current);
			gc.queue(current);
			gc.flush();
			assert.equal(cache.size, 0);
		});
	});

	test(`${source}: sharing one value across keys shares its unregister token`, () => {
		const gc = collector();
		withApis(gc, () => {
			const Cache = source === 'npm' ? WeakerMap : bridge({globals: gc}).module.WeakerMap;
			const value = {}, cache = new Cache([[1, value], [2, value]]);
			cache.delete(1);
			gc.queue(value);
			gc.flush();
			// Both implementations cancel all registrations using that value as
			// the token. The surviving dead entry needs a lookup/iteration to prune.
			assert.equal(cache.size, 1);
			assert.equal(cache.has(2), false);
			assert.equal(cache.size, 0);
		});
	});
}

test('the npm iterator API differs from the embedded snapshot-array API', () => {
	const value = {}, Embedded = bridge().module.WeakerMap;
	const embedded = new Embedded([[1, value]]), npm = new WeakerMap([[1, value]]);
	assert.equal(Array.isArray(embedded.keys()), true);
	assert.equal(Array.isArray(embedded.values()), true);
	assert.equal(Array.isArray(npm.keys()), false);
	assert.equal(Array.isArray(npm.values()), false);
	assert.deepEqual([...npm.keys()], [1]);
	assert.deepEqual([...npm.values()], [value]);
	assert.equal(typeof embedded[Symbol.iterator]()[Symbol.iterator], 'undefined');
	const iterator = npm[Symbol.iterator]();
	assert.equal(iterator[Symbol.iterator](), iterator);
});

test('missing native weak-reference APIs produce an actionable startup error', () => {
	for(const apis of [
		{WeakRef: undefined}
		, {FinalizationRegistry: undefined}
		, {WeakRef: undefined, FinalizationRegistry: undefined}
	]){
		const value = {};
		assert.throws(() => bridge({globals: apis}), /Vrzno requires WeakRef and FinalizationRegistry.*enable_weak_ref.*2025-05-05/);
		withApis(apis, () => assert.throws(() => new WeakerMap([[1, value]]), /is not a constructor/));
	}
});

test('npm resolves WeakRef when setting values; Vrzno captures it at initialization', () => {
	const gc = collector();
	const {embedded, npm} = withApis(gc, () => ({
		embedded: new (bridge({globals: gc}).module.WeakerMap)(), npm: new WeakerMap()
	}));
	const value = {};
	embedded.set(1, value);
	npm.set(1, value);
	assert.ok(embedded.map.get(1) instanceof gc.WeakRef);
	assert.ok(npm.map.get(1) instanceof WeakRef);
});
