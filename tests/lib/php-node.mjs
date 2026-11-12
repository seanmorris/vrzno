import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const phpWasmRoot = resolve(process.env.PHP_WASM_ROOT ?? '../php-wasm');
const { PhpNode: BasePhpNode } = await import(pathToFileURL(
	resolve(phpWasmRoot, 'packages/php-wasm/PhpNode.mjs')
));
const { nodeRuntimeOptions } = await import(pathToFileURL(
	resolve(phpWasmRoot, 'test/lib/node-runtime-options.mjs')
));

export class PhpNode extends BasePhpNode
{
	constructor(args = {})
	{
		// Naming the runtime lets php-wasm's helper add Vrzno's side module when the
		// runtime under test was built without Vrzno linked in.
		super(nodeRuntimeOptions({runtime: 'php', ...args}));
	}
}

export const capture = php => {
	let stdout = '';
	let stderr = '';
	php.addEventListener('output', event => event.detail.forEach(line => void (stdout += line)));
	php.addEventListener('error', event => event.detail.forEach(line => void (stderr += line)));
	return {
		get stdout() { return stdout; }
		, get stderr() { return stderr; }
	};
};
