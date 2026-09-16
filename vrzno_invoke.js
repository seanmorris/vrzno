/**
 * Calls a JS function with converted PHP arguments. JavaScript errors are forwarded to
 * PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_invoke
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} valuesAddress Address of contiguous, caller-owned zval arguments.
 * @param {number} argumentCount Number of PHP arguments to convert.
 * @param {number} elementSize Size of each native argument element in bytes.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
try
{
	const target = Module.targets.get(targetId);
	const argv = valuesAddress;
	const argc = argumentCount;
	const size = elementSize;
	const args = [];

	for(let i = 0; i < argc; i++)
	{
		args.push(Module.zvalToJS(argv + i * size));
	}

	Module.jsToZval(target(...args), resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
