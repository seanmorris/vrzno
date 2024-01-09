# What is VRZNO?
(/vərəˈzɑːnoʊ/ vər-ə-ZAH-noh)

VRZNO is a bridge between Javascript & PHP in an extremely nontraditional sense.

* VRZNO lets you call javascript code from PHP.
* VRZNO is a statically compiled PHP module.
* VRZNO runs in-browser, nodeJS and cloudflare.

## Contents

* [PDO D1 SQL Connector (Cloudflare)](#pdo-d1-sql-connector-cloudflare)
* [PHP API](#php-api)

## How is this possible?
VRZNO is the first PHP extension built for php-wasm. Once its compiled with PHP, it can be served to any browser and executed client side.

This changes all that.

VRZNO allows you to control the page directly from PHP.

https://github.com/seanmorris/php-wasm

![](https://github.com/seanmorris/vrzno/blob/master/banner.jpg?raw=true)

## PDO D1 SQL Connector (Cloudflare)
You can connect to a D1 database inside cloudflare with PDO.

In javascript, just pass the DB object into the contructor (note that Cloudflare workers use `PhpWeb`. not `PhpNode`.)

```js
const php = new PhpWeb({ db: context.env.db });
```

In PHP, just import that object through `vrzno_env`, take the target ID with `vrzno_target` and append it to `vrzno:`. Thats it, that's your whole connection string.

```php
$pdo = new PDO('vrzno:' . vrzno_target(vrzno_env('db')));
```

PDO can now be used as normal:

```php
$select = $pdo->prepare('SELECT PageTitle, PageContent FROM WikiPages WHERE PageTitle = ?');
$select->execute([$pageTitle]);
$page = $select->fetchObject();
```

## PHP API
### `new Vrzno()`
Creates a new `Vrzno` object that holds a reference to JavaScript's `globalThis` object.

### `vrzno_import()`
Import a javascript library asyncronously. This is the PHP equivalent of JavaScript's dynamic `import()`.

```php
$import = vrzno_import('https://cdn.jsdelivr.net/npm/@observablehq/plot@0.6/+esm');
```

See a demo: https://codepen.io/SeanMorris227/pen/LYqNNrE

```html
<script async type = "text/javascript" src = "https://cdn.jsdelivr.net/npm/php-wasm/php-tags.mjs"></script>

<script type = "text/php" data-stdout = "#out" data-stderr = "#err">
<?php
  $window = new Vrzno;
  $import = vrzno_import('https://cdn.jsdelivr.net/npm/@observablehq/plot@0.6/+esm');
  $import->then(function($Plot) use($window) {
  $plot = $Plot->rectY(
    (object)['length' => 100000],
    $Plot->binX(
      (object)['y'=> function($a,$b){
        return -cos($b->x1*pi());
      }],
      (object)['x'=> $window->Math->random]
    )
  )->plot();

  $window->document->body->append($plot);

  })->catch(fn($error) => $window->console->error($error->message));

</script>

<pre id = "out"></pre>
<pre id = "err"></pre>
```

### `vrzno_env()`
Takes a string, and returns an object passed into the PHP Object's constructor.

For example, if you invoke `PhpNode` like this in JavaScript:

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

### `vrzno_target()`
Get the `targetId` as an integer from a `Vrzno` object.

### `vrzno_await()`
Wait on a Promise-like object to resolve within PHP before proceeding with the script. This will pause execution of PHP in the same way the `await` keyword will when used in JavaScript.


