import assert from 'node:assert/strict';
import {test} from 'node:test';
import {bridge, declarations} from './lib/js-bridge.mjs';

test('all native bridge bodies load, with only await and fetch suspending', () => {
	const {module} = bridge();
	assert.equal(declarations().length, 44);
	assert.deepEqual(declarations().filter(entry => entry.async).map(entry => entry.name).sort(), [
		'php_stream_fetch_real_open', 'vrzno_await_internal'
	]);
	assert.equal(module.hasVrzno, true);
	assert.equal(module.vrznoOwnershipStats().targets, 1);
});

test('target references retain identity and release strong retention only at the final removal', () => {
	const {module, functions} = bridge();
	const object = {};
	module.tacked.add(object);
	const id = module.targets.add(object);
	assert.equal(module.targets.add(object), id);
	module.targets.remove(id);
	assert.equal(module.targets.get(id), object);
	assert.equal(module.tacked.has(object), true);
	module.targets.remove(id);
	assert.equal(module.targets.get(id), undefined);
	assert.equal(module.tacked.has(object), false);
	const generation = module.vrznoGeneration;
	functions.vrzno_js_shutdown();
	assert.equal(module.vrznoGeneration, generation + 1);
	assert.throws(() => module.vrznoAssertGeneration(generation), /previous PHP runtime/);
	assert.equal(module.targets.references.size, 1);
	assert.ok(module.targets.add(object) > id);
});

test('explicit ownership release and shutdown work without native weak-reference APIs', () => {
	const {module, functions, calls} = bridge({globals: {WeakRef: undefined, FinalizationRegistry: undefined}});
	const owner = {}, other = {};
	module.ownedZvalRegistry.register(owner, 100);
	module.ownedZvalRegistry.register(owner, 200);
	module.ownedZvalRegistry.register(other, 300);
	assert.equal(module.ownedZvalRegistry.outstanding, 2);
	assert.equal(module.ownedZvalRegistry.release(owner), true);
	assert.equal(module.ownedZvalRegistry.release(owner), false);
	functions.vrzno_js_shutdown();
	assert.equal(module.ownedZvalRegistry.outstanding, 0);
	assert.deepEqual(calls.map(call => [call.name, ...call.args]), [
		['vrzno_expose_destroy_zval', 200]
		, ['vrzno_expose_destroy_zval', 100]
		, ['vrzno_expose_destroy_zval', 300]
	]);
});

