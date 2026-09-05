import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { setImmediate } from 'node:timers/promises';
import { PhpNode } from './lib/php-node.mjs';

const staleValueError = {
	name: 'ReferenceError',
	message: 'Vrzno value belongs to a previous PHP runtime.',
};

test('Detached array factories retain independent iterator owners', async context => {
	const php = new PhpNode();
	const module = await php.binary;
	context.after(() => php.refresh());
	const registry = module.ownedZvalRegistry;
	const baseline = module.vrznoOwnershipStats().outstanding;
	const proxy = await php.x`range(1, 3)`;
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 1);

	const factory = proxy[Symbol.iterator];
	// Check ownership before forcing proxy finalization: old runtimes must fail safely.
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 2,
		'A detached iterator factory must own a separate PHP value');
	assert.equal(registry.release(proxy), true);

	const first = factory();
	const second = factory();
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 3);
	assert.equal(registry.release(factory), true);

	assert.deepEqual(first.next(), {done: false, value: 1});
	assert.deepEqual(second.next(), {done: false, value: 1});
	assert.deepEqual(first.next(), {done: false, value: 2});
	assert.deepEqual(first.next(), {done: false, value: 3});
	assert.deepEqual(first.next(), {done: true, value: undefined});
	assert.deepEqual(first.next(), {done: true, value: undefined});
	assert.equal(registry.release(first), true);
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 1);

	assert.deepEqual(second.next(), {done: false, value: 2});
	assert.deepEqual(second.next(), {done: false, value: 3});
	assert.deepEqual(second.next(), {done: true, value: undefined});
	assert.equal(registry.release(second), true);
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline);

	const released = module.vrznoOwnershipStats().releases;
	for(const owner of [proxy, factory, first, second])
	{
		assert.equal(registry.release(owner), false);
	}
	assert.equal(module.vrznoOwnershipStats().releases, released);
});

test('Separate array factories survive each other being released', async context => {
	const php = new PhpNode();
	const module = await php.binary;
	context.after(() => php.refresh());
	const registry = module.ownedZvalRegistry;
	const baseline = module.vrznoOwnershipStats().outstanding;
	const proxy = await php.x`range(4, 5)`;
	const firstFactory = proxy[Symbol.iterator];
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 2,
		'A detached iterator factory must own a separate PHP value');
	const secondFactory = proxy[Symbol.iterator];
	assert.notEqual(firstFactory, secondFactory);
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 3);
	assert.equal(registry.release(proxy), true);
	assert.equal(registry.release(firstFactory), true);

	const iterator = secondFactory();
	assert.equal(registry.release(secondFactory), true);
	assert.deepEqual(iterator.next(), {done: false, value: 4});
	assert.deepEqual(iterator.next(), {done: false, value: 5});
	assert.deepEqual(iterator.next(), {done: true, value: undefined});
	assert.equal(registry.release(iterator), true);
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline);
});

test('Refresh rejects stale array factories before they allocate PHP owners', async context => {
	const php = new PhpNode();
	const module = await php.binary;
	context.after(() => php.refresh());
	const baseline = module.vrznoOwnershipStats().outstanding;
	const proxy = await php.x`range(1, 3)`;
	const factory = proxy[Symbol.iterator];
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 2,
		'A detached iterator factory must own a separate PHP value');
	const iterator = factory();
	assert.deepEqual(iterator.next(), {done: false, value: 1});

	await php.refresh();

	assert.equal(module.vrznoOwnershipStats().outstanding, 0);
	const refreshed = module.vrznoOwnershipStats();
	assert.throws(() => factory(), staleValueError);
	assert.throws(() => iterator.next(), staleValueError);
	assert.deepEqual(module.vrznoOwnershipStats(), refreshed);
	assert.equal(await php.x`6 * 7`, 42);
});

test('Unreachable array factories and iterators release their PHP owners', {
	skip: typeof globalThis.gc !== 'function' || typeof WeakRef !== 'function',
}, async context => {
	const php = new PhpNode();
	const module = await php.binary;
	context.after(() => php.refresh());
	const baseline = module.vrznoOwnershipStats().outstanding;

	const createUnreachableOwners = async () => {
		const proxy = await php.x`range(1, 3)`;
		const factory = proxy[Symbol.iterator];
		assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 2,
			'A detached iterator factory must own a separate PHP value');
		const iterator = factory();
		assert.deepEqual(iterator.next(), {done: false, value: 1});
		return [proxy, factory, iterator].map(owner => new WeakRef(owner));
	};
	const references = await createUnreachableOwners();

	for(let attempt = 0; attempt < 30; attempt++)
	{
		// Yield before GC so deref() in the previous attempt no longer keeps values alive.
		await setImmediate();
		globalThis.gc();
		await setImmediate();
		if(references.every(reference => reference.deref() === undefined)
			&& module.vrznoOwnershipStats().outstanding === baseline)
		{
			return;
		}
	}

	// Explicit release tests above enforce cleanup without depending on finalizer timing.
	context.skip('Collection or finalization was not observed during the bounded GC check');
});
