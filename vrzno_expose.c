zend_refcounted* EMSCRIPTEN_KEEPALIVE vrzno_expose_gc_ptr(zval *rc)
{
	return Z_COUNTED_P(rc);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_inc_refcount(zend_refcounted *rc)
{
	GC_TRY_ADDREF(rc);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_dec_refcount(zend_refcounted *rc)
{
	GC_TRY_DELREF(rc);
}

uint32_t EMSCRIPTEN_KEEPALIVE vrzno_expose_refcount(zend_refcounted *rc)
{
	return GC_REFCOUNT(rc);
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

char* EMSCRIPTEN_KEEPALIVE vrzno_expose_object_keys(zend_object* zo)
{
	HashTable *properties = zo->handlers->get_properties(zo);

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

char* EMSCRIPTEN_KEEPALIVE vrzno_expose_array_keys(zend_array *za)
{
	zval keys;
	array_init(&keys);
	zend_string *key;
	zend_ulong	*index;

	ZEND_HASH_FOREACH_KEY(za, index, key) {
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

uint32_t EMSCRIPTEN_KEEPALIVE vrzno_expose_array_length(zend_array *za)
{
	return zend_hash_num_elements(za);
}

jstarget* EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_target(zval *zv)
{
	if(Z_TYPE_P(zv) != IS_OBJECT)
	{
		return NULL;
	}

	if(Z_OBJCE_P(zv) == vrzno_class_entry)
	{
		return vrzno_fetch_object(Z_OBJ_P(zv))->targetId;
	}
	else if(Z_OBJCE_P(zv)->parent == vrzno_class_entry)
	{
		return vrzno_fetch_object(Z_OBJ_P(zv))->targetId;
	}

	return NULL;
}

jstarget* EMSCRIPTEN_KEEPALIVE vrzno_expose_target(zend_object *zo)
{
	if(zo->ce == vrzno_class_entry)
	{
		return vrzno_fetch_object(zo)->targetId;
	}
	else if(zo->ce->parent == vrzno_class_entry)
	{
		return vrzno_fetch_object(zo)->targetId;
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

zend_object* EMSCRIPTEN_KEEPALIVE vrzno_expose_object(zval *zv)
{
	return Z_OBJ_P(zv);
}

zend_array* EMSCRIPTEN_KEEPALIVE vrzno_expose_array(zval *zv)
{
	return Z_ARR_P(zv);
}

zend_object* EMSCRIPTEN_KEEPALIVE vrzno_expose_closure(zend_function *zf)
{
	return ZEND_CLOSURE_OBJECT(zf);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_key_pointer(zend_array *za, char *key)
{
	zend_string *zKey = zend_string_init(key, strlen(key), 0);
	zval *rv = zend_hash_find(za, zKey);
	zend_string_release(zKey);
	return rv;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_property_pointer(zend_object *zo, char *name)
{
	zval *rv = NULL;
	return zend_read_property(zo->ce, zo, name, strlen(name), 1, rv);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_pointer(zend_array *za, unsigned offset)
{
	return zend_hash_index_find(za, offset);
}

zend_function* EMSCRIPTEN_KEEPALIVE vrzno_expose_method_pointer(zend_object *zo, char *method)
{
	zend_string *zMethod = zend_string_init(method, strlen(method), 0);
	zend_function *zf = zend_std_get_method(&zo, zMethod, 0);
	zend_string_release(zMethod);
	return zf;
}
