import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';

/**
 * Bundles the pinned npm cache and Vrzno adapter for inclusion in an EM_JS body.
 * Make installs dependencies in the build directory; tests use their npm install.
 *
 * @param {string} sourceDirectory Directory containing vrzno_weakermap.mjs.
 * @param {string} dependencies Directory containing the locked node_modules.
 * @returns {Promise<string>} Self-contained JS with no runtime imports.
 */
export async function bundleWeakermap(sourceDirectory, dependencies)
{
	const {build} = await import(pathToFileURL(path.resolve(dependencies, 'esbuild/lib/main.js')));
	const result = await build({
		entryPoints: [path.resolve(sourceDirectory, 'vrzno_weakermap.mjs')]
		, alias: {weakermap: path.resolve(dependencies, 'weakermap/WeakerMap.mjs')}
		, bundle: true
		, format: 'iife'
		, platform: 'neutral'
		, target: ['chrome85', 'firefox79', 'safari15', 'node18.3']
		, legalComments: 'inline'
		, write: false
	});
	return result.outputFiles[0].text;
}

if(process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href)
{
	const [source, dependencies, output] = process.argv.slice(2);
	await fs.writeFile(output, await bundleWeakermap(source, dependencies));
}
