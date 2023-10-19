int EMSCRIPTEN_KEEPALIVE vrzno_expose_dec_refcount(zval *zv)
{
	Z_DELREF_P(zv);

	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_property_type(zval *object, char *name)
{
	zval *rv = NULL;

	EM_ASM({ console.log('TYPE_CHECK', $0, $1, $2, $3, $4, $5, $6); }, object, Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	zval *data = zend_read_property(Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	// if (data == &EG(uninitialized_zval)) {
	// 	return NULL;
	// }

	// ZVAL_DEREF(data);

	return Z_TYPE_P(data);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_property_callable(zval *object, char *name)
{
	zval *rv = NULL;

	EM_ASM({ console.log('CALL_CHECK', $0, $1, $2, $3, $4, $5, $6); }, object, Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	zval *data = zend_read_property(Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	// if (data == &EG(uninitialized_zval)) {
	// 	return NULL;
	// }

	// ZVAL_DEREF(data);

	zend_fcall_info_cache fcc;
	char *errstr = NULL;

	if(zend_is_callable_ex(data, NULL, 0, NULL, &fcc, &errstr))
	{
		return fcc.function_handler;
	}

	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_property_long(zval *object, char *name)
{
	zval *rv = NULL;

	zval *data = zend_read_property(Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	// if (data == &EG(uninitialized_zval)) {
	// 	return NULL;
	// }

	// ZVAL_DEREF(data);

	return Z_LVAL_P(data);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_property_double(zval *object, char *name)
{
	zval *rv = NULL;

	zval *data = zend_read_property(Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	// if (data == &EG(uninitialized_zval)) {
	// 	return NULL;
	// }

	// ZVAL_DEREF(data);

	return &Z_DVAL_P(data);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_property_string(zval *object, char *name)
{
	zval *rv = NULL;

	zval *data = zend_read_property(Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	// if (data == &EG(uninitialized_zval)) {
	// 	return NULL;
	// }

	// ZVAL_DEREF(data);

	return Z_STRVAL_P(data);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_property_pointer(zval *object, char *name)
{
	zval *rv = NULL;

	zval *data = zend_read_property(Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	// if (data == &EG(uninitialized_zval)) {
	// 	return NULL;
	// }

	// ZVAL_DEREF(data);

	return data;
}

// int EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_type(zval *object, char *name)
// int EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_long(zval *object, char *name)
// int EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_double(zval *object, char *name)
// int EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_string(zval *object, char *name)
// int EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_pointer(zval *object, char *name)
