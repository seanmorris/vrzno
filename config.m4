dnl config.m4 for extension vrzno
if test -z "$SED"; then
  PHP_VRZNO_SED="sed";
else
  PHP_VRZNO_SED="$SED";
fi

PHP_ARG_ENABLE([vrzno],
  [whether to enable vrzno support],
  [AS_HELP_STRING([--enable-vrzno],
    [Enable vrzno support])],
  [no])

if test "$PHP_VRZNO" != "no"; then
  AC_DEFINE(HAVE_VRZNO, 1, [ Have vrzno support ])

  PHP_NEW_EXTENSION(vrzno, vrzno.c, $ext_shared)
fi
