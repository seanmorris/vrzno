import { test } from 'node:test';
import { runLifecycleChild, lifecycleFailures } from './lib/lifecycle-process.mjs';

for(const [name, mode] of [
	['Callback collection releases ownership before cache cleanup', 'controlled-callback-owner-first'],
	['Callback collection releases ownership after cache cleanup', 'controlled-callback-cache-first'],
	['Explicit release cancels queued finalization exactly once', 'controlled-explicit-release'],
	['Iterators survive their factory being finalized first', 'controlled-factory-first'],
	['Factories survive iterator finalization and create replacement iterators', 'controlled-iterator-first'],
	['Shutdown cancels queued owner finalizers before runtime reuse', 'controlled-shutdown'],
])
{
	test(name, () => runLifecycleChild(mode));
}

test('Lifecycle checks reject a deliberately strong callback cache', () =>
	runLifecycleChild('negative-strong-cache', lifecycleFailures.strongCache));

test('Lifecycle checks reject disabled owned-value finalization', () =>
	runLifecycleChild('negative-disabled-finalizer', lifecycleFailures.disabledFinalizer));
