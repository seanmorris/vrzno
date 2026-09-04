# Changelog

All notable changes to Vrzno are documented in this file.

## 0.2.0 - 2026-09-04

- Define native null semantics: JavaScript `null` and `undefined` become PHP `null`, while PHP `null` becomes JavaScript `null`.
- Correct PHP `isset()`, `empty()`, and `property_exists()` behavior for bridged values.
- Add string-keyed, indexed, appended, and unset dimension writes without unsafe scalar type access.
- Preserve embedded null bytes in strings moving in either direction.
- Convert non-finite and out-of-wasm32-range JavaScript numbers to PHP floats; reject BigInt and Symbol values with `TypeError`.
- Convert JavaScript exceptions and rejected promises into catchable PHP `RuntimeException` instances.
- Make bridged values non-cloneable and non-serializable and invalidate JavaScript proxies when their PHP runtime is refreshed.
- Repair callback binding, return-value, fetch-stream, iterator, target-handle, and response-header ownership.
- Replace pointer-shaped JavaScript target IDs with explicit 32-bit handles.
- Split the extension into normal C translation units and generate arginfo from `vrzno.stub.php`.
- Replace stale and network-dependent coverage with deterministic bridge, lifecycle, error, and local fetch tests.
- Add an Apache-2.0 license, package metadata, and oldest/newest supported PHP CI coverage.

The legacy `vrzno_eval()`, `vrzno_run()`, and `vrzno_timeout()` helpers remain supported without deprecation warnings.

## 0.1.0

- Initial public release.
