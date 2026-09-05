import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { PhpNode, capture } from './lib/php-node.mjs';

test('Expression results release their PHP owner with the JavaScript proxy', async () => {
	const php = new PhpNode();
	const output = capture(php);
	const module = await php.binary;
	const baseline = module.vrznoOwnershipStats().outstanding;
	const value = await php.x`new class {
		public function __destruct() { echo "released\n"; }
	}`;

	assert.equal(output.stdout, '');
	// Deterministically simulate proxy finalization without relying on JavaScript GC.
	assert.equal(module.ownedZvalRegistry.release(value), true);
	await php.exec('gc_collect_cycles();');

	assert.equal(output.stdout, 'released\n');
	assert.equal(output.stderr, '');
	assert.equal(module.vrznoOwnershipStats().outstanding, baseline);
});
