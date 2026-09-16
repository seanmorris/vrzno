import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import {fileURLToPath} from 'node:url';
import {bundleWeakermap} from '../../vrzno_bundle.mjs';

export const root = fileURLToPath(new URL('../../', import.meta.url));
export const templates = fs.readdirSync(root).filter(name => name.endsWith('_js.h.in')).sort();
const cacheBundle = await bundleWeakermap(root, path.join(root, 'node_modules'));

/** Read a bridge input, generating the npm bundle with the same builder as Make. */
export function inputBody(name, directory = root)
{
	return name === 'vrzno_weakermap.js' ? cacheBundle : fs.readFileSync(path.join(directory, name), 'utf8');
}

/** Read the native declarations and their ordered JS includes. */
export function declarations(directory = root)
{
	return templates.flatMap(template => {
		const source = fs.readFileSync(path.join(directory, template), 'utf8');
		return [...source.matchAll(/EM_(ASYNC_)?JS\(([^,]+), (\w+), \(([^)]*)\), \{\n([\s\S]*?)\n\}\);/g)].map(match => ({
			template
			, async: Boolean(match[1])
			, result: match[2]
			, name: match[3]
			, parameters: match[4] === 'void' ? [] : match[4].split(',').map(value => value.trim())
			, inputs: [...match[5].matchAll(/^#include "([^"]+)"$/gm)].map(include => include[1])
		}));
	});
}

/** Load the same bodies used by Make into an isolated module with a small Wasm heap. */
export function bridge(overrides = {})
{
	const heap = new Uint8Array(65536);
	const view = new DataView(heap.buffer);
	const calls = [], freed = [], timers = [];
	let next = 256;
	const malloc = bytes => {
		const address = next;
		next = (next + bytes + 7) & ~7;
		if(next > heap.length) throw new RangeError('Fixture heap exhausted');
		return address;
	};
	const encoder = new TextEncoder(), decoder = new TextDecoder();
	const readString = (address, max = heap.length - address) => {
		let end = address;
		while(end < address + max && heap[end]) end++;
		return decoder.decode(heap.subarray(address, end));
	};
	const writeString = (value, address, length) => {
		const bytes = encoder.encode(value).subarray(0, length - 1);
		heap.set(bytes, address);
		heap[address + bytes.length] = 0;
	};
	const string = value => {
		const size = encoder.encode(value).length + 1;
		const address = malloc(size);
		writeString(value, address, size);
		return address;
	};
	const module = {
		HEAPU8: heap
		, shared: {}
		, getValue: address => view.getUint32(address, true)
		, ccall: (name, result, types, args) => { calls.push({name, args}); }
		, ...overrides.Module
	};
	const context = vm.createContext({
		Module: module
		, WeakRef
		, FinalizationRegistry
		, ArrayBuffer
		, Uint8Array
		, TextEncoder
		, console
		, HEAPU8: heap
		, _malloc: malloc
		, _free: address => freed.push(address)
		, lengthBytesUTF8: value => encoder.encode(value).length
		, UTF8ToString: readString
		, UTF8ArrayToString: (buffer, address, length) => decoder.decode(buffer.subarray(address, address + length))
		, stringToUTF8: writeString
		, getValue: module.getValue
		, setValue: (address, value) => view.setUint32(address, value, true)
		, setTimeout: callback => { timers.push(callback); }
		, ...overrides.globals
	});
	const functions = Object.fromEntries(declarations().map(entry => {
		const body = entry.inputs.map(name => inputBody(name)).join('\n');
		const args = entry.parameters.map(parameter => parameter.match(/\w+$/)[0]);
		return [entry.name, vm.runInContext(`(${entry.async ? 'async ' : ''}function(${args.join(',')}) {\n${body}\n})`, context)];
	}));
	functions.vrzno_js_init();
	return {module, functions, context, heap, view, malloc, string, readString, calls, freed, timers};
}
