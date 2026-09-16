# Using weakermap in Vrzno

Vrzno embeds the published [`weakermap@0.0.13`](https://www.npmjs.com/package/weakermap/v/0.0.13)
package from [seanmorris/Weaker](https://github.com/seanmorris/Weaker), replacing
the duplicated cache class and registry helper. `package-lock.json` pins the
package and esbuild with integrity hashes. Make installs those build dependencies
under `generated/npm`, bundles the adapter, and includes the result in the native
object. Runtime consumers need no npm installation, package import, or network access.

## Runtime requirements

`WeakRef` and `FinalizationRegistry` are required. Initialization checks both
before setting up the bridge and reports the Cloudflare compatibility flag when
either is missing. There are no strong-reference or no-op finalizer fallbacks.

Both APIs have been available across major browsers since April 2021:
Chrome 84, Firefox 79, Safari 14.1, and Node 14.6 support both
([WeakRef data](https://github.com/mdn/browser-compat-data/blob/main/javascript/builtins/WeakRef.json),
[FinalizationRegistry data](https://github.com/mdn/browser-compat-data/blob/main/javascript/builtins/FinalizationRegistry.json)).
php-wasm's Emscripten 6.0.6 defaults already target Chrome 85, Firefox 79,
Safari 15, and Node 18.3, so this adds no version restriction to standard builds.

Cloudflare enables both APIs from compatibility date `2025-05-05`. With an older
date, add `compatibility_flags = ["enable_weak_ref"]` to Wrangler configuration
([Cloudflare documentation](https://developers.cloudflare.com/workers/configuration/compatibility-flags/#enable-finalizationregistry-and-weakref)).
php-wasm's Pages configuration and workerd tests explicitly enable that flag.

## Preserved contracts

`vrzno_weakermap.mjs` is a small adapter preserving Vrzno's `keys()` and `values()`
snapshot arrays and its existing entry-iterator shape. The npm package supplies
the cache storage, weak references, finalization, replacement handling, and pruning.
`Module.WeakerMap` can still be supplied by callers.

Initialization captures the runtime's weak-reference constructors. The bundle
shares that lexical scope, so later changes to host globals cannot mix different
collectors in one runtime. This also preserves the controlled lifecycle tests.

`vrzno_targets.js` still manages stable numeric IDs and explicit PHP-side reference
counts. `vrzno_ownership.js` still owns copied PHP zvals, destroys each exactly once,
and releases outstanding owners at shutdown without depending on GC timing.
A weak-value cache does not replace either contract.

## Existing package limitation

The package uses the value as the finalizer unregister token, as the former
embedded implementation did. Storing one object under two keys and deleting one
key unregisters cleanup for both. The remaining dead entry is pruned on lookup
or iteration. Vrzno's caches use one identity key per wrapper. Distinct per-entry
tokens remain an upstream improvement; adoption does not change this behavior.

## Verification

```sh
npm ci
npm run lint
npm run test:weakermap
npm test # requires Emscripten 6.0.6 on PATH
```

Compatibility checks exercise the actual bundle and published package, controlled
late finalization, snapshot methods, scoped constructors, and missing-API errors.
Make tests verify locked dependency installation, incremental rebuilds, recovery,
separate build directories, and linking after all JS sources are removed.
The integration suite separately checks startup rejection and requires real
garbage collection for PHP ownership and callback/iterator lifetimes.
