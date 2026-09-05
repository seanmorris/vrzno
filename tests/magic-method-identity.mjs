import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { PhpNode, capture } from './lib/php-node.mjs';

test('Magic method identity uses the exact requested name before and after calls', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const output = capture(php);
	// The array prevents Promise assimilation from probing the magic "then" method.
	const values = await php.x`[new class {
		public function __call($name, $args) { return $name . ':' . implode(',', $args); }
	}]`;
	const object = values[0];
	const first = object.first;
	assert.equal(object.first, first, 'Repeated extraction before invocation must reuse the wrapper');
	assert.equal(first('one'), 'first:one');
	const second = object.second;
	assert.notEqual(first, second, 'A recycled Zend trampoline must not alias another method');
	assert.equal(object.first, first);
	assert.equal(object.second, second);
	assert.notEqual(object.FIRST, first);
	for(let i = 0; i < 20; i++)
	{
		assert.equal(first(i), `first:${i}`);
		assert.equal(second(i), `second:${i}`);
		assert.equal(object.FIRST(i), `FIRST:${i}`);
	}
	// PHP itself truncates magic dispatch names at NUL for compatibility.
	assert.equal(object['with\0nul'](), 'with:');
	assert.equal(object['méthode'](), 'méthode:');
	assert.equal(output.stderr, '');
});

test('Concrete methods remain case insensitive and magic methods retain their receiver', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const values = await php.x`(function () {
		$first = new class('first') {
			public function __construct(private string $label) {}
			public function concrete() { return $this->label; }
			public function __call($name, $args) { return $this->label . ':' . $name; }
		};
		return [$first, new ($first::class)('second')];
	})()`;
	const first = values[0];
	const second = values[1];
	assert.equal(first.concrete, first.CONCRETE);
	assert.notEqual(first.concrete, second.concrete);
	assert.equal(first.CONCRETE(), 'first');
	assert.equal(second.concrete(), 'second');
	assert.notEqual(first.missing, second.missing);
	assert.equal(first.missing(), 'first:missing');
	assert.equal(second.missing(), 'second:missing');
	assert.equal(String(first), 'first:__toString');
});

test('Magic cache hits create no additional PHP owners', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const module = await php.binary;
	const values = await php.x`[new class {
		public function __call($name, $args) { return $name; }
	}]`;
	const object = values[0];
	const callback = object.event;
	const baseline = module.vrznoOwnershipStats().allocations;
	const ccall = module.ccall;
	let created = 0;
	module.ccall = (...args) => {
		if(['vrzno_expose_method_callable', 'vrzno_expose_copy_zval', 'vrzno_expose_copy_object'].includes(args[0])) created++;
		return ccall(...args);
	};
	try
	{
		for(let i = 0; i < 2000; i++) assert.equal(object.event, callback);
	}
	finally
	{
		module.ccall = ccall;
	}
	assert.equal(created, 0);
	assert.equal(module.vrznoOwnershipStats().allocations, baseline);
	assert.equal(callback(), 'event');
});

test('Magic methods support listener removal and callable-array PHP round trips', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	let calls = 0;
	const called = () => calls++;
	const values = await php.x`(function () {
		$called = ${called};
		$object = new class($called) {
			public function __construct(private $called) {}
			public function __call($name, $args) { ($this->called)(); return $name; }
		};
		$callback = [$object, 'arrayCallback'];
		return [$object, $callback, $callback];
	})()`;
	const object = values[0];
	const events = new EventTarget();
	events.addEventListener('ping', object.listener);
	events.dispatchEvent(new Event('ping'));
	events.removeEventListener('ping', object.listener);
	events.dispatchEvent(new Event('ping'));
	assert.equal(calls, 1);
	const callback = values[1];
	assert.equal(callback, values[2]);
	for(let i = 0; i < 2000; i++) assert.equal(values[1], callback);
	assert.equal(callback(), 'arrayCallback');
	assert.equal(callback(), 'arrayCallback');
	assert.equal(await php.x`is_array(${object.listener})`, true);
});

test('Magic wrappers retain their receiver and reject calls after refresh', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const module = await php.binary;
	const output = capture(php);
	const values = await php.x`[new class {
		public function __call($name, $args) { return $name; }
		public function __destruct() { echo 'released'; }
	}]`;
	const object = values[0];
	const callback = object.retained;
	assert.equal(module.ownedZvalRegistry.release(object), true);
	assert.equal(module.ownedZvalRegistry.release(values), true);
	assert.equal(output.stdout, '');
	assert.equal(callback(), 'retained');
	assert.equal(module.ownedZvalRegistry.release(callback), true);
	await php.exec('gc_collect_cycles();');
	assert.equal(output.stdout, 'released');
	await php.refresh();
	assert.equal(module.vrznoOwnershipStats().outstanding, 0);
	assert.throws(() => callback(), {
		name: 'ReferenceError', message: 'Vrzno value belongs to a previous PHP runtime.',
	});
});

test('A retained magic callable propagates PHP exceptions on successive invocations', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const output = capture(php);
	const values = await php.x`[new class {
		public function __call($name, $args) { throw new RuntimeException($name); }
	}]`;
	const callback = values[0].failure;
	const invoke = () => callback();
	assert.equal(await php.r`<?php
		$invoke = ${invoke};
		for($i = 0; $i < 2; $i++) {
			try { $invoke(); }
			catch (RuntimeException $error) { echo $error->getMessage(), "\n"; }
		}
	`, 0);
	assert.equal(output.stdout, 'failure\nfailure\n');
	assert.equal(output.stderr, '');
});
