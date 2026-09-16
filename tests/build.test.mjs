import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {test} from 'node:test';
import {parse} from 'espree';
import {declarations, root, templates} from './lib/js-bridge.mjs';

const entries = declarations();
const inputs = [...new Set(entries.flatMap(entry => entry.inputs))];
const units = templates.map(name => name.replace('_js.h.in', ''));
const compiler = process.env.CC || 'emcc';
const syntax = source => JSON.parse(JSON.stringify(parse(source, {
	ecmaVersion: 'latest', sourceType: 'module', ecmaFeatures: {globalReturn: true}
}), (key, value) => ['start', 'end'].includes(key) ? undefined : value));

function fixture(t, outOfTree = false)
{
	const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vrzno-make-'));
	t.after(() => fs.rmSync(temporary, {recursive: true, force: true}));
	const source = path.join(temporary, 'source');
	const build = outOfTree ? path.join(temporary, 'build') : source;
	fs.mkdirSync(source);
	fs.mkdirSync(build, {recursive: true});
	for(const name of ['Makefile.frag', ...templates, ...inputs])
		fs.copyFileSync(path.join(root, name), path.join(source, name));
	const header = unit => path.join(build, `generated/${unit}_js.h`);
	const depfile = unit => path.join(build, `generated/${unit}_js.d`);
	const object = unit => path.join(build, `${unit}.lo`);
	const fragment = fs.readFileSync(path.join(source, 'Makefile.frag'), 'utf8')
		.replaceAll('$(srcdir)', source).replaceAll('$(builddir)', build);
	fs.writeFileSync(path.join(build, 'Makefile'), `all: ${units.map(object).join(' ')}\ndistclean: clean\n`
		+ units.map(unit => `${object(unit)}:\n\t@cp "${header(unit)}" "$@"\n`).join('') + fragment);
	const run = (goal = 'all', success = true, variables = []) => {
		const result = spawnSync('make', ['--no-print-directory', '-j8', `CC=${compiler}`, ...variables, goal], {
			cwd: build, encoding: 'utf8', timeout: 60000
		});
		assert.equal(result.error, undefined, 'Make must finish without hanging');
		if(success) assert.equal(result.status, 0, result.stdout + result.stderr);
		else assert.notEqual(result.status, 0, 'Expected generation to fail');
		return result;
	};
	const outputs = units.flatMap(unit => [header(unit), depfile(unit), object(unit)]);
	const mtimes = () => outputs.map(file => fs.statSync(file, {bigint: true}).mtimeNs);
	const prepareEdit = () => {
		const past = new Date(Date.now() - 10000), newer = new Date(Date.now() - 5000);
		for(const name of ['Makefile.frag', ...templates, ...inputs])
			fs.utimesSync(path.join(source, name), past, past);
		for(const file of outputs) fs.utimesSync(file, newer, newer);
	};
	return {source, build, header, depfile, object, outputs, run, mtimes, prepareEdit};
}

