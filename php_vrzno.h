/* vrzno extension for PHP */

#ifndef PHP_VRZNO_H
# define PHP_VRZNO_H

extern zend_module_entry vrzno_module_entry;
# define phpext_vrzno_ptr &vrzno_module_entry

# define PHP_VRZNO_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_VRZNO)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

extern PHPAPI int vrzno_exec_callback(zend_function *fptr);
extern PHPAPI int vrzno_del_callback(zend_function *fptr);

static PHP_METHOD(Vrzno, addeventlistener);
static PHP_METHOD(Vrzno, removeeventlistener);
static PHP_METHOD(Vrzno, queryselector);

#endif	/* PHP_VRZNO_H */
