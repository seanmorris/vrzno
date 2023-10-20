int EMSCRIPTEN_KEEPALIVE vrzno_expose_inc_refcount(zval *zv)
{
	Z_ADDREF_P(zv);
	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_dec_refcount(zval *zv)
{
	Z_DELREF_P(zv);
	return NULL;
}


int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_bool(long value)
{
	zval *zv = (zval*) malloc(sizeof(zval));

	if(value)
	{
		ZVAL_TRUE(zv);
	}
	else
	{
		ZVAL_FALSE(zv);
	}

	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_long(long value)
{
	zval *zv = (zval*) malloc(sizeof(zval));
    ZVAL_LONG(zv, value);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_double(double* value)
{
	zval *zv = (zval*) malloc(sizeof(zval));
	ZVAL_DOUBLE(zv, *value);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_string(char* value)
{
	zval *zv = (zval*) malloc(sizeof(zval));
	ZVAL_STRING(zv, value);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_object_for_target(int target_id)
{
	zval *zv = (zval*) malloc(sizeof(zval));
	zend_object *zObj = vrzno_create_object_for_target(target_id);
	ZVAL_OBJ(zv, zObj);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_params(int argc)
{
	zval *zvals = (zval*) malloc(argc * sizeof(zval*));
	return zvals;
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_set_param(zval** zvals, int index, zval* arg)
{
	zvals[index] = arg;
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_dump(zval* zv)
{
	php_debug_zval_dump(zv, 1);
}

// int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_vrzno_object(long target)
// {
// }

void EMSCRIPTEN_KEEPALIVE vrzno_expose_ptor_dtor(zval *zv)
{
	zval_ptr_dtor(zv);
}


int EMSCRIPTEN_KEEPALIVE vrzno_expose_type(zval *zv)
{
	return Z_TYPE_P(zv);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_callable(zval *zv)
{
	zend_fcall_info_cache fcc;
	char *errstr = NULL;

	if(zend_is_callable_ex(zv, NULL, 0, NULL, &fcc, &errstr))
	{
		return fcc.function_handler;
	}

	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_long(zval *zv)
{
	return Z_LVAL_P(zv);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_double(zval *zv)
{
	return &Z_DVAL_P(zv);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_string(zval *zv)
{
	return Z_STRVAL_P(zv);
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