for(const separate of [false, true])
{
	test(`Make tracks every JS body across all five objects (${separate ? 'separate build directory' : 'source directory'})`, t => {
		const f = fixture(t, separate);
		f.run();
		for(const entry of entries)
		{
			const unit = entry.template.replace('_js.h.in', '');
			const header = fs.readFileSync(f.header(unit), 'utf8');
			const opening = header.match(new RegExp('^EM_(?:ASYNC_)?JS\\([^\\n]*\\b' + entry.name + '\\b[^\\n]*\\{\\n', 'm'));
			assert.notEqual(opening, null, entry.name);
			const tail = header.slice(opening.index + opening[0].length);
			const next = tail.search(/^EM_(?:ASYNC_)?JS\(/m);
			const section = next < 0 ? tail : tail.slice(0, next);
			const body = section.slice(0, section.lastIndexOf('\n});'));
			assert.deepEqual(syntax(body), syntax(entry.inputs.map(name => fs.readFileSync(path.join(f.source, name), 'utf8')).join('\n')), entry.name);
			for(const input of entry.inputs)
				assert.ok(fs.readFileSync(f.depfile(unit), 'utf8').includes(input), input);
		}
		const unchanged = f.mtimes();
		f.run();
		assert.deepEqual(f.mtimes(), unchanged, 'No-op Make preserves every output timestamp');
		for(const input of inputs)
		{
			f.prepareEdit();
			const before = f.mtimes();
			const unit = entries.find(entry => entry.inputs.includes(input)).template.replace('_js.h.in', '');
			const marker = `// edited ${input}: $value, \\n, "quotes", \`template\`\n`;
			fs.appendFileSync(path.join(f.source, input), marker);
			f.run();
			assert.ok(fs.readFileSync(f.header(unit), 'utf8').includes(marker));
			assert.equal(fs.readFileSync(f.object(unit), 'utf8'), fs.readFileSync(f.header(unit), 'utf8'));
			const after = f.mtimes();
			for(let i = 0; i < units.length; i++)
			{
				if(units[i] === unit) assert.notEqual(after[3 * i + 2], before[3 * i + 2], input);
				else assert.deepEqual(after.slice(3 * i, 3 * i + 3), before.slice(3 * i, 3 * i + 3), units[i]);
			}
		}
	});
}

test('compiler dependency files track nested includes and preserve good outputs on errors', t => {
	const f = fixture(t, true);
	f.run();
	f.prepareEdit();
	const extra = path.join(f.source, 'extra.js'), leaf = path.join(f.source, 'nested/leaf.js');
	fs.mkdirSync(path.dirname(leaf));
	fs.writeFileSync(leaf, '// nested body\n');
	fs.writeFileSync(extra, '#include "nested/leaf.js"\n');
	fs.appendFileSync(path.join(f.source, 'vrzno_js.h.in'), '#include "extra.js"\n');
	f.run();
	assert.ok(fs.readFileSync(f.depfile('vrzno'), 'utf8').includes(leaf));
	const before = fs.readFileSync(f.header('vrzno'), 'utf8');
	const deps = fs.readFileSync(f.depfile('vrzno'), 'utf8');
	fs.unlinkSync(leaf);
	const failed = f.run('all', false);
	assert.match(failed.stderr, /extra\.js:1:\d+: fatal error:.*leaf\.js.*file not found/);
	assert.equal(fs.readFileSync(f.header('vrzno'), 'utf8'), before);
	assert.equal(fs.readFileSync(f.depfile('vrzno'), 'utf8'), deps);
	assert.equal(fs.existsSync(f.header('vrzno') + '.tmp'), false);
	fs.writeFileSync(extra, '// removed missing include\n');
	f.run();
	assert.ok(!fs.readFileSync(f.depfile('vrzno'), 'utf8').includes(leaf));
});

test('directives-only preprocessing preserves JS macro names, comments, and template strings', t => {
	const f = fixture(t);
	const body = 'const sample = {__LINE__: 17, __FILE__: "kept", __COUNTER__: 23, unix: 29};\n'
		+ '/* keep comment */\nconst $value = `literal ${sample.__LINE__} \\n`;\n';
	fs.appendFileSync(path.join(f.source, 'vrzno_shutdown.js'), body);
	f.run();
	assert.ok(fs.readFileSync(f.header('vrzno'), 'utf8').includes(body));
});

test('missing generated headers and dependency files are recreated independently', t => {
	const f = fixture(t, true);
	f.run();
	for(const unit of units)
	{
		for(const file of [f.header(unit), f.depfile(unit)])
		{
			f.prepareEdit();
			fs.unlinkSync(file);
			f.run();
			assert.ok(fs.existsSync(file));
			assert.equal(fs.readFileSync(f.object(unit), 'utf8'), fs.readFileSync(f.header(unit), 'utf8'));
		}
	}
});

test('clean targets work without source inputs or an installed compiler', t => {
	const f = fixture(t);
	f.run();
	for(const name of [...templates, ...inputs]) fs.unlinkSync(path.join(f.source, name));
	for(const goal of ['clean', 'distclean', 'clean-vrzno-js']) f.run(goal, true, ['CC=missing-compiler']);
	for(const unit of units)
	{
		assert.equal(fs.existsSync(f.header(unit)), false);
		assert.equal(fs.existsSync(f.depfile(unit)), false);
	}
});

test('all 44 functions link from the native object alone and synchronous/async calls execute', t => {
	const f = fixture(t, true);
	f.run();
	// Opaque PHP types are sufficient to verify these C-to-JS call signatures.
	const declarations = '#include <emscripten.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <sys/types.h>\n'
		+ 'typedef uint32_t vrzno_target_id; typedef int zend_long; typedef void zval; typedef void zend_class_entry;\n'
		+ units.map(unit => `#include <${unit}_js.h>\n`).join('');
	const wrappers = entries.map(entry => {
		const args = entry.parameters.map(parameter => parameter.match(/\w+$/)[0]).join(', ');
		return `EMSCRIPTEN_KEEPALIVE ${entry.result} test_${entry.name}(${entry.parameters.join(', ') || 'void'}) { ${entry.result === 'void' ? '' : 'return '}${entry.name}(${args}); }\n`;
	}).join('');
	fs.writeFileSync(path.join(f.build, 'glue.c'), declarations + wrappers);
	const run = (command, args) => {
		const result = spawnSync(command, args, {cwd: f.build, encoding: 'utf8', timeout: 60000});
		assert.equal(result.status, 0, result.stdout + result.stderr);
		return result;
	};
	run(compiler, ['-I' + path.join(f.build, 'generated'), '-c', 'glue.c', '-o', 'glue.o']);
	fs.rmSync(f.source, {recursive: true});
	fs.rmSync(path.join(f.build, 'generated'), {recursive: true});
	fs.unlinkSync(path.join(f.build, 'glue.c'));
	run(compiler, ['glue.o', '--no-entry', '-o', 'bridge.mjs', '-sASYNCIFY=1', '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sENVIRONMENT=node', '-sEXPORTED_RUNTIME_METHODS=["ccall","getValue","setValue","UTF8ToString","lengthBytesUTF8","stringToUTF8","HEAPU8"]', '-sEXPORTED_FUNCTIONS=["_malloc","_free"]']);
	fs.writeFileSync(path.join(f.build, 'verify.mjs'), `
import assert from 'node:assert/strict';
import load from './bridge.mjs';
const module = await load();
module._test_vrzno_js_init();
const values = new Map();
module.jsToZval = (value, address) => values.set(address, value);
const id = module.targets.add({answer: 42});
const name = module._malloc(16);
module.stringToUTF8('answer', name, 16);
module._test_vrzno_js_property_read(id, name, 100);
assert.equal(values.get(100), 42);
assert.equal(module._test_vrzno_js_property_has(id, name, 2), 1);
assert.equal(module._test_vrzno_js_class_lookup(id), 0);
const promised = module.targets.add(Promise.resolve(43));
await module.ccall('test_vrzno_await_internal', null, ['number', 'number'], [promised, 104], {async: true});
assert.equal(values.get(104), 43);
const errors = [];
module.vrznoThrowRuntimeError = error => errors.push(error.message);
await module.ccall('test_vrzno_await_internal', null, ['number', 'number'], [module.targets.add(Promise.reject(new Error('expected rejection'))), 104], {async: true});
assert.deepEqual(errors, ['expected rejection']);
globalThis.fetch = async () => ({status: 200, statusText: 'OK', headers: new Map([['x-test', 'yes']]), arrayBuffer: async () => new Uint8Array([7, 0, 9]).buffer});
module.stringToUTF8('data:test', name, 16);
const pointers = module._malloc(8);
module.setValue(pointers, 0, '*');
module.setValue(pointers + 4, 0, 'i32');
const response = await module.ccall('test_php_stream_fetch_real_open', 'number', ['number', 'number', 'number', 'number', 'number'], [name, 0, 4, pointers, pointers + 4], {async: true});
assert.equal(module._test_vrzno_js_fetch_status(response), 200);
assert.equal(module._test_vrzno_js_fetch_read(response, name, 0, 16), 3);
assert.deepEqual(Array.from(module.HEAPU8.slice(name, name + 3)), [7, 0, 9]);
const headers = module.getValue(pointers, '*'), count = module.getValue(pointers + 4, 'i32');
for(let i = 0; i < count; i++) module._free(module.getValue(headers + i * 4, '*'));
module._free(headers);
module._free(pointers);
module._free(name);
module._test_vrzno_js_fetch_release(response);
module._test_vrzno_js_shutdown();
assert.equal(module.vrznoOwnershipStats().outstanding, 0);
assert.equal(module.targets.references.size, 1);
console.log('embedded bridge passed');
`);
	assert.match(run(process.execPath, ['verify.mjs']).stdout, /embedded bridge passed/);
});
