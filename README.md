# vrzno
(/vərəˈzɑːnoʊ/ | vər-ə-ZAH-noh)

`vrzno` is the JavaScript bridge extension for `php-wasm`.
It lets PHP work with JavaScript values, objects, arrays, callbacks, classes, promises, and globals as if they were local PHP values.

![](https://github.com/seanmorris/vrzno/blob/master/banner.jpg?raw=true)

`vrzno` requires PHP 8.0+.

The PDO connectors that used to live here now ship as separate extensions:

- Cloudflare D1: <https://github.com/seanmorris/pdo-cfd1>
- PGlite / PostgreSQL: <https://github.com/seanmorris/pdo-pglite>

## What It Gives You

- access to `globalThis` from PHP through `new Vrzno`
- JS object, array, and callback marshalling in both directions
- promise interop through `vrzno_await()`
- dynamic module loading through `vrzno_import()`
- runtime value injection through `vrzno_env()` and `vrzno_shared()`
- `http` and `https` stream wrapper support backed by JavaScript `fetch()`

## Quick Start

In `php-wasm`, Vrzno is typically available by default.
Pass any JavaScript values you want to expose into the runtime constructor, then read them from PHP.

```js
import { PhpNode } from 'php-wasm/PhpNode.mjs';

const php = new PhpNode({
  version: '8.4',
  answer: 42,
});

await php.run(`<?php
  $window = new Vrzno;

  var_dump(vrzno_env('answer'));
  var_dump($window->Date->now() > 0);
`);
```

## Core API

### `new Vrzno`

Creates a handle to JavaScript's `globalThis`.
In a browser that usually means `window`.

```php
<?php
$window = new Vrzno;
```

### `vrzno_await($promiseLike)`

Waits for a promise-like JavaScript value to settle and returns the resolved value to PHP.

```php
<?php
$window = new Vrzno;

$response = vrzno_await(
    $window->fetch('https://api.weather.gov/gridpoints/TOP/40,74/forecast')
);

$json = vrzno_await($response->json());

var_dump($json);
```

### `vrzno_import($moduleUrl)`

Performs a dynamic JavaScript `import()` and returns the resulting promise/object bridge.

```php
<?php
$plot = vrzno_await(
    vrzno_import('https://cdn.jsdelivr.net/npm/@observablehq/plot@0.6/+esm')
);
```

### `vrzno_env($name)`

Returns a value that was attached directly to the runtime constructor arguments.

```js
import { PhpNode } from 'php-wasm/PhpNode.mjs';
import gi from 'node-gtk';

const Gtk = gi.require('Gtk', '3.0');
const WebKit2 = gi.require('WebKit2');

const php = new PhpNode({ gi, Gtk, WebKit2 });
```

```php
<?php
$gi = vrzno_env('gi');
$Gtk = vrzno_env('Gtk');
$WebKit2 = vrzno_env('WebKit2');
```

### `vrzno_shared($name)`

Reads a value from the runtime's shared-value map.
This is the mechanism used by helpers such as `php.x` and `php.r` in `php-wasm` to move arbitrary JavaScript values into PHP without JSON-encoding them first.

### `vrzno_target($value)`

Returns the internal numeric target handle for a bridged JavaScript object.
This is mostly useful for debugging and internals work.

### Legacy Compatibility Helpers

These functions remain supported without deprecation warnings, but the object bridge is the preferred API:

- `vrzno_eval($code)`
- `vrzno_run($globalFunctionName, $args = [])`
- `vrzno_timeout($milliseconds, $callback)`

## Working With JavaScript Objects And Classes

JavaScript classes and objects are marshalled directly through Vrzno.
Static calls, constructors, property reads, and method calls all use normal PHP syntax.

```php
<?php
$window = new Vrzno;
$Date = $window->Date;

var_dump($Date->now());

$date = new $Date;
var_dump($date->toISOString());
```

## Callbacks In Both Directions

You can pass a PHP callable to JavaScript and let JavaScript call it later:

```php
<?php
$window = new Vrzno;

$window->setTimeout(
    fn() => $window->console->log('Done from PHP'),
    1000
);
```

You can also construct JavaScript promises from PHP:

```php
<?php
$window = new Vrzno;
$Promise = $window->Promise;

$promise = new $Promise(function($accept, $reject) use ($window) {
    $window->setTimeout(fn() => $accept('Pass.'), 1000);
});

$promise->then(var_dump(...))->catch(var_dump(...));
```

## Arrays And Iteration

JavaScript arrays are exposed as array-like / iterable values on the PHP side.
Indexed access and property-style access are both bridged.

## Value Semantics

- JavaScript `null` and `undefined` both become PHP `null`; PHP has no separate undefined value.
- PHP `null` becomes JavaScript `null`. A missing PHP array key or object property reads as JavaScript `undefined`.
- `property_exists()` can distinguish an existing JavaScript property containing `null` or `undefined` from a missing property. `isset()` remains false for all three cases.
- JavaScript 32-bit integers become PHP integers. Other numbers—including `NaN`, infinities, and larger integers—become PHP floats.
- JavaScript BigInt and Symbol values cannot be represented in PHP and raise `TypeError`.
- Embedded null bytes are preserved in strings crossing either direction.

JavaScript exceptions and rejected promises become catchable PHP `RuntimeException` instances. A bridged JavaScript proxy that outlives a PHP runtime refresh throws `ReferenceError` when used.

`Vrzno` objects are runtime handles. They cannot be cloned or serialized.

## HTTP And `allow_url_fopen`

Vrzno implements `http` and `https` stream wrappers using JavaScript `fetch()`.
That means normal PHP stream functions can work in wasm-hosted runtimes when `allow_url_fopen` is enabled.

```php
<?php
var_dump(file_get_contents('https://jsonplaceholder.typicode.com/users'));
```

Basic stream context options are supported:

- `method`
- `content`
- `header`
- `ignore_errors`

```php
<?php
$context = stream_context_create([
    'http' => [
        'method'  => 'POST',
        'content' => json_encode(['value' => 'foobar']),
    ],
]);

var_dump(
    file_get_contents(
        'https://jsonplaceholder.typicode.com/users',
        false,
        $context
    )
);
```

More background on HTTP stream options: <https://www.php.net/manual/en/context.http.php>

## Limitations

- JavaScript uses one namespace for properties and methods. PHP separates them. If a PHP object exposes both `$x->y` and `$x->y()`, JavaScript can only see one of them. Today, method names win.
- PHP classes are not exposed back to JavaScript as constructible classes.
- Static methods are not currently proxied back from PHP into JavaScript.
- The legacy helper functions are string-based conveniences, not the preferred long-term API.

## Platform Support

Vrzno 0.2 supports PHP 8.0 through 8.5 compiled for Emscripten's wasm32 memory model. It is not a native desktop/server PHP extension and intentionally fails compilation on non-wasm32 targets.

## Building And Testing

Use a neighboring `php-wasm` checkout as the build harness:

```sh
cd ../php-wasm
npm ci
make image
make -j2 node-mjs PHP_VERSION=8.4 VRZNO_DEV_PATH="$PWD/../vrzno"
PHP_VERSION=8.4 node --test packages/vrzno/test/*.mjs
```

The CI matrix compiles and smoke-tests the oldest and newest supported PHP releases. Before sending a change, regenerate `vrzno_arginfo.h` from `vrzno.stub.php`, run the integration tests, and confirm `git diff --check` is clean.

## License

Vrzno is licensed under the Apache License 2.0. See [LICENSE](LICENSE).
