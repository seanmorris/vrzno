import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { PhpNode } from './lib/php-node.mjs';

test('Vrzno phpinfo reports the extension version', async context => {
	const php = new PhpNode();
	context.after(() => php.refresh());
	const version = await php.x`phpversion('vrzno')`;
	const info = await php.x`(function () {
		ob_start();
		(new ReflectionExtension('vrzno'))->info();
		return ob_get_clean();
	})()`;
	const text = info.replace(/<[^>]*>/g, ' ').replace(/=>/g, ' ').replace(/\s+/g, ' ');
	assert.ok(text.includes(`Version ${version}`), 'The phpinfo row must match phpversion("vrzno")');
});

test('Vrzno extension smoke test', async () => {
	const php = new PhpNode();
	let stdout = '';
	let stderr = '';
	php.addEventListener('output', event => event.detail.forEach(line => void (stdout += line)));
	php.addEventListener('error', event => event.detail.forEach(line => void (stderr += line)));
	await php.binary;

	const value = {
		nil: null,
		add: (left, right) => left + right,
	};

	const exitCode = await php.r`<?php
		$value = ${value};
		echo phpversion('vrzno'), "\n";
		var_dump(property_exists($value, 'nil'));
		var_dump(isset($value->nil));
		var_dump($value->add(2, 3));
		echo vrzno_eval('1 + 2'), "\n";
		vrzno_eval('globalThis.__vrzno_ci_sum = (left, right) => left + right');
		echo vrzno_run('__vrzno_ci_sum', [3, 4]), "\n";
		echo (new ReflectionFunction('vrzno_timeout'))->getReturnType(), "\n";
	`;

	assert.equal(exitCode, 0);
	assert.equal(stdout, "0.2.0\nbool(true)\nbool(false)\nint(5)\n3\n7\nvoid\n");
	assert.equal(stderr, '');

	const callback = await php.x`(function () {
		class VrznoCiCallableFixture {
			public function __construct(private int $factor) {}
			public function multiply(int $value): int { return $this->factor * $value; }
		}

		$fixture = new VrznoCiCallableFixture(3);
		return [$fixture, 'multiply'];
	})()`;
	const list = await php.x`['first', 'second']`;

	assert.equal(callback(4), 12);
	assert.equal(Object.getOwnPropertyDescriptor(list, 'length').value, 2);
});
