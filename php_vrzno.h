/* vrzno extension for PHP */

#ifndef PHP_VRZNO_H
# define PHP_VRZNO_H

extern zend_module_entry vrzno_module_entry;
# define phpext_vrzno_ptr &vrzno_module_entry

# define PHP_VRZNO_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_VRZNO)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#include <stdbool.h>

typedef struct {
	zend_object zo;
	long isFunction;
	bool isConstructor;
	long targetId;
} vrzno_object;

vrzno_object* vrzno_fetch_object(zend_object *obj);

PHP_FUNCTION(vrzno_eval);
PHP_FUNCTION(vrzno_run);
PHP_FUNCTION(vrzno_timeout);
PHP_FUNCTION(vrzno_await);
PHP_FUNCTION(vrzno_env);
PHP_FUNCTION(vrzno_shared);
PHP_FUNCTION(vrzno_import);
PHP_FUNCTION(vrzno_target);
PHP_FUNCTION(vrzno_zval);

PHP_METHOD(Vrzno, __invoke);
PHP_METHOD(Vrzno, __call);
PHP_METHOD(Vrzno, __get);
PHP_METHOD(Vrzno, __construct);
PHP_METHOD(Vrzno, __destruct);
PHP_METHOD(Vrzno, __toString);

static zend_object_iterator *vrzno_array_get_iterator(zend_class_entry *ce, zval *zv, int by_ref);

# define VRZNO_ICON_DATA_URI "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADIAAAAwCAYAAABT9ym6AAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH5AgXBycOhI1eNAAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAACQklEQVRo3u2asUvDUBDGv0hBwdmhlFQQHLt0EyrFRUQ6FDuUujh1KggO4pIxIOKgiJ38A4pDS0Hp0KUUBTcXR8HBULoLgrrEwZw2R17yDI2Vl35T07x3vN7v3d27pBpCStd1GxHIsiwtzLwZKKJEWALJZDKqNdlhCClDRJMlESEBl2q1muvaMAwpMuoT+WsSIiKyZNQlQiRM0wQA1Ov1iRAQSURGvTrCSfw30c4YiVl7lEz8KvukYmJ61hJ5MOrsFRQb8SVCHuB7OSyZqGIifjHyW88Ph8Ox9ClhYzG+dUTULzxfHgEA0uUDTOKEMK3snASJkyGJCI2rPqnXj8h2hJSdOIkgiWInbHY83dkAAJTMC7VOv1rTqNoAsLW55vIckSEP8BgIKx47JNn6w3dEq9NTPGvxrMMJ7DbufA2eVVZ875M9bqfdbvuSCYpN9etIsVj09Ny6PgsAKOSynvNovIiM6D5di2KU1hPfyk6e71rvUiQajw8uz3LP0zXZofGV5YxvjBIJmqc+EaqMTfZegupK1/GkiATp6rr/5eG9jG+WIzvbJ+ee40XZjeZR3SApV9kT/JeRfgjNSRlaTC9IxVrQeJGIBF9nrPoRzfGoLVMfDktlz+z0HVu39yyblX0Xxu18vLzF441VYIdYyGU1x6O2zFlLVG9EZCTsaF7ZSv0OUaSmUaWPrr4lao0Q0JxsFXMix/k8AGA1lXJ9P1iaj5QMkUg9vbrWdjMYAAD2+/3pcy2XyFOtTi+SfwdxErJShsgnw04E9v7JwL4AAAAASUVORK5CYII="

#endif
