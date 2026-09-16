/**
 * Copies request-body bytes out of Wasm memory so the async fetch owns its snapshot.
 *
 * @function vrzno_js_fetch_body
 * @param {number} contextId Handle of the retained fetch-options object.
 * @param {number} bodyAddress Address of borrowed binary request-body bytes.
 * @param {number} bodyLength Body length in bytes, including any embedded NUL bytes.
 * @returns {void}
 */
(() => {
			const context = Module.targets.get(contextId);
			context.body = Module.HEAPU8.slice(bodyAddress, bodyAddress + bodyLength);
})();
