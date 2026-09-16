/**
 * Creates and strongly retains an options object for the next fetch call.
 *
 * @function vrzno_js_fetch_context
 * @returns {number} New options handle, consumed by php_stream_fetch_real_open().
 */
const context = {};
Module.tacked.add(context);
return Module.targets.add(context);
