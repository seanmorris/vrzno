/**
 * @typedef {number} WasmAddress
 * Byte address in the supported wasm32 linear memory.
 */

/**
 * @typedef {number} VrznoTargetId
 * Reference-counted JS target handle, separate from Wasm memory addresses.
 */

/**
 * @typedef {object} VrznoOwnershipStats
 * @property {number} allocations Owned zvals registered in this module instance.
 * @property {number} releases Owned zvals destroyed so far.
 * @property {number} outstanding Allocations minus releases.
 * @property {number} targets Target handles with recorded reference counts.
 */

/**
 * @typedef {object} VrznoReference
 * @property {function(): (object|Function|undefined)} deref Retrieves a live value.
 */

/**
 * Initializes the JavaScript bridge during PHP module startup. The ordered includes in
 * vrzno_js.h.in share this single function scope.
 *
 * @function vrzno_js_init
 * @returns {void}
 */
Module.hasVrzno = true;

const IS_UNDEF    = 0;
const IS_NULL     = 1;
const IS_FALSE    = 2;
const IS_TRUE     = 3;
const IS_LONG     = 4;
const IS_DOUBLE   = 5;
const IS_STRING   = 6;
const IS_ARRAY    = 7;
const IS_OBJECT   = 8;
const IS_RESOURCE = 9;

const IS_INDIRECT = 12;

Module.vrznoGeneration = (Module.vrznoGeneration || 0) + 1;
Module.tacked = new Set;
/**
 * Returns a cached byte view for an ArrayBuffer and leaves other values unchanged.
 *
 * @param {*} target Value being indexed.
 * @returns {*} Cached Uint8Array or the original value.
 */
Module.vrznoArrayView = target => {
	if(target instanceof ArrayBuffer)
	{
		if(!Module.bufferMaps.has(target))
		{
			Module.bufferMaps.set(target, new Uint8Array(target));
		}

		return Module.bufferMaps.get(target);
	}

	return target;
};
/**
 * Applies PHP truthiness to JS scalar property values, including the special string
 * "0".
 *
 * @param {*} value Property value being checked.
 * @returns {boolean} Whether the value satisfies the bridge's nonempty check.
 */
Module.vrznoPhpTruthy = value => !(
	value === undefined
	|| value === null
	|| value === false
	|| value === 0
	|| (typeof value === 'string' && value.length === 0)
	|| value === '0'
);
/**
 * Recognizes canonical signed 32-bit integer strings as PHP numeric keys. Leading
 * zeros, noncanonical spellings, and other key types are preserved.
 *
 * @param {*} key Property key supplied to a PHP-array proxy.
 * @returns {*} Numeric key when representable, otherwise the unchanged key.
 */
Module.vrznoNormalizeArrayKey = key => {
	if(typeof key !== 'string')
	{
		return key;
	}

	const numericKey = Number(key);
	return Number.isInteger(numericKey)
		&& numericKey >= -2147483648
		&& numericKey <= 2147483647
		&& String(numericKey) === key
		? numericKey
		: key;
};

const origZval = Symbol('origZval');
const proxyGeneration = Symbol('vrznoGeneration');

/**
 * Uses native finalization when available. The fallback does no GC cleanup; request
 * shutdown still releases every owned zval explicitly.
 */
const _FinalizationRegistry = globalThis.FinalizationRegistry || class { // Polyfill for cloudflare
	register(){};
	unregister(){};
};
