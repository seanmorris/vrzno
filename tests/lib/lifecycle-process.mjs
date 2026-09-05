import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { strict as assert } from 'node:assert';

const childPath = fileURLToPath(new URL('./lifecycle-child.mjs', import.meta.url));
const resultPrefix = 'VRZNO_LIFECYCLE_RESULT ';

export const lifecycleFailures = {
	strongCache: 'Re-export after collection must create a fresh wrapper',
	disabledFinalizer: 'Collected callback must release its PHP owner',
};

export async function runLifecycleChild(mode, expectedFailure)
{
	const result = await new Promise((resolve, reject) => {
		const child = spawn(process.execPath, ['--expose-gc', childPath, mode], {
			env: process.env,
			stdio: ['ignore', 'pipe', 'pipe'],
		});
		let stdout = '';
		let stderr = '';
		let timedOut = false;
		const timer = setTimeout(() => {
			timedOut = true;
			child.kill('SIGKILL');
		}, 20_000);
		child.stdout.on('data', chunk => { stdout += chunk; });
		child.stderr.on('data', chunk => { stderr += chunk; });
		child.once('error', error => {
			clearTimeout(timer);
			reject(error);
		});
		child.once('close', (code, signal) => {
			clearTimeout(timer);
			resolve({code, signal, stdout, stderr, timedOut});
		});
	});

	const diagnostic = `${mode}: exit=${result.code}, signal=${result.signal}\n${result.stdout}\n${result.stderr}`;
	assert.equal(result.timedOut, false, `Lifecycle child exceeded 20 seconds: ${diagnostic}`);
	const lines = result.stdout.split('\n').filter(line => line.startsWith(resultPrefix));
	assert.equal(lines.length, 1, `Lifecycle child did not report exactly one result: ${diagnostic}`);
	const report = JSON.parse(lines[0].slice(resultPrefix.length));
	assert.equal(report.mode, mode, diagnostic);

	if(expectedFailure)
	{
		assert.equal(result.code, 1, `Negative control unexpectedly passed: ${diagnostic}`);
		assert.equal(report.kind, 'assertion', diagnostic);
		assert.equal(report.message, expectedFailure, diagnostic);
	}
	else
	{
		assert.equal(result.code, 0, diagnostic);
		assert.equal(report.ok, true, diagnostic);
	}

	return report;
}