test('properties, dimensions, isset/empty, and error forwarding preserve PHP semantics', () => {
	const f = bridge(), {module, functions: js} = f;
	const values = new Map(), errors = [];
	module.jsToZval = (value, address) => values.set(address, value);
	module.zvalToJS = address => values.get(address);
	module.vrznoThrowRuntimeError = error => errors.push(error.message);
	const object = {zero: '0', nil: null, answer: 42};
	const id = module.targets.add(object), name = f.string('answer');
	js.vrzno_js_property_read(id, name, 10);
	js.vrzno_js_magic_get(id, name, 11);
	assert.equal(values.get(10), 42);
	assert.equal(values.get(11), 42);
	values.set(20, 43);
	js.vrzno_js_property_write(id, name, 20, 4);
	assert.equal(object.answer, 43);
	assert.equal(js.vrzno_js_property_has(id, f.string('nil'), 2), true);
	assert.equal(js.vrzno_js_property_has(id, f.string('nil'), 0), false);
	assert.equal(js.vrzno_js_property_has(id, f.string('zero'), 1), false);
	js.vrzno_js_property_write(id, name, 20, 0);
	assert.equal(Object.hasOwn(object, 'answer'), false);
	js.vrzno_js_property_unset(id, f.string('nil'));
	assert.equal(Object.hasOwn(object, 'nil'), false);
	const array = ['zero'], arrayId = module.targets.add(array);
	js.vrzno_js_array_append(arrayId, 20);
	js.vrzno_js_index_read(arrayId, 1, 30);
	assert.equal(values.get(30), 43);
	js.vrzno_js_key_write(arrayId, f.string('extra'), 20);
	js.vrzno_js_key_read(arrayId, f.string('extra'), 31);
	assert.equal(values.get(31), 43);
	assert.equal(js.vrzno_js_index_has(arrayId, 1, 1), true);
	assert.equal(js.vrzno_js_key_has(arrayId, f.string('missing'), 0), false);
	js.vrzno_js_index_write(arrayId, 0, 20);
	js.vrzno_js_index_unset(arrayId, 1);
	js.vrzno_js_key_unset(arrayId, f.string('extra'));
	assert.equal(array[0], 43);
	assert.equal(1 in array, false);
	assert.equal('extra' in array, false);
	const bytes = new Uint8Array([1, 2]), bytesId = module.targets.add(bytes.buffer);
	assert.equal(js.vrzno_js_array_valid(bytesId, 1), 1);
	assert.equal(js.vrzno_js_array_valid(bytesId, 2), 0);
	js.vrzno_js_array_current(bytesId, 1, 32);
	assert.equal(values.get(32), 2);
	assert.equal(module.vrznoNormalizeArrayKey('01'), '01');
	assert.equal(module.vrznoNormalizeArrayKey('-1'), -1);
	const bad = module.targets.add(new Proxy({}, {get() { throw new Error('read failed'); }}));
	js.vrzno_js_property_read(bad, name, 33);
	assert.deepEqual(errors, ['read failed']);
});

test('native argument arrays preserve JS method receivers, invocation, and constructor identity', () => {
	const f = bridge(), {module, functions: js} = f;
	const values = new Map([[100, 3], [116, 4]]);
	module.zvalToJS = address => values.get(address);
	module.jsToZval = (value, address) => values.set(address, value);
	f.view.setUint32(200, 100, true);
	f.view.setUint32(204, 116, true);
	const target = {offset: 5, add(a, b) { return this.offset + a + b; }};
	const id = module.targets.add(target);
	js.vrzno_js_method_call(id, f.string('add'), 200, 2, 4, 300);
	assert.equal(values.get(300), 12);
	js.vrzno_js_invoke(module.targets.add((a, b) => a * b), 100, 2, 16, 300);
	assert.equal(values.get(300), 12);
	class Example { constructor(a, b) { this.sum = a + b; } }
	const constructorId = module.targets.add(Example);
	assert.equal(js.vrzno_js_class_lookup(constructorId), undefined);
	js.vrzno_js_class_register(constructorId, 400);
	assert.equal(js.vrzno_js_class_lookup(constructorId), 400);
	assert.equal(js.vrzno_js_object_create(400), 0);
	const instanceId = js.vrzno_js_construct(400, 100, 2, 16);
	const instance = module.targets.get(instanceId);
	assert.ok(instance instanceof Example);
	assert.equal(instance.sum, 7);
	assert.equal(module.tacked.has(instance), true);
	assert.equal(f.readString(js.vrzno_js_properties_json(instanceId)), '{"sum":7}');
	assert.equal(f.readString(js.vrzno_js_to_string(instanceId)), '[object Object]');
	js.vrzno_js_object_release(instanceId);
	assert.equal(module.targets.get(instanceId), undefined);
});

