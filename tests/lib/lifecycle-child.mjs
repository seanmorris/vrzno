import { strict as assert } from 'node:assert';
import { setImmediate, setTimeout as delay } from 'node:timers/promises';
import { PhpNode } from './php-node.mjs';
import { lifecycleFailures } from './lifecycle-process.mjs';

const mode = process.argv[2];
const staleValueError = {
	name: 'ReferenceError',
	message: 'Vrzno value belongs to a previous PHP runtime.',
};

function controlledCollector()
{
	const references = [];
	const registries = [];
	const queued = [];

	class ControlledWeakRef
	{
		constructor(value) { this.value = value; references.push(this); }
		deref() { return this.value; }
	}

	class ControlledFinalizationRegistry
	{
		constructor(callback)
		{
			this.callback = callback;
			this.records = new Set();
			registries.push(this);
		}

		register(target, heldValue, token)
		{
			this.records.add({target, heldValue, token});
		}

		unregister(token)
		{
			let removed = false;
			for(const record of this.records)
			{
				if(record.token === token)
				{
					this.records.delete(record);
					removed = true;
				}
			}
			return removed;
		}
	}

	const queue = target => {
		for(const reference of references)
		{
			if(reference.value === target) reference.value = undefined;
		}
		for(const registry of registries)
		{
			for(const record of registry.records)
			{
				if(record.target === target) queued.push({registry, record});
			}
		}
	};
	const flush = reverse => {
		const pending = queued.splice(0);
		if(reverse) pending.reverse();
		for(const {registry, record} of pending)
		{
			if(registry.records.delete(record)) registry.callback(record.heldValue);
		}
	};

	return {
		WeakRef: ControlledWeakRef,
		FinalizationRegistry: ControlledFinalizationRegistry,
		queue,
		flush,
		collect(target, reverse = false) { queue(target); flush(reverse); },
	};
}

async function withCollector(collector, callback)
{
	const nativeWeakRef = globalThis.WeakRef;
	const nativeFinalizationRegistry = globalThis.FinalizationRegistry;
	try
	{
		if(collector)
		{
			globalThis.WeakRef = collector.WeakRef;
			globalThis.FinalizationRegistry = collector.FinalizationRegistry;
		}
		return await callback();
	}
	finally
	{
		globalThis.WeakRef = nativeWeakRef;
		globalThis.FinalizationRegistry = nativeFinalizationRegistry;
	}
}

const createRuntime = collector => withCollector(collector, async () => {
	const php = new PhpNode();
	const module = await php.binary;
	return {php, module};
});

async function controlledCallback(php, module, collector)
{
	if(mode === 'negative-strong-cache') module._callables = new Map();
	const source = await php.x`[fn() => 42]`;
	const baseline = module.vrznoOwnershipStats().outstanding;
	if(mode === 'negative-disabled-finalizer')
	{
		module.ownedZvalRegistry.registry = {register() {}, unregister() { return false; }};
	}
	const first = source[0];
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 1);
	if(mode === 'controlled-explicit-release')
	{
		collector.queue(first);
		assert.equal(module.ownedZvalRegistry.release(first), true);
		const released = module.vrznoOwnershipStats();
		assert.equal(module.ownedZvalRegistry.release(first), false);
		assert.deepEqual(module.vrznoOwnershipStats(), released);
		collector.flush();
		assert.deepEqual(module.vrznoOwnershipStats(), released,
			'Explicit release must cancel queued owner finalizers');
	}
	else
	{
		collector.collect(first, mode === 'controlled-callback-cache-first');
	}
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline, lifecycleFailures.disabledFinalizer);

	const second = source[0];
	// A strong-cache negative control must fail before invoking a finalized wrapper.
	assert.notEqual(second, first, lifecycleFailures.strongCache);
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 1);
	assert.equal(second(), 42);
	collector.collect(second);
	const released = module.vrznoOwnershipStats().releases;
	collector.collect(second);
	assert.equal(module.vrznoOwnershipStats().releases, released);
	collector.collect(source);
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline - 1);
}

async function controlledIterators(php, module, collector)
{
	const baseline = module.vrznoOwnershipStats().outstanding;
	const proxy = await php.x`range(1, 3)`;
	const factory = proxy[Symbol.iterator];
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 2,
		'A detached iterator factory must own a separate PHP value');
	const first = factory();
	const second = factory();
	collector.collect(proxy);
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 3);

	if(mode === 'controlled-factory-first')
	{
		collector.collect(factory);
		assert.deepEqual(first.next(), {done: false, value: 1});
		collector.collect(first);
		assert.deepEqual(second.next(), {done: false, value: 1});
		collector.collect(second);
	}
	else
	{
		collector.collect(first);
		assert.deepEqual(second.next(), {done: false, value: 1});
		const replacement = factory();
		collector.collect(factory);
		assert.deepEqual(second.next(), {done: false, value: 2});
		collector.collect(second);
		assert.deepEqual(replacement.next(), {done: false, value: 1});
		collector.collect(replacement);
	}
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline);
}

