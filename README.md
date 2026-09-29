# vrzno

JavaScript bridge for PHP running under Emscripten.

Vrzno (/vərəˈzɑːnoʊ/, vər-ə-ZAH-noh) lets PHP access JavaScript objects,
functions, constructors, and promises through runtime handles. Vrzno 0.2 supports
PHP 8.0 through 8.5 on Emscripten's wasm32 memory model.

The private `vrzno-bridge` npm package supplies this repository's build and test
tools.

![Vrzno banner](banner.jpg)

## Table of Contents

- [Install](#install)
- [Usage](#usage)
- [Building](#building)
- [API](#api)
	- [Objects and arrays](#objects-and-arrays)
	- [Injected values, promises, and modules](#injected-values-promises-and-modules)
	- [Callbacks and lifetime](#callbacks-and-lifetime)
	- [Value conversion](#value-conversion)
	- [HTTP streams](#http-streams)
	- [Compatibility helpers](#compatibility-helpers)
	- [Limits](#limits)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [License](#license)

## Install

Standard [php-wasm](https://github.com/seanmorris/php-wasm) builds include Vrzno.
php-wasm 0.2.0 and newer ship the Vrzno 0.2 bridge described here:

```sh
npm install php-wasm@^0.2.0
```

This README describes the current sources. To use a revision newer than the
published runtime, build php-wasm as described in [Building](#building) or obtain
matching artifacts from
[php-wasm CI](https://github.com/seanmorris/php-wasm/actions/workflows/build.yaml),
then install the generated package directory:

```sh
npm install /absolute/path/to/php-wasm/packages/php-wasm
```

Keep its JavaScript, Wasm, and support files together. Vrzno is compiled into
PHP; installing extension sources alone does not add it to an existing Wasm binary.

### Dependencies

The JavaScript host must provide `WeakRef` and `FinalizationRegistry`.
Initialization fails with an explanatory error when either is missing. Source
builds require wasm32 and Asyncify; native PHP and wasm64 builds are unsupported.

Cloudflare Workers need compatibility date `2025-05-05` or newer, or
`compatibility_flags = ["enable_weak_ref"]`. See the
[Cloudflare flag documentation](https://developers.cloudflare.com/workers/configuration/compatibility-flags/#enable-finalizationregistry-and-weakref)
and php-wasm's [Cloudflare build guide](https://github.com/seanmorris/php-wasm/blob/develop/CLOUDFLARE.md).

## Usage

Save this as `example.mjs` and run it with `node example.mjs`:

```js
import { PhpNode } from 'php-wasm/PhpNode.mjs';

const php = new PhpNode({
	version: '8.4'
	, answer: 42
	, shared: {greeting: 'Hello from JavaScript'}
});

php.addEventListener('output', event => console.log(event.detail.join('')));
php.addEventListener('error', event => console.error(event.detail.join('')));

const exitCode = await php.run(`<?php
	$js = new Vrzno;
	$greeting = vrzno_shared('greeting');
	$promise = $js->Promise->resolve($greeting);

	echo vrzno_env('answer'), ': ', vrzno_await($promise);
`);

if(exitCode !== 0)
{
	throw new Error('PHP execution failed');
}
```

The output is `42: Hello from JavaScript`. The `answer` option is read through
`vrzno_env()`, while `shared.greeting` is read through `vrzno_shared()`.

For a browser application that resolves npm imports, use `PhpWeb` from
`php-wasm/PhpWeb.mjs` with the same options and PHP code. The browser runtime is
built with `web-mjs`; the Node runtime uses `node-mjs`.

## Building

From a php-wasm checkout with its dependencies and builder image available,
build a Node runtime using local Vrzno sources:

```sh
npm ci
make node-mjs PHP_VERSION=8.4 WITH_VRZNO=1 \
  VRZNO_DEV_PATH=/absolute/path/to/vrzno
```

Use `make image` to build the builder image when needed, and `web-mjs` for the
browser runtime. Outputs go into `packages/php-wasm/`. Omit `VRZNO_DEV_PATH` to
use php-wasm's pinned upstream revision.

| Setting | Effect |
| --- | --- |
| `WITH_VRZNO=1` | Enables the extension; the standard build default. |
| `VRZNO_REPOSITORY` | Selects the upstream source repository. |
| `VRZNO_REF` | Selects its revision; php-wasm defaults to an immutable commit pin. |
| `VRZNO_DEV_PATH` | Uses a local checkout instead of upstream sources. |

Direct PHP configuration uses `--enable-vrzno`. Building requires GNU Make 4.3
or newer, Node.js, npm, and Emscripten with Asyncify enabled. Make installs the
locked JavaScript build dependencies inside the build directory.

## API

The PHP examples below run inside `php.run()`. Function declarations are in
[vrzno.stub.php](vrzno.stub.php).

### Objects and arrays

`new Vrzno` returns a handle to JavaScript's `globalThis`: `window` in a browser's
main thread, or the host global in Node and workers. Read and write properties,
call methods, invoke function handles, and construct JavaScript objects using
PHP syntax. Casting a handle to a string uses JavaScript string conversion.

```php
<?php
$js = new Vrzno;
$Date = $js->Date;
$date = new $Date(0);
echo $date->toISOString(), PHP_EOL;

$numbers = $js->Array->of(3, 5);
$numbers[] = 8;

foreach($numbers as $number)
{
	echo $number, ' ';
}
```

This prints `1970-01-01T00:00:00.000Z`, then `3 5 8`. Numeric indexing and
`foreach` work for JavaScript arrays, typed arrays, and ArrayBuffers viewed as
bytes. Iteration by reference and arbitrary JavaScript iterators are unsupported.
PHP arrays exposed to JavaScript support keyed reads, `length`, and iteration
in PHP insertion order.

### Injected values, promises, and modules

| Function | Behavior |
| --- | --- |
| `vrzno_env(string $name): mixed` | Reads a value from the runtime constructor options. |
| `vrzno_shared(string $name): mixed` | Reads a value from the runtime's shared map. |
| `vrzno_await(Vrzno $promise_like): mixed` | Suspends PHP through Asyncify until the JavaScript await completes. |
| `vrzno_import(string $module_url): Vrzno` | Starts a JavaScript module import and returns a handle to its promise. |

The shared map also carries values interpolated by php-wasm's `php.r` and
`php.x` template helpers. `php.r` runs a PHP script; `php.x` evaluates a PHP
expression and returns its bridged value.

For a local module example, save this as `values.mjs` beside your application:

```js
export const answer = 42;
```

Add `moduleUrl: new URL('./values.mjs', import.meta.url).href` to the runtime
constructor options, then run:

```php
<?php
$module = vrzno_await(vrzno_import(vrzno_env('moduleUrl')));
echo $module->answer;
```

This prints `42`. Use a URL supported by the JavaScript host. Relative import
paths resolve from the generated JavaScript runtime, so passing an absolute URL
avoids dependence on its location. Node's normal loader accepts local file URLs;
browsers can load HTTP modules subject to CORS. Cloudflare requires modules to
be packaged with the Worker.

### Callbacks and lifetime

PHP closures, invokable objects, and callable arrays can cross into JavaScript.
This PHP 8.0-compatible example creates a JavaScript promise, resolves it from a
PHP callback, and waits before printing `Done from PHP`:

```php
<?php
$js = new Vrzno;
$Promise = $js->Promise;
$promise = new $Promise(function($resolve) use ($js)
{
	$js->setTimeout(fn() => $resolve('Done from PHP'), 10);
});

echo vrzno_await($promise);
```

Re-exporting the same callable reuses its JavaScript function while that wrapper
is alive. Methods retain their PHP receiver, so listener removal can match a
previously added callback. Wrappers keep their PHP values alive, including arrays
used by detached iterator factories and iterators.

Runtime refresh and PHP request shutdown release owned PHP values and invalidate
old wrappers. Using a stale JavaScript proxy or callback throws `ReferenceError`.
Remove event listeners and finish pending work before refreshing their runtime.
Cleanup at shutdown does not depend on garbage collection running first.

### Value conversion

| Value crossing the bridge | Result |
| --- | --- |
| JavaScript `null` or `undefined` | PHP `null`. |
| PHP `null` | JavaScript `null`. |
| Missing PHP array key or object property | JavaScript `undefined`. |
| JavaScript signed 32-bit integer | PHP integer. |
| Other JavaScript numbers, including larger integers, `NaN`, and infinities | PHP float. |
| JavaScript BigInt or Symbol | PHP `TypeError`. |
| JavaScript object or function | Vrzno handle. |
| PHP object, array, or callable | JavaScript proxy or callback retaining its PHP owner. |

Strings cross as UTF-8, with embedded NUL bytes preserved. `property_exists()`
can distinguish an existing JavaScript property containing `null` or `undefined`
from a missing property; `isset()` is false for all three.

PHP magic `__get` and `__isset` behavior is preserved. An object containing only
`method => 'GET'` can therefore supply JavaScript `Request` or `fetch` options:
omitted `headers` and `cache` remain `undefined`, while explicit null stays null.
JavaScript exceptions and promise rejections become catchable PHP
`RuntimeException` instances.

### HTTP streams

Vrzno provides `http` and `https` stream wrappers through the host's `fetch()`.
Enable `allow_url_fopen` for PHP stream functions such as `file_get_contents()`.
Pass your HTTP endpoint as the runtime's `endpoint` option before running:

```php
<?php
$context = stream_context_create([
	'http' => [
		'method' => 'POST',
		'header' => ['Content-Type: application/json'],
		'content' => json_encode(['value' => 'hello']),
		'ignore_errors' => true
	]
]);

$body = file_get_contents(vrzno_env('endpoint'), false, $context);

if($body === false)
{
	throw new RuntimeException('HTTP request failed');
}

echo $body;
```

Supported context options are `method`, `content`, `header`, and `ignore_errors`.
Headers accept an array of lines or a newline-separated string. Request content
and response bodies preserve binary bytes. Response headers populate
`$http_response_header` and stream metadata.

The complete response is buffered before the stream opens. Streams are readable
and non-seekable. HTTP status 400 or higher normally fails with a PHP stream
warning; `ignore_errors` allows the body to be read. Network failures still fail.
Other PHP HTTP context options, including `timeout`, are not implemented here.
Browser CORS and header restrictions follow the host's
[Fetch API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API/Using_Fetch).

### Compatibility helpers

These helpers remain supported without deprecation warnings. Use the object
bridge when passing typed values:

| Function | Behavior |
| --- | --- |
| `vrzno_eval(string $code): string` | Evaluates JavaScript source and stringifies the result. |
| `vrzno_run(string $global_function_name, array $args = []): string` | Calls a named global function with JSON-encoded arguments and stringifies the result. |
| `vrzno_timeout(int $milliseconds, callable $callback): void` | Schedules a PHP callback after a nonnegative delay. |

For debugging, `vrzno_target(Vrzno $value): int` returns an internal target handle.
These handles are distinct from Wasm memory addresses and are valid only in their
owning runtime. The internal `vrzno_zval(mixed $value): int` ownership helper
supports `php.x`; it returns an owned Wasm address.

Host restrictions still apply to evaluation and module loading. The
[Cloudflare build](https://github.com/seanmorris/php-wasm/blob/develop/CLOUDFLARE.md#worker-usage)
disables JavaScript string evaluation.

### Limits

Vrzno handles cannot be cloned or serialized. If a PHP object has a property and
a method with the same name, JavaScript sees the method. PHP classes are not
exposed as JavaScript constructors. Public static methods can be called through
an exported PHP object.

Database drivers are separate extensions:
[PDO-CFD1](https://github.com/seanmorris/pdo-cfd1) for Cloudflare D1 and
[PDO-PGlite](https://github.com/seanmorris/pdo-pglite) for embedded PostgreSQL.

## Maintainers

[Sean Morris](https://github.com/seanmorris).

## Contributing

Use [GitHub issues](https://github.com/seanmorris/vrzno/issues) for questions and
bug reports, and [pull requests](https://github.com/seanmorris/vrzno/pulls) for
changes. Follow `sm-no-saccade-style` in JavaScript, including README examples.

### Edit the bridge

JavaScript bodies and JSDoc live in [js/](js/). Five `*_js.h.in` templates declare
the C signatures and include those bodies.
[Makefile.frag](Makefile.frag) runs Emscripten's directives-only preprocessor
before `EM_JS` and `EM_ASYNC_JS` stringify the JavaScript. The resulting native
objects contain the code needed at link time.

Make installs locked dependencies under `generated/npm`, bundles `weakermap`
and its adapter with esbuild, and expands that bundle into the initialization
header. Generated headers and dependency files also stay under the build
directory's `generated/`, including builds outside the source checkout. The
runtime needs no separate npm import. See the
[weakermap integration contract](docs/weakermap.md) for cache and ownership rules.

### Run tests

With Node.js, GNU Make 4.3 or newer, and Emscripten 6.0.6 on `PATH`, run:

```sh
npm ci
npm run lint
npm test
```

Lint covers the bridge, build scripts, ESLint configuration, and tests.
`npm run lint:fix` applies formatting. `npm run test:weakermap` runs the package
and adapter checks separately. Make tests cover dependencies, parallel and
separate builds, missing inputs, recovery, cleaning, and linking all 44 bridge
functions after removing their JavaScript sources.

After building the Node runtime with this Vrzno checkout, run this repository's
PHP integration suite:

```sh
PHP_VERSION=8.4 PHP_WASM_ROOT=/absolute/path/to/php-wasm npm run test:integration
```

Then, from the php-wasm checkout, run its Vrzno package tests:

```sh
PHP_VERSION=8.4 node --expose-gc --test packages/vrzno/test/*.mjs
```

[CI](.github/workflows/ci.yml) runs the fast checks on Node 22.23.2 and 24.5.0,
then builds PHP 8.0 and 8.5 and runs both integration suites on Node 22.23.2.
The suites cover conversion, callback identity, listener removal, detached
iterators, refresh, and ownership cleanup. Controlled finalization and native GC
are both required. Negative controls check that a strong callback cache or
disabled finalization fails the expected assertion; missing GC support and
timeouts fail the tests.

Regenerate `vrzno_arginfo.h` when [vrzno.stub.php](vrzno.stub.php) changes. Run both
fast and integration checks, and confirm `git diff --check` is clean before
submitting a change.

## License

[Apache License 2.0](LICENSE). [CREDITS](CREDITS) names Sean Morris as the author.
See [NOTICE](NOTICE) for the bundled weakermap attribution.
