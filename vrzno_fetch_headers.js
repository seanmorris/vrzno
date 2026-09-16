/**
 * Parses newline-separated request headers and strips carriage returns.
 *
 * @function vrzno_js_fetch_headers
 * @param {number} contextId Handle of the retained fetch-options object.
 * @param {number} nameAddress Wasm byte address of a NUL-terminated UTF-8 name.
 * @returns {void}
 */
(() => {
				const context = Module.targets.get(contextId);
				const headerLines = UTF8ToString(nameAddress);

				headerLines.split(String.fromCharCode(10)).forEach(headerLine => {
					headerLine = headerLine.replace(String.fromCharCode(13), String());
					const colon = headerLine.indexOf(':');
					if(colon < 1) return;

					const key = headerLine.substr(0, colon).trim();
					const val = headerLine.substr(1 + colon).trim();

					context.headers = context.headers ?? {};
					context.headers[key] = val;
				});
})();
