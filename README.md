# What is VRZNO?
(/vərəˈzɑːnoʊ/ | vər-ə-ZAH-noh )

VRZNO is a bridge between Javascript & PHP in an extremely nontraditional sense.

* VRZNO lets you call javascript code from PHP.
* VRZNO is a statically compiled PHP module.
* VRZNO runs in-browser, nodeJS and cloudflare.

## Contents

* [new Vrzno](#new-vrzno)
* [PHP Functions](#php-functions)
* [Javascript Object and Classes](#javascript-object-and-classes)
* [Callbacks](#callbacks)
* [Arrays](#arrays)
* [toString & __toString](#toString-&amp;-__toString)
* [URL fopen](#url-fopen)
* [Limitations](#limitations)

## How is this possible?
VRZNO is the first PHP extension built for php-wasm. Once its compiled with PHP, it can be served to any browser and executed client side. It can also run in NodeJS and CloudFlare workers.

https://github.com/seanmorris/php-wasm

![](https://github.com/seanmorris/vrzno/blob/master/banner.jpg?raw=true)

### new Vrzno
Creates a new `Vrzno` object that holds a reference to Javascript's `globalThis` object. In the browser this coresponds to `window`.

```php
<?php
$window = new Vrzno;
```
## PHP Functions

### `vrzno_await($promise)`
Wait on a Promise-like object to resolve within PHP before proceeding with the script. This will pause execution of PHP in the same way the `await` keyword does when used in Javascript.

```php
<?php
$window = new Vrzno;
$response = vrzno_await($window->fetch('https://api.weather.gov/gridpoints/TOP/40,74/forecast'));
$json = vrzno_await($response->json());

var_dump($json);
```

### `vrzno_import($module_url)`
Import a javascript library asyncronously. This is the PHP equivalent of Javascript's dynamic `import()`.

See a demo: https://codepen.io/SeanMorris227/pen/LYqNNrE

```php
<?php
$import = vrzno_import('https://cdn.jsdelivr.net/npm/@observablehq/plot@0.6/+esm');
```

### `vrzno_env($name)`
Takes a string, and returns a value passed into the PHP Object's constructor.

For example, if you invoke `PhpNode` like this in Javascript:

```javascript
import { PhpNode } from './PhpNode.mjs';
import gi from 'node-gtk';
const Gtk = gi.require('Gtk', '3.0');
const WebKit2 = gi.require('WebKit2');

const php = new PhpNode({Gtk, gi, WebKit2});
```

You can access those values in PHP like so:

```php
<?php
$gi  = vrzno_env('gi');
$Gtk = vrzno_env('Gtk');
$WebKit2 = vrzno_env('WebKit2');
```

### `vrzno_target($vrzno)`
Get the `targetId` as an integer from a `Vrzno` object.

## Javascript Object and Classes

Classes and objects are fully marshalled through Vrzno. You can use Javascript classes by bringing them in like any other object, and things should work as normal.

For example, we'll pull in the Javascript `Date` object and call the static function `now()` on it:

```php
<?php
$window = new Vrzno;
$Date = $window->Date;

var_dump($Date->now());
```

Since the `Date` object is also technically a class in Javascript, we can create a new instance in PHP with the `new` operator, and call the instance method, `toISOString`:

```php
<?php
$window = new Vrzno;
$Date = $window->Date;

$d = new $Date;

var_dump($d, $d->toISOString());
```

## Callbacks

Functions are fully marshalled as well. In thie example, we'll create an anonymous PHP callback that calls the Javascript function `window.alert()`, and then pass the PHP callback to Javascript's `setTimeout()` function, which will call it after 1 second.

As you can see, functions from **both** lnaguages are crossing the boundary here:


```php
<?php
$window = new Vrzno;

$window->setTimeout( fn() => $window->alert('Done!'), 1000);
```

Since we can create new objects from classes, we can even use this to create promises:

```php
<?php
$window = new Vrzno;
$Promise = $window->Promise;

$p = new $Promise(function($accept, $reject) use ($window) {
    $window->setTimeout( fn() => $accept('Pass.'), 1000);
});

$p->then(var_dump(...))->catch(var_dump(...));
```

## Arrays

Both string and integer properties are fully marshalled for both PHP and JS arrays.

## toString & __toString

The Javascript `.toString` method and the PHP `->__toString` method are proxied to eachother to ensure proper stringification in both languages.

## URL fopen

Vrzno implements a fetch-backend for the `http` and `https` stream wrappers, which repsects the `allow_url_fopen` ini directive.

```php
<?php
var_dump( file_get_contents('https://jsonplaceholder.typicode.com/users') );
```

You can use the `stream_context_create` like normal to modify the request:

```php
<?php
$opts = ['http' => [
    'method' => 'POST',
    'content' => json_encode(['value' => 'foobar'])
]];

$context = stream_context_create($opts);

var_dump(
    file_get_contents('https://jsonplaceholder.typicode.com/users', false, $context)
);
```

Currently the following context options are implemented:

* method
* content
* header
* ignore_errors

More information on HTTP context options can be found here:

https://www.php.net/manual/en/context.http.php

## Limitations

* The Javascript object model places properties and methods in the same namespace. PHP however uses separate namespaces for properties and methods. This means an object in PHP can have a property `$x->y` as well as a method `$x->y()`. However if a Javascript object has a method `x.y()`, then the property `x.y` must resolve to the same callback as the method.

  Currently, when a PHP object is passed into a Javascript execution environment, method names take precedence over property names. This means if a PHP object has both a method `$x->y()`, and a property `$x->y`, then the property will be inacessible from Javascript.

* PHP Classes are not **yet** accessible from Javascript. I.e. there is no way to pass a class out of PHP and call `new` on it from Javascript.
