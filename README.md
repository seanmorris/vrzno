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