async function controlledShutdown(php, module, collector)
{
	const source = await php.x`[fn() => 42, range(1, 3)]`;
	const callback = source[0];
	const array = source[1];
	const factory = array[Symbol.iterator];
	const iterator = factory();
	for(const target of [source, callback, array, factory, iterator]) collector.queue(target);

	await withCollector(collector, () => php.refresh());
	assert.equal(module.vrznoOwnershipStats().outstanding, 0);
	assert.throws(() => callback(), staleValueError);
	assert.throws(() => factory(), staleValueError);
	assert.throws(() => iterator.next(), staleValueError);
	const fresh = await php.x`fn() => 43`;
	const before = module.vrznoOwnershipStats();
	collector.flush(true);
	assert.deepEqual(module.vrznoOwnershipStats(), before,
		'Shutdown must cancel queued owner finalizers before runtime reuse');
	assert.equal(fresh(), 43);
	collector.collect(fresh);
	assert.equal(module.vrznoOwnershipStats().outstanding, 0);
}

async function nativeCleanup(php, module)
{
	assert.equal(typeof globalThis.gc, 'function', 'The native-GC lane requires --expose-gc');
	assert.equal(typeof WeakRef, 'function', 'The native-GC lane requires WeakRef');
	assert.equal(typeof FinalizationRegistry, 'function', 'The native-GC lane requires FinalizationRegistry');
	const baseline = module.vrznoOwnershipStats().outstanding;
	let controlFinalized = false;
	const controlRegistry = new FinalizationRegistry(() => { controlFinalized = true; });
	const control = (() => {
		const target = {};
		controlRegistry.register(target, 'control');
		return new WeakRef(target);
	})();
	const createTargets = async () => {
		if(mode === 'native-callback')
		{
			const callback = await php.x`fn() => 42`;
			assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 1);
			return [new WeakRef(callback)];
		}
		const proxy = await php.x`range(1, 3)`;
		const factory = proxy[Symbol.iterator];
		assert.equal(module.vrznoOwnershipStats().outstanding, baseline + 2,
			'A detached iterator factory must own a separate PHP value');
		const iterator = factory();
		assert.deepEqual(iterator.next(), {done: false, value: 1});
		return [proxy, factory, iterator].map(target => new WeakRef(target));
	};
	const references = await createTargets();
	const deadline = performance.now() + 10_000;
	while(performance.now() < deadline)
	{
		await setImmediate();
		globalThis.gc();
		await delay(10);
		if(controlFinalized && control.deref() === undefined
			&& references.every(reference => reference.deref() === undefined)
			&& module.vrznoOwnershipStats().outstanding === baseline)
		{
			return;
		}
	}
	// Keep the registry itself reachable throughout the check.
	assert.ok(controlRegistry instanceof FinalizationRegistry);
	if(!controlFinalized || control.deref() !== undefined)
	{
		const error = new Error('Native GC control did not collect and finalize within 10000 ms');
		error.lifecycleKind = 'gc-control';
		throw error;
	}
	assert.ok(references.every(reference => reference.deref() === undefined),
		'Native GC collected its control but lifecycle targets remained reachable after 10000 ms');
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline,
		'Native GC finalized its control but PHP owners remained after 10000 ms');
}

let php;
let report;
try
{
	const collector = mode.startsWith('native-') ? null : controlledCollector();
	const runtime = await createRuntime(collector);
	php = runtime.php;
	if(mode === 'native-callback' || mode === 'native-iterators')
	{
		await nativeCleanup(php, runtime.module);
	}
	else if(mode === 'controlled-factory-first' || mode === 'controlled-iterator-first')
	{
		await controlledIterators(php, runtime.module, collector);
	}
	else if(mode === 'controlled-shutdown')
	{
		await controlledShutdown(php, runtime.module, collector);
	}
	else if(['controlled-callback-owner-first', 'controlled-callback-cache-first', 'controlled-explicit-release',
		'negative-strong-cache', 'negative-disabled-finalizer'].includes(mode))
	{
		await controlledCallback(php, runtime.module, collector);
	}
	else
	{
		throw new Error(`Unknown lifecycle mode: ${mode}`);
	}
	report = {mode, ok: true};
}
catch(error)
{
	report = {
		mode,
		ok: false,
		kind: error.lifecycleKind ?? (error.code === 'ERR_ASSERTION' ? 'assertion' : 'unexpected'),
		message: error.message.split('\n')[0],
		stack: error.stack,
	};
	process.exitCode = 1;
}
finally
{
	if(php)
	{
		try { await php.refresh(); }
		catch(error)
		{
			report = {mode, ok: false, kind: 'cleanup', message: error.message};
			process.exitCode = 1;
		}
	}
}
console.log('VRZNO_LIFECYCLE_RESULT ' + JSON.stringify(report));
