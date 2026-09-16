/**
 * Copies buffered response bytes into PHP stream storage without making another request.
 *
 * @function vrzno_js_fetch_read
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @param {number} bufferAddress Address of the writable destination byte buffer.
 * @param {number} offset Byte offset within the buffered response.
 * @param {number} requestedCount Maximum bytes to copy into the destination.
 * @returns {number} Bytes copied, or zero at EOF or for an HTTP error that is not
 * ignored.
 */
const target = Module.targets.get(targetId);
const dest = bufferAddress;
const fpos = offset;
let count = requestedCount;

if(target.status >= 400 && !target.context.ignoreErrors)
{
	return 0;
}

if(fpos >= target.buffer.length)
{
	count = 0;
}
else if(fpos + count > target.buffer.length)
{
	count = target.buffer.length - fpos;
}

if(count)
{
	Module.HEAPU8.set(target.buffer.slice(fpos, fpos + count), dest);
}

return count;
