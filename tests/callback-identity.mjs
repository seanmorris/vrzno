import { test } from 'node:test';
import { strict as assert } from 'node:assert';

import { PhpNode, capture } from './lib/php-node.mjs';
import { runLifecycleChild } from './lib/lifecycle-process.mjs';

test('The same PHP closure can be added and removed as an event listener', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const output = capture(php);
	const listeners = new EventTarget();
	const callbacks = [];
	let calls = 0;
	const bridge = {
		listeners,
		remember(callback) { callbacks.push(callback); },
		called() { calls++; },
		fire() { listeners.dispatchEvent(new Event('ping')); },
	};

	assert.equal(await php.r`<?php
		$bridge = ${bridge};
		$listener = function () use ($bridge) { $bridge->called(); };
		$bridge->remember($listener);
		$bridge->remember($listener);
		$bridge->listeners->addEventListener('ping', $listener);
		$bridge->fire();
		$bridge->listeners->removeEventListener('ping', $listener);
		$bridge->fire();
	`, 0);

	assert.equal(output.stderr, '');
	assert.equal(calls, 1, 'The removed listener must not fire again');
	assert.equal(callbacks[0], callbacks[1]);
	bridge.fire();
	assert.equal(calls, 1);
});

test('Closure and invokable-object identity is preserved without merging instances', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const values = await php.x`(function () {
		$make = fn($value) => fn() => $value;
		$first = $make('first');
		$second = $make('second');
		$invokable = new class {
			public function __invoke() { return 'invoked'; }
		};
		return [$first, $first, $second, $invokable, $invokable, clone $invokable];
	})()`;

	assert.equal(values[0], values[1]);
	assert.notEqual(values[0], values[2]);
	assert.equal(values[0](), 'first');
	assert.equal(values[2](), 'second');
	assert.equal(values[3], values[4]);
	assert.notEqual(values[3], values[5]);
	assert.equal(values[3](), 'invoked');
});

test('Callable-array identity retains the correct receiver and PHP round trip', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const values = await php.x`(function () {
		$first = new class('first') {
			public function __construct(private string $value) {}
			public function value() { return $this->value; }
		};
		$second = new ($first::class)('second');
		$callback = [$first, 'value'];
		return [$callback, $callback, [$second, 'value'], [$first, 'value']];
	})()`;

	const first = values[0];
	assert.equal(first, values[1]);
	assert.notEqual(first, values[2]);
	assert.notEqual(first, values[3], 'Separately constructed arrays keep their own identity');
	assert.equal(first(), 'first');
	assert.equal(values[2](), 'second');
	assert.equal(await php.x`is_array(${first})`, true);
});

test('Repeated callable conversion and method extraction reuse ownership', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const module = await php.binary;
	const values = await php.x`(function () {
		$first = new class('first') {
			public function __construct(private string $value) {}
			public function value() { return $this->value; }
		};
		$second = new ($first::class)('second');
		return [fn() => 42, [$first, 'value'], $first, $second];
	})()`;
	const callback = values[0];
	const arrayCallback = values[1];
	const first = values[2];
	const second = values[3];
	const firstMethod = first.value;
	const secondMethod = second.value;
	assert.notEqual(firstMethod, secondMethod);
	assert.equal(firstMethod(), 'first');
	assert.equal(secondMethod(), 'second');

	const baseline = module.vrznoOwnershipStats().allocations;
	const ccall = module.ccall;
	let copies = 0;
	module.ccall = (...args) => {
		if(['vrzno_expose_copy_zval', 'vrzno_expose_copy_object'].includes(args[0]))
		{
			copies++;
		}
		return ccall(...args);
	};
	try
	{
		for(let i = 0; i < 100; i++)
		{
			assert.equal(values[0], callback);
			assert.equal(values[1], arrayCallback);
			assert.equal(first.value, firstMethod);
			assert.equal(second.value, secondMethod);
		}
	}
	finally
	{
		module.ccall = ccall;
	}
	assert.equal(copies, 0, 'Cache hits must not allocate duplicate PHP owners');
	assert.equal(module.vrznoOwnershipStats().allocations, baseline);
});

test('Refresh clears callable identity and rejects stale wrappers', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const module = await php.binary;
	const previous = await php.x`fn() => 'previous'`;
	assert.equal(previous(), 'previous');
	await php.refresh();

	assert.equal(module.vrznoOwnershipStats().outstanding, 0);
	assert.throws(() => previous(), {
		name: 'ReferenceError',
		message: 'Vrzno value belongs to a previous PHP runtime.',
	});
	const current = await php.x`fn() => 'current'`;
	assert.notEqual(current, previous);
	assert.equal(current(), 'current');
});

test('The callable cache releases abandoned wrappers under native GC', () =>
	runLifecycleChild('native-callback'));
