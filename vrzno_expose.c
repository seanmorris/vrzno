void EMSCRIPTEN_KEEPALIVE vrzno_expose_inc_refcount(zval *zv)
{
	Z_ADDREF_P(zv);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_dec_refcount(zval *zv)
{
	Z_DELREF_P(zv);
}

uint32_t EMSCRIPTEN_KEEPALIVE vrzno_expose_zrefcount(zval *zv)
{
	return Z_REFCOUNT_P(zv);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_inc_crefcount(zend_function *fptr)
{
	GC_ADDREF(ZEND_CLOSURE_OBJECT(fptr));
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_dec_crefcount(zend_function *fptr)
{
	int count = GC_REFCOUNT(ZEND_CLOSURE_OBJECT(fptr));

	if(count)
	{
		GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));
	}
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_efree(void *ptr)
{
	efree(ptr);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_bool(int value, zval *rv)
{
	if(value)
	{
		ZVAL_TRUE(rv);
	}
	else
	{
		ZVAL_FALSE(rv);
	}
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_null(zval *rv)
{
	ZVAL_NULL(rv);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_undef(zval *rv)
{
	ZVAL_UNDEF(rv);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_long(long value, zval *rv)
{
	ZVAL_LONG(rv, value);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_double(double value, zval *rv)
{
	ZVAL_DOUBLE(rv, value);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_string(char* value, zval* rv)
{
	ZVAL_STRING(rv, value);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_object_for_target(jstarget *targetId, jstarget *isFunction, int isConstructor, zval *rv)
{
	vrzno_object *vObj = vrzno_create_object_for_target(targetId, isFunction, isConstructor);
	ZVAL_OBJ(rv, &vObj->zo);
}

zval** EMSCRIPTEN_KEEPALIVE vrzno_expose_create_params(int argc)
{
	zval **zvals = (zval**) emalloc(argc * sizeof(zval*));

	for(int i = 0; i < argc; i++)
	{
		zvals[i] = (zval*) emalloc(sizeof(zval)); // @todo: Figure when to clear this.
	}

	return zvals;
}

char* EMSCRIPTEN_KEEPALIVE vrzno_expose_object_keys(zval* zv)
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

	index = NULL;

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

char* EMSCRIPTEN_KEEPALIVE vrzno_expose_array_keys(zval *zv)
{
	if (Z_TYPE_P(zv) != IS_ARRAY)
	{
		return NULL;
	}

	zval keys;
	array_init(&keys);
	zend_string *key;
	zend_ulong	*index;

	ZEND_HASH_FOREACH_KEY(Z_ARRVAL_P(zv), index, key) {
		if(key)
		{
			add_next_index_string(&keys, ZSTR_VAL(key));
		}
	} ZEND_HASH_FOREACH_END();

	index = NULL;

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

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_deref(zval* zv)
{
	ZVAL_DEREF(zv);
	return zv;
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_dump(zval* zv, int depth)
{
	php_debug_zval_dump(zv, depth);
}

uint8_t EMSCRIPTEN_KEEPALIVE vrzno_expose_type(zval *zv)
{
	return Z_TYPE_P(zv);
}

uint32_t EMSCRIPTEN_KEEPALIVE vrzno_expose_array_length(zval *zv)
{
	return zend_hash_num_elements(Z_ARRVAL_P(zv));
}

jstarget* EMSCRIPTEN_KEEPALIVE vrzno_expose_target(zval *zv)
{
	if(Z_TYPE_P(zv) == IS_OBJECT && Z_OBJCE_P(zv) == vrzno_class_entry)
	{
		return vrzno_fetch_object(Z_OBJ_P(zv))->targetId;
	}

	if(Z_TYPE_P(zv) == IS_OBJECT && Z_OBJCE_P(zv)->parent == vrzno_class_entry)
	{
		return vrzno_fetch_object(Z_OBJ_P(zv))->targetId;
	}

	return NULL;
}

zend_function* EMSCRIPTEN_KEEPALIVE vrzno_expose_callable(zval *zv)
{
	zend_fcall_info_cache fcc;
	char *errstr = NULL;

	if(zend_is_callable_ex(zv, NULL, 0, NULL, &fcc, &errstr))
	{
		return fcc.function_handler;
	}

	return NULL;
}

zend_long* EMSCRIPTEN_KEEPALIVE vrzno_expose_long(zval *zv)
{
	return Z_LVAL_P(zv);
}

double* EMSCRIPTEN_KEEPALIVE vrzno_expose_double(zval *zv)
{
	return &Z_DVAL_P(zv);
}

char* EMSCRIPTEN_KEEPALIVE vrzno_expose_string(zval *zv)
{
	return Z_STRVAL_P(zv);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_property_pointer(zval *zv, char *name)
{
	zval *rv = NULL;
	return zend_read_property(Z_OBJCE_P(zv), Z_OBJ_P(zv), name, strlen(name), 1, rv);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_pointer(zval *zv, unsigned offset)
{
	return zend_hash_index_find(Z_ARRVAL_P(zv), offset);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_key_pointer(zval *zv, char *key)
{
	zend_string *zKey = zend_string_init(key, strlen(key), 0);
	zval *rv = zend_hash_find(Z_ARRVAL_P(zv), zKey);
	zend_string_release(zKey);
	return rv;
}

zend_function* EMSCRIPTEN_KEEPALIVE vrzno_expose_method_pointer(zval *zv, char *method)
{
	zend_string *zMethod = zend_string_init(method, strlen(method), 0);
	zend_function *zf = zend_std_get_method(&Z_OBJ_P(zv), zMethod, 0);
	zend_string_release(zMethod);
	return zf;
}
