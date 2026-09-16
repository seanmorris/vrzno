/**
 * Serializes enumerable target properties for PHP inspection; functions and
 * unserializable targets produce an empty object.
 *
 * @function vrzno_js_properties_json
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @returns {number} Address of allocated NUL-terminated UTF-8 JSON; C frees it with
 * free().
 */
const target = Module.targets.get(targetId);
let json;

if(typeof target === 'function')
{
	json = JSON.stringify({});
}
else
{
	try
	{ json = JSON.stringify({...target}); }
	catch
	{ json = JSON.stringify({}); }
}

const str = String(json);
const len = 1 + lengthBytesUTF8(str);
const loc = _malloc(len);

stringToUTF8(str, loc, len);

return loc;
