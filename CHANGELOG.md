# Changelog

All notable changes to Vrzno are documented in this file.

## Unreleased

- Add `vrzno_dbg_api_version()` (returns 2) so debugger hosts can detect the exports below without probing for each symbol.
- Add `vrzno_dbg_dump_frames()`: the stack from the innermost frame, whichever frame is selected, with each frame's file, line, function, class, and a flag for the selected frame. Frames are counted the way `vrzno_dbg_switch_frame()` counts them, so a frame's position is the index to switch to. `vrzno_dbg_dump_backtrace()` is unchanged.
- `vrzno_dbg_dump_symbols()` takes a scope instead of a bool: 0 for the current frame's variables and 1 for superglobals, as before, and 2 for every global variable, user-defined ones included.
- Add `vrzno_dbg_zval_type()`, `vrzno_dbg_zval_class()`, `vrzno_dbg_zval_count()`, and `vrzno_dbg_zval_is_reference()` so a debugger can show a value's PHP type, tell arrays from objects, and recognise a variable that has not been assigned yet.

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
