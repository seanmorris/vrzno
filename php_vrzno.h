/* vrzno extension for PHP */

#ifndef PHP_VRZNO_H
# define PHP_VRZNO_H

extern zend_module_entry vrzno_module_entry;
# define phpext_vrzno_ptr &vrzno_module_entry

# define PHP_VRZNO_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_VRZNO)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

// extern PHPAPI int vrzno_exec_callback(zend_function *fptr, zval **argv, int argc);
// extern PHPAPI int vrzno_del_callback(zend_function *fptr);

typedef struct {
	bool isFunction;
	bool isConstructor;
	long targetId;
	zend_object zo;
} vrzno_object;

typedef struct {
	unsigned int errcode;
	char *errmsg;
} pdo_vrzno_db_error_info;

typedef struct {
	long *targetId;
	pdo_vrzno_db_error_info einfo;
} pdo_vrzno_db_handle;

typedef struct {
	pdo_vrzno_db_handle *db;
	vrzno_object *stmt;
	unsigned long curr;
	unsigned long row_count;
	zval *results;
	unsigned pre_fetched:1;
	unsigned done:1;
} pdo_vrzno_stmt;

PHP_FUNCTION(vrzno_eval);
PHP_FUNCTION(vrzno_run);
PHP_FUNCTION(vrzno_timeout);
PHP_FUNCTION(vrzno_new);
PHP_FUNCTION(vrzno_await);
PHP_FUNCTION(vrzno_env);
PHP_FUNCTION(vrzno_import);
PHP_FUNCTION(vrzno_target);

// PHP_METHOD(Vrzno, addeventlistener);
// PHP_METHOD(Vrzno, removeeventlistener);
// PHP_METHOD(Vrzno, queryselector);
PHP_METHOD(Vrzno, __invoke);
PHP_METHOD(Vrzno, __call);
PHP_METHOD(Vrzno, __get);
// PHP_METHOD(Vrzno, __set);
PHP_METHOD(Vrzno, __construct);

#endif

static zend_object_iterator *vrzno_array_get_iterator(zend_class_entry *ce, zval *zv, int by_ref);
