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
  dnl Find the differences between 8.0 and 8.1 and then remove this.
  tmp_version=$PHP_VERSION
  if test -z "$tmp_version"; then
    if test -z "$PHP_CONFIG"; then
      AC_MSG_ERROR([php-config not found])
    fi
    php_version=`$PHP_CONFIG --version 2>/dev/null|head -n 1|$PHP_VRZNO_SED -e 's#\([0-9]\.[0-9]*\.[0-9]*\)\(.*\)#\1#'`
  else
    php_version=`echo "$tmp_version"|$PHP_VRZNO_SED -e 's#\([0-9]\.[0-9]*\.[0-9]*\)\(.*\)#\1#'`
  fi

  ac_IFS=$IFS
  IFS="."
  set $php_version
  IFS=$ac_IFS
  vrzno_php_version=`expr [$]1 \* 1000000 + [$]2 \* 1000 + [$]3`

  if test -z "$php_version"; then
    AC_MSG_ERROR([failed to detect PHP version, please report])
  fi

  if test "$vrzno_php_version" -lt "8001000"; then
    AC_MSG_ERROR([You need at least PHP 8.1 to be able to use Vrzno])
  fi
  dnl Find the differences between 8.0 and 8.1 and then remove this.

  AC_DEFINE(HAVE_VRZNO, 1, [ Have vrzno support ])

  PHP_NEW_EXTENSION(vrzno, vrzno.c, $ext_shared)
fi
