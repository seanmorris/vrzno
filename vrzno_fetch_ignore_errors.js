/**
 * Stores the PHP ignore_errors option on the request context.
 *
 * @function vrzno_js_fetch_ignore_errors
 * @param {number} contextId Handle of the retained fetch-options object.
 * @param {number} ignoreErrorsArgument Whether the stream should accept HTTP error
 * responses.
 * @returns {void}
 */
{
	const context = Module.targets.get(contextId);
	context.ignoreErrors = ignoreErrorsArgument;
}
