# What is VRZNO?

(/vərəˈzɑːnoʊ/ vər-ə-ZAH-noh)

VRZNO is a bridge between Javascript & PHP in an extremely nontraditional sense.

* VRZNO lets you call javascript code from PHP.

* VRZNO is a statically compiled PHP module.

* VRZNO runs in-browser.

## How is this possible?

VRZNO is the first PHP extension built for PIB/php-wasm. Once its compiled with PHP to WASM, it can be served to any browser and executed client side.

The PIB/php-wasm packages allow you to call PHP from javascript, the only way to communicate from PHP back to JS was to print output.

This changes all that.

VRZNO allows you to control the page directly from PHP.

## Usage

At the moment, the only exposed functions are vrzno_run($funcName, $args) & vrzno_eval($js).

`vrzno_run` will execute a global function. It takes a function name as the first param, and an array of arguments as its second:

```php
<?php vrzno_run('alert', ['Hello, world!']);
```

`vrzno_eval` will run any javascript code provided in its first param in the context of the page, and return its response as a string to PHP:

```php
<?php vrzno_eval('alert(`Hello, world!`)');
```

There are plans in the works for `vrzno_set($varname, $value)`, `vrzno_get($varname)`, and `vrzno_select($namespace)`.

## Compiling (incomplete)

```bash
# Clone the PHP Repo
git clone https://github.com/php/php-src.git;

# Enter the extension directory
cd php-src/ext;


# Clone the VRZNO repo into the PHP source tree
git clone https://github.com/seanmorris/vrzno.git;

cd vrzno;

rm configure        # Remove the configure script if any exists
./buildconf --force # Rebuild the configuration script

# Run the new configuration script via emscripten.
emconfigure \
  ./configure \
  --disable-all \
  --disable-cgi \
  --disable-cli \
  --disable-rpath \
  --disable-phpdbg \
  --with-valgrind=no \
  --without-pear \
  --without-pcre-jit \
  --with-layout=GNU \
  --enable-embed=static \
  --enable-bcmath \
  --enable-json \
  --enable-ctype \
  --enable-mbstring \
  --disable-mbregex \
  --enable-tokenizer \
  --enable-vrzno

# Run the make scripts with emscripten.
emmake make -j8

#

```
