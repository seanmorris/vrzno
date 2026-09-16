/**
 * Calls a named JS method with its original receiver and PHP arguments. JavaScript
 * errors are forwarded to PHP through vrznoThrowRuntimeError().
 *
 * @function vrzno_js_method_call
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @param {number} argumentsAddress Address of an array of borrowed zval pointers.
 * @param {number} argumentCount Number of PHP arguments to convert.
 * @param {number} elementSize Size of each native argument element in bytes.
 * @param {number} resultAddress Address of a caller-owned zval receiving the converted
 * value.
 * @returns {void}
 */
try
{
	const target = Module.targets.get(targetId);
	const methodName = UTF8ToString(nameAddress);
	const argp = argumentsAddress;
	const argc = argumentCount;
	const size = elementSize;
	const args = [];

	for(let i = 0; i < argc; i++)
	{
		const loc = argp + i * size;
		const ptr = Module.getValue(loc, '*');
		args.push(Module.zvalToJS(ptr));
	}

	Module.jsToZval(target[methodName](...args), resultAddress);
}
catch(error)
{
	Module.vrznoThrowRuntimeError(error);
}
