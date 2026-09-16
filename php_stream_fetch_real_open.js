/**
 * Fetches and buffers the complete response. Allocates UTF-8 header strings for C and
 * retains the response until the stream releases it. Rejections become response objects
 * with status -1.
 *
 * @function php_stream_fetch_real_open
 * @async
 * @param {number} path Address of the NUL-terminated UTF-8 request URL.
 * @param {number} context_id Retained fetch-options handle, or zero for default options;
 * consumed on success or failure.
 * @param {number} ptrsize Size of a Wasm pointer in bytes; four for the supported wasm32
 * build.
 * @param {number} headersv Address receiving the allocated header-pointer array; C frees
 * every string and the array.
 * @param {number} headersc Address receiving the number of allocated header strings on
 * success.
 * @returns {Promise<number>} Handle of the retained response or error object after
 * Asyncify resumes C.
 */
const pathString = UTF8ToString(path);
const context = Module.targets.get(context_id) || {ignoreErrors: false};

try
{
	const response = await fetch(pathString, context);
	const buffer = new Uint8Array( await response.arrayBuffer() );
	const status = response.status;

	const headerLines = [...response.headers.entries()].map(([key, val]) => `${key}: ${val}`);
	headerLines.unshift(`HTTP/1.1 ${response.status} ${response.statusText}`);

	const headersloc = _malloc(ptrsize * headerLines.length); // free()'d in php_stream_fetch_open
	setValue(headersv, headersloc, '*');
	setValue(headersc, headerLines.length, 'i32');

	let i = 0;
	for(const line of headerLines)
	{
		const len = lengthBytesUTF8(line) + 1;
		const loc = _malloc(len); // free()'d in php_stream_fetch_open
		stringToUTF8(line, loc, len);
		setValue(headersloc + (i * ptrsize), loc, 'i' + (8 * ptrsize));
		i++;
	}

	const parsed = {status, buffer, context};
	Module.tacked.add(parsed);
	if(context_id)
	{
		Module.targets.remove(context_id);
	}
	return Module.targets.add(parsed);
}
catch(error)
{
	const message = error && error.message ? error.message : String(error);
	const parsed = {status: -1, buffer: new TextEncoder().encode(message), context, error: message};
	Module.tacked.add(parsed);
	if(context_id)
	{
		Module.targets.remove(context_id);
	}
	return Module.targets.add(parsed);
}
