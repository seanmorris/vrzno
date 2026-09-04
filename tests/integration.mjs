import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const phpWasmRoot = resolve(process.env.PHP_WASM_ROOT ?? '../php-wasm');
const { PhpNode: BasePhpNode } = await import(pathToFileURL(
	resolve(phpWasmRoot, 'packages/php-wasm/PhpNode.mjs')
));
const { nodeRuntimeOptions } = await import(pathToFileURL(
	resolve(phpWasmRoot, 'test/lib/node-runtime-options.mjs')
));

class PhpNode extends BasePhpNode
{
	constructor(args = {})
	{
		super(nodeRuntimeOptions(args));
	}
}

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
