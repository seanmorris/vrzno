int EMSCRIPTEN_KEEPALIVE vrzno_expose_inc_zrefcount(zval *zv)
{
	Z_ADDREF_P(zv);
	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_dec_zrefcount(zval *zv)
{
	int count = Z_REFCOUNT_P(zv);
	if(!count)
	{
		return NULL;
	}
	Z_DELREF_P(zv);
	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_zrefcount(zval *zv)
{
	return Z_REFCOUNT_P(zv);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_inc_crefcount(zend_function *fptr)
{
	GC_ADDREF(ZEND_CLOSURE_OBJECT(fptr));
	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_dec_crefcount(zend_function *fptr)
{
	int count = GC_REFCOUNT(ZEND_CLOSURE_OBJECT(fptr));
	if(!count)
	{
		return NULL;
	}

	GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));

	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_crefcount(zend_function *fptr)
{
	return GC_REFCOUNT(ZEND_CLOSURE_OBJECT(fptr));
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_efree(void *addr)
{
	efree(addr);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_bool(long value)
{
	zval *zv = (zval*) emalloc(sizeof(zval));
	memset(zv, 0, sizeof(zval));

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

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_null(void)
{
	zval *zv = (zval*) emalloc(sizeof(zval));
	memset(zv, 0, sizeof(zval));
    ZVAL_NULL(zv);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_undef(void)
{
	zval *zv = (zval*) emalloc(sizeof(zval));
	memset(zv, 0, sizeof(zval));
    ZVAL_UNDEF(zv);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_long(long value)
{
	zval *zv = (zval*) emalloc(sizeof(zval));
	memset(zv, 0, sizeof(zval));
    ZVAL_LONG(zv, value);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_double(double value)
{
	zval *zv = (zval*) emalloc(sizeof(zval));
	memset(zv, 0, sizeof(zval));
	ZVAL_DOUBLE(zv, value);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_string(char* value)
{
	zval *zv = (zval*) emalloc(sizeof(zval));
	memset(zv, 0, sizeof(zval));
	ZVAL_STRING(zv, value);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_object_for_target(int target_id, int isFunction, int isConstructor)
{
	zval *zv = (zval*) emalloc(sizeof(zval));
	memset(zv, 0, sizeof(zval));
	vrzno_object *vObj = vrzno_create_object_for_target(target_id, isFunction, isConstructor);
	ZVAL_OBJ(zv, &vObj->zo);
	return zv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_create_params(int argc)
{
	zval *zvals = (zval*) emalloc(argc * sizeof(zval*));
	memset(zvals, 0, argc * sizeof(zval*));
	return zvals;
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_set_param(zval** zvals, int index, zval* arg)
{
	zvals[index] = arg;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_is_target(zval* zv)
{
	if(Z_TYPE_P(zv) != IS_OBJECT)
	{
		return 0;
	}

	if(Z_OBJCE_P(zv) != vrzno_class_entry)
	{
		if(Z_OBJCE_P(zv)->parent != vrzno_class_entry)
		{
			return 0;
		}
	}

	return vrzno_fetch_object(Z_OBJ_P(zv))->targetId;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_object_keys(zval* zv)
{
	if (Z_TYPE_P(zv) != IS_OBJECT)
	{
		return NULL;
	}

	HashTable *properties = Z_OBJPROP_P(zv);

	if(!properties)
	{
		return NULL;
	}

	zval keys;
	array_init(&keys);
	zend_string *key;
	zend_ulong	*index;

	ZEND_HASH_FOREACH_KEY(properties, index, key) {
		if(key)
		{
			add_next_index_string(&keys, ZSTR_VAL(key));
		}
	} ZEND_HASH_FOREACH_END();

	smart_str buf = {0};

	php_json_encoder  encoder;
	php_json_encode_init(&encoder);
	encoder.max_depth = PHP_JSON_PARSER_DEFAULT_DEPTH;
	php_json_encode_zval(&buf, &keys, 0, &encoder);
	smart_str_0(&buf);

	char *json = malloc(ZSTR_LEN(buf.s) + 1);

	memcpy(json, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s) + 1);
	smart_str_free(&buf);

	return json;
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_dump(zval* zv)
{
	php_debug_zval_dump(zv, 1);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_type(zval *zv)
{
	return Z_TYPE_P(zv);
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_callable(zval *zv)
{
	zend_fcall_info_cache fcc;
	char *errstr = NULL;

	if(Z_TYPE_P(zv) == IS_OBJECT && Z_OBJCE_P(zv) == vrzno_class_entry)
	{
		return vrzno_fetch_object(Z_OBJ_P(zv))->isFunction;
	}

	if(Z_TYPE_P(zv) == IS_OBJECT && Z_OBJCE_P(zv)->parent == vrzno_class_entry)
	{
		return vrzno_fetch_object(Z_OBJ_P(zv))->isFunction;
	}

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

int EMSCRIPTEN_KEEPALIVE vrzno_expose_property_pointer(zval *object, char *name)
{
	zval *rv = NULL;

	zval *data = zend_read_property(Z_OBJCE_P(object), Z_OBJ_P(object), name, strlen(name), 1, rv);

	return data;
}

// int EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_pointer(zval *object, char *name)

// zend_array_count
// zend_array_is_list
