/**
 * Awaits a JavaScript value and converts the result into PHP after Asyncify resumes.
 * Rejections are forwarded as PHP exceptions.
 *
 * @function vrzno_await_internal
 * @async
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} rv Address of the caller-owned result zval, initialized to PHP null
 * before waiting.
 * @returns {Promise<void>}
 */
try
{
	const target = Module.targets.get(targetId);
	const result = await target;
	Module.jsToZval(result, rv);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
