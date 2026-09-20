import {test} from 'node:test';
import {strict as assert} from 'node:assert';
import {PhpNode} from './lib/php-node.mjs';

test('PHP option objects distinguish absent properties from explicit null values', async () => {
	const php = new PhpNode({version: process.env.PHP_VERSION ?? '8.4'});
	const options = await php.x`(object) ['method' => 'GET', 'nil' => null]`;
	assert.equal(options.method, 'GET');
	assert.equal(options.cache, undefined);
	assert.equal(options.headers, undefined);
	assert.equal(options.attributionReporting, undefined);
	assert.equal('cache' in options, false);
	assert.equal(Object.getOwnPropertyDescriptor(options, 'cache'), undefined);
	assert.equal(options.nil, null);
	assert.equal('nil' in options, true);
	assert.equal(Object.getOwnPropertyDescriptor(options, 'nil').value, null);
	assert.deepEqual(Object.keys(options), ['method', 'nil']);
	const request = new Request('https://example.invalid/', options);
	assert.equal(request.method, 'GET');
	assert.equal([...request.headers].length, 0);
});

test('absent-property handling preserves magic getter and isset behavior', async () => {
	const php = new PhpNode({version: process.env.PHP_VERSION ?? '8.4'});
	const value = await php.x`new class {
		public $reads = 0;
		public $checks = 0;
		public function __isset($name) { $this->checks++; return $name === 'present'; }
		public function __get($name) { $this->reads++; return null; }
		public function method() { return 'method'; }
	}`;
	// Promise resolution probes `then` before the PHP object reaches this caller.
	const checks = value.checks;
	const reads = value.reads;
	assert.equal(value.absent, undefined);
	assert.equal(value.checks, checks + 1);
	assert.equal(value.reads, reads);
	assert.equal(value.present, null);
	assert.equal(value.checks, checks + 2);
	assert.equal(value.reads, reads + 1);
	assert.equal(value.method(), 'method');
	const getter = await php.x`new class {
		public $reads = 0;
		public function __get($name) { $this->reads++; return null; }
	}`;
	const getterReads = getter.reads;
	assert.equal(getter.virtual, null);
	assert.equal(getter.reads, getterReads + 1);
});
