# Evaluating weakermap for Vrzno

Evaluated the published [`weakermap@0.0.13`](https://www.npmjs.com/package/weakermap/v/0.0.13)
package from [seanmorris/Weaker](https://github.com/seanmorris/Weaker). The lockfile
pins the exact tarball as a development dependency for repeatable compatibility
checks. It is not part of the native build or shipped PHP runtime.

The package can provide the weak-value cache operations Vrzno uses. Adopting it
needs a scoped compatibility adapter and build-time ESM bundling. This extraction
keeps the existing implementation and records the required follow-up here.

## Coverage and differences

| Behavior | Embedded cache | weakermap 0.0.13 |
| --- | --- | --- |
| Primitive keys with object/function values | Supported | Supported |
| Get, set, has, delete, clear, and entry enumeration | Pass | Pass |
| Late finalizers after replacement or clear | Preserve new entries | Preserve new entries |
| Missing `WeakRef` | Strong-reference fallback | Throws when storing a value |
| Missing `FinalizationRegistry` | No-op fallback; shutdown releases PHP owners | Throws during construction |
| `keys()` and `values()` | Snapshot arrays | Iterators |
| Iterator returned by `[Symbol.iterator]()` | Has `next()` | Also iterable itself |
| Weak-reference constructors | Captured during module initialization | Resolved from globals when used |

The internal cache call sites use the common operations. The iterator differences
still matter because `Module.WeakerMap` is exposed and can be supplied by a caller.
Capturing constructors also supports the controlled lifecycle tests, which restore
the host globals after initializing a runtime.

## Compatibility cost

The native APIs have been available across major browsers since April 2021:
Chrome 84, Firefox 79, Safari 14.1, and Node 14.6 support both
([WeakRef data](https://github.com/mdn/browser-compat-data/blob/main/javascript/builtins/WeakRef.json),
[FinalizationRegistry data](https://github.com/mdn/browser-compat-data/blob/main/javascript/builtins/FinalizationRegistry.json)).
php-wasm's Emscripten 6.0.6 defaults already target Chrome 85, Firefox 79,
Safari 15, and Node 18.3. Requiring these APIs adds no browser or Node version
restriction to those standard builds.

Cloudflare enables both APIs by default from compatibility date `2025-05-05`;
older dates can opt in with `enable_weak_ref`
([Cloudflare documentation](https://developers.cloudflare.com/workers/configuration/compatibility-flags/#enable-finalizationregistry-and-weakref)).
At evaluation, php-wasm's Pages configuration uses `2024-12-01` and its workerd
tests use `2024-02-01`. A local workerd probe confirms both APIs are undefined
with the Pages date, and available with the flag or the newer default date.

Preserving those existing configurations requires the fallback adapter. An
explicit native-API requirement is also practical if Cloudflare users are told
to enable the flag; only older/custom builds and older compatibility settings
would lose support. The fallback is a compatibility choice, not a broad obstacle
to package adoption.

## Unregister tokens

Both implementations use the value as the finalizer unregister token. Registering
one object under two keys and then deleting one key unregisters cleanup for both.
The remaining dead entry is pruned on lookup or iteration. The package therefore
does not itself fix that limitation. Vrzno's current caches use one identity key
per wrapper; a future general-purpose cache migration should use a distinct token
per entry.

## What can be consolidated

`vrzno_weakermap.js` contains the duplicated cache class and registry helper. The
npm package can replace that implementation once the differences above are handled.
Its published JS source is 3,076 bytes, with no dependencies; the tarball also
contains documentation and license files. Removing duplication would mainly reduce
maintenance work, since the runtime still needs the same cache implementation.

`vrzno_targets.js` also manages stable numeric IDs and PHP-side reference counts.
`vrzno_ownership.js` owns copied PHP zvals, invokes native destructors exactly once,
and releases every outstanding owner during shutdown. A weak-value map does not
replace either contract. Removing these layers would regress callback identity,
detached iterator lifetimes, or cleanup when finalization is unavailable.

## Follow-up implementation

1. Provide scoped weak-reference/finalization constructors to the package, through
   an upstream factory/options API or a build wrapper. Preserve the fallback modes
   without installing shims on `globalThis`.
2. Preserve Vrzno's observable snapshot methods in a small adapter, or explicitly
   document and test an API change. Address per-entry unregister tokens upstream.
3. Bundle the ESM dependency during Make's preparation stage, then include its
   generated JS before Emscripten compilation. Pin the dependency and bundler,
   preserve license notices, and keep runtime loading independent of npm/network.
4. Run these compatibility checks, the full controlled/native-GC lifecycle suite,
   PHP 8.0/8.5 integration tests, and Cloudflare artifact tests against the adapter.

## Reproduce

```sh
npm ci
npm run test:weakermap
```

The nine checks in `tests/weakermap.test.mjs` compare both implementations, simulate
late finalization, and make the missing-API and iterator differences explicit.
They do not depend on garbage-collector timing. The separate integration suite
continues to require real GC coverage as well.
