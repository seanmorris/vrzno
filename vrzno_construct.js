/**
 * Constructs and strongly retains a JS instance using the constructor cached for the PHP
 * class. JavaScript errors are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_construct
 * @param {number} classAddress Wasm address of the PHP class entry.
 * @param {number} valuesAddress Address of contiguous, caller-owned zval arguments.
 * @param {number} argumentCount Number of PHP arguments to convert.
 * @param {number} elementSize Size of each native argument element in bytes.
 * @returns {number} Retained instance handle, or zero after a forwarded exception.
 */
try
{
	const constructor = Module._classes.get(classAddress);
	const argv = valuesAddress;
	const argc = argumentCount;
	const size = elementSize;
	const args = [];

	for(let i = 0; i < argc; i++)
	{
		args.push(Module.zvalToJS(argv + i * size));
	}

	const instance = new constructor(...args);
	const index = Module.targets.add(instance);
	Module.tacked.add(instance);

	return index;
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
	return 0;
}
