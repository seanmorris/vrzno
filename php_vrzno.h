/* vrzno extension for PHP */

#ifndef PHP_VRZNO_H
# define PHP_VRZNO_H

extern zend_module_entry vrzno_module_entry;
# define phpext_vrzno_ptr &vrzno_module_entry

# define PHP_VRZNO_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_VRZNO)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

extern PHPAPI int vrzno_exec_callback(zend_function *fptr, zval **argv, int argc);
extern PHPAPI int vrzno_del_callback(zend_function *fptr);

PHP_FUNCTION(vrzno_eval);
PHP_FUNCTION(vrzno_run);
PHP_FUNCTION(vrzno_timeout);

PHP_METHOD(Vrzno, addeventlistener);
PHP_METHOD(Vrzno, removeeventlistener);
PHP_METHOD(Vrzno, queryselector);
PHP_METHOD(Vrzno, __invoke);
PHP_METHOD(Vrzno, __call);
PHP_METHOD(Vrzno, __get);
// PHP_METHOD(Vrzno, __set);

#endif	/* PHP_VRZNO_H */
