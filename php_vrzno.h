/* vrzno extension for PHP */

#ifndef PHP_VRZNO_H
# define PHP_VRZNO_H

extern zend_module_entry vrzno_module_entry;
# define phpext_vrzno_ptr &vrzno_module_entry

# define PHP_VRZNO_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_VRZNO)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif	/* PHP_VRZNO_H */