test('legacy helpers, environment values, await, and timeout ownership use the extracted bodies', async () => {
	const f = bridge({globals: {sum: (a, b) => a + b}}), {module, functions: js} = f;
	const values = new Map(), errors = [];
	module.jsToZval = (value, address) => values.set(address, value);
	module.vrznoThrowRuntimeError = error => errors.push(error.message);
	assert.equal(f.readString(js.vrzno_js_eval(f.string('2 + 3'))), '5');
	assert.equal(f.readString(js.vrzno_js_run(f.string('sum'), f.string('[3, 4]'))), '7');
	assert.equal(js.vrzno_js_run(f.string('missing'), f.string('[]')), 0);
	module.option = 23;
	module.shared.option = 24;
	js.vrzno_js_env(f.string('option'), 100);
	js.vrzno_js_shared(f.string('option'), 101);
	assert.equal(values.get(100), 23);
	assert.equal(values.get(101), 24);
	await js.vrzno_await_internal(module.targets.add(Promise.resolve(25)), 102);
	assert.equal(values.get(102), 25);
	await js.vrzno_await_internal(module.targets.add(Promise.reject(new Error('rejected'))), 103);
	assert.equal(errors.at(-1), 'rejected');
	js.vrzno_js_timeout(0, 500);
	assert.equal(module.ownedZvalRegistry.outstanding, 1);
	f.timers.shift()();
	assert.equal(module.ownedZvalRegistry.outstanding, 0);
	assert.ok(f.calls.some(call => call.name === 'vrzno_exec_zval_callback' && call.args[0] === 500));
	js.vrzno_js_timeout(0, 600);
	js.vrzno_js_shutdown();
	f.timers.shift()();
	assert.equal(f.calls.filter(call => call.name === 'vrzno_exec_zval_callback').length, 1);
	assert.equal(module.ownedZvalRegistry.outstanding, 0);
});

test('fetch snapshots binary options, returns headers and bytes, and consumes its context', async () => {
	let request;
	const f = bridge({globals: {fetch: async (url, context) => {
		request = {url, context};
		return {status: 201, statusText: 'Created', headers: new Map([['x-test', 'yes']]), arrayBuffer: async () => new Uint8Array([4, 0, 6]).buffer};
	}}});
	const {module, functions: js} = f;
	const context = js.vrzno_js_fetch_context();
	js.vrzno_js_fetch_method(context, f.string('POST'));
	js.vrzno_js_fetch_header(context, f.string('X-One: first'));
	js.vrzno_js_fetch_headers(context, f.string('X-Two: second\r\ninvalid'));
	f.heap.set([1, 0, 3], 100);
	js.vrzno_js_fetch_body(context, 100, 3);
	f.heap[100] = 99;
	js.vrzno_js_fetch_ignore_errors(context, 1);
	const response = await js.php_stream_fetch_real_open(f.string('https://example.test/'), context, 4, 120, 124);
	assert.equal(request.context.method, 'POST');
	assert.deepEqual(Array.from(request.context.body), [1, 0, 3]);
	assert.equal(request.context.headers['X-One'], 'first');
	assert.equal(request.context.headers['X-Two'], 'second');
	assert.equal(module.targets.get(context), undefined);
	assert.equal(js.vrzno_js_fetch_status(response), 201);
	assert.equal(f.view.getUint32(124, true), 2);
	const headers = f.view.getUint32(120, true);
	assert.equal(f.readString(f.view.getUint32(headers, true)), 'HTTP/1.1 201 Created');
	assert.equal(js.vrzno_js_fetch_read(response, 140, 1, 8), 2);
	assert.deepEqual(Array.from(f.heap.slice(140, 142)), [0, 6]);
	assert.equal(js.vrzno_js_fetch_read(response, 140, 3, 8), 0);
	js.vrzno_js_fetch_release(response);
	assert.equal(module.targets.get(response), undefined);
});

test('fetch rejection records an error response and releases the retained context', async () => {
	const f = bridge({globals: {fetch: async () => { throw new Error('offline'); }}});
	const {module, functions: js} = f;
	const context = js.vrzno_js_fetch_context();
	const response = await js.php_stream_fetch_real_open(f.string('https://example.test/'), context, 4, 120, 124);
	assert.equal(js.vrzno_js_fetch_status(response), -1);
	assert.equal(module.targets.get(response).error, 'offline');
	assert.equal(module.targets.get(context), undefined);
	assert.equal(f.view.getUint32(124, true), 0);
	js.vrzno_js_fetch_release(response);
});
