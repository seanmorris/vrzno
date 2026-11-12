# Changelog

All notable changes to Vrzno are documented in this file.

## Unreleased

- Add `vrzno_dbg_api_version()` (returns 2) so debugger hosts can detect the exports below without probing for each symbol.
- Add `vrzno_dbg_dump_frames()`: the stack from the innermost frame, whichever frame is selected, with each frame's file, line, function, class, and a flag for the selected frame. Frames are counted the way `vrzno_dbg_switch_frame()` counts them, so a frame's position is the index to switch to. `vrzno_dbg_dump_backtrace()` is unchanged.
- `vrzno_dbg_switch_frame()` now skips internal function frames by function type. It tested the frame's opline for NULL, which PHP never sets on internal frames, so whether one was counted depended on leftover stack memory.
- Both count from the innermost frame that runs PHP code. When a host asks while PHP is inside an internal function, such as one awaiting a promise, frame 0 is that function's caller, whose line and variables the other exports already report.
- `vrzno_dbg_dump_symbols()` takes a scope instead of a bool: 0 for the current frame's variables and 1 for superglobals, as before, and 2 for every global variable, user-defined ones included.
- Add `vrzno_dbg_zval_child()` to find an array element or object property by key, so the helpers below also work for values reached through a parent.
- Add `vrzno_dbg_zval_type()`, `vrzno_dbg_zval_class()`, `vrzno_dbg_zval_count()`, and `vrzno_dbg_zval_is_reference()` so a debugger can show a value's PHP type, tell arrays from objects, and recognise a variable that has not been assigned yet.
- Add `vrzno_dbg_zval_class_length()`. An anonymous class is named `class@anonymous`, a NUL byte, then the file and line that declare it, so a host that reads `vrzno_dbg_zval_class()` as a C string sees the same name for every anonymous class.
- Encode JSON through PHP's exported `php_json_encode_ex()` instead of the internal `php_json_encode_zval()`, and put the script's `json_last_error()` state back afterwards. A Vrzno built as a side module cannot bind the internal function: listing a PHP object's keys from JavaScript failed there with `resolved is not a function`, and array keys and `vrzno_run()` arguments go through the same call.

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
