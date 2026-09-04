dnl config.m4 for extension vrzno

PHP_ARG_ENABLE([vrzno],
  [whether to enable vrzno support],
  [AS_HELP_STRING([--enable-vrzno],
    [Enable vrzno support])],
  [no])

if test "$PHP_VRZNO" != "no"; then
  AC_DEFINE(HAVE_VRZNO, 1, [ Have vrzno support ])

  PHP_NEW_EXTENSION(vrzno, vrzno.c vrzno_object.c vrzno_array.c vrzno_expose.c vrzno_fetch.c vrzno_functions.c vrzno_dbg.c, $ext_shared)
fi
