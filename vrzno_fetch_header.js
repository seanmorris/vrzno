/**
 * Parses one header line, ignoring lines without a nonempty name before the colon.
 *
 * @function vrzno_js_fetch_header
 * @param {number} contextId Handle of the retained fetch-options object.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @returns {void}
 */
(() => {
						const context = Module.targets.get(contextId);
						const headerLine = UTF8ToString(nameAddress);
						const colon = headerLine.indexOf(':');
						if(colon < 1) return;

						const key = headerLine.substr(0, colon).trim();
						const val = headerLine.substr(1 + colon).trim();

						context.headers = context.headers ?? {};
						context.headers[key] = val;
})();
