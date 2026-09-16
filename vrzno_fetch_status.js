/**
 * Reads the status of a buffered fetch response.
 *
 * @function vrzno_js_fetch_status
 * @param {number} targetId Registered JavaScript target handle; this is not a Wasm
 * address.
 * @returns {number} HTTP status, or -1 when fetch rejected.
 */
{
	const parsed = Module.targets.get(targetId);
	return parsed.status;
}
