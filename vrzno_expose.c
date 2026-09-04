#include "vrzno_private.h"

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

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_long(long value, zval *rv)
{
	ZVAL_LONG(rv, value);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_double(double value, zval *rv)
{
	ZVAL_DOUBLE(rv, value);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_string(char *value, size_t length, zval *rv)
{
	ZVAL_STRINGL(rv, value, length);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_create_object_for_target(vrzno_target_id targetId, int isConstructor, zval *rv)
{
	vrzno_object *vObj = vrzno_create_object_for_target(targetId, isConstructor);
	ZVAL_OBJ(rv, &vObj->zo);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_create_params(int argc)
{
	zval *zvals;

	if(argc <= 0)
	{
		return NULL;
	}

	zvals = safe_emalloc((size_t) argc, sizeof(zval), 0);

	for(int i = 0; i < argc; i++)
	{
		ZVAL_UNDEF(&zvals[i]);
	}

	return zvals;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_param_at(zval *params, int index)
{
	return &params[index];
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_destroy_params(zval *params, int argc)
{
	if(!params)
	{
		return;
	}

	for(int i = 0; i < argc; i++)
	{
		if(Z_TYPE(params[i]) != IS_UNDEF)
		{
			zval_ptr_dtor(&params[i]);
		}
	}

	efree(params);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_copy_zval(zval *source)
{
	zval *owned = emalloc(sizeof(zval));
	ZVAL_COPY(owned, source);
	return owned;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_copy_object(zend_object *source)
{
	zval *owned = emalloc(sizeof(zval));
	ZVAL_OBJ_COPY(owned, source);
	return owned;
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_copy_into(zval *destination, zval *source)
{
	ZVAL_COPY(destination, source);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_type_error(const char *message)
{
	zend_type_error("%s", message);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_runtime_error(const char *message)
{
	zend_throw_exception(spl_ce_RuntimeException, message, 0);
}

void EMSCRIPTEN_KEEPALIVE vrzno_expose_destroy_zval(zval *zv)
{
	if(!zv)
	{
		return;
	}

	if(Z_TYPE_P(zv) != IS_UNDEF)
	{
		zval_ptr_dtor(zv);
	}

	efree(zv);
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
	zend_ulong index;

	ZEND_HASH_FOREACH_KEY(properties, index, key) {
		(void) index;
		if(key)
		{
			add_next_index_stringl(&keys, ZSTR_VAL(key), ZSTR_LEN(key));
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
	zval_ptr_dtor(&keys);

	return json;
}

char* EMSCRIPTEN_KEEPALIVE vrzno_expose_array_keys(zend_array *za)
{
	zval keys;
	array_init(&keys);
	zend_string *string_key;
	zend_ulong num_key;

	ZEND_HASH_FOREACH_KEY(za, num_key, string_key) {
		if(string_key)
		{
			add_next_index_stringl(&keys, ZSTR_VAL(string_key), ZSTR_LEN(string_key));
		}
		else
		{
			zend_string *numeric_key = zend_long_to_str((zend_long) num_key);
			add_next_index_str(&keys, numeric_key);
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
	zval_ptr_dtor(&keys);

	return json;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_deref(zval* zv)
{
	ZVAL_DEREF(zv);
	return zv;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_direct(zval* zv)
{
	return Z_INDIRECT_P(zv);
}

uint8_t EMSCRIPTEN_KEEPALIVE vrzno_expose_type(zval *zv)
{
	return Z_TYPE_P(zv);
}

uint32_t EMSCRIPTEN_KEEPALIVE vrzno_expose_array_length(zend_array *za)
{
	return zend_hash_num_elements(za);
}

vrzno_target_id EMSCRIPTEN_KEEPALIVE vrzno_expose_zval_target(zval *zv)
{
	if(Z_TYPE_P(zv) != IS_OBJECT)
	{
		return 0;
	}

	if(instanceof_function(Z_OBJCE_P(zv), vrzno_class_entry))
	{
		return vrzno_fetch_object(Z_OBJ_P(zv))->targetId;
	}

	return 0;
}

vrzno_target_id EMSCRIPTEN_KEEPALIVE vrzno_expose_target(zend_object *zo)
{
	if(instanceof_function(zo->ce, vrzno_class_entry))
	{
		return vrzno_fetch_object(zo)->targetId;
	}

	return 0;
}

zend_function* EMSCRIPTEN_KEEPALIVE vrzno_expose_callable(zval *zv)
{
	zend_fcall_info_cache fcc;

	if(zend_is_callable_ex(zv, NULL, 0, NULL, &fcc, NULL))
	{
		return fcc.function_handler;
	}

	return NULL;
}

zend_long EMSCRIPTEN_KEEPALIVE vrzno_expose_long(zval *zv)
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

size_t EMSCRIPTEN_KEEPALIVE vrzno_expose_string_length(zval *zv)
{
	return Z_STRLEN_P(zv);
}

zend_object* EMSCRIPTEN_KEEPALIVE vrzno_expose_object(zval *zv)
{
	return Z_OBJ_P(zv);
}

zend_array* EMSCRIPTEN_KEEPALIVE vrzno_expose_array(zval *zv)
{
	return Z_ARR_P(zv);
}

zend_resource* EMSCRIPTEN_KEEPALIVE vrzno_expose_resource(zval *zv)
{
	return Z_RES_P(zv);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_key_pointer(zend_array *za, char *key)
{
	zend_string *zKey = zend_string_init(key, strlen(key), 0);
	zval *rv = zend_hash_find(za, zKey);
	zend_string_release(zKey);
	return rv;
}

int EMSCRIPTEN_KEEPALIVE vrzno_expose_has_property(zend_object *zo, char *name)
{
	zend_string *property = zend_string_init(name, strlen(name), 0);
	int result = zo->handlers->has_property(zo, property, ZEND_PROPERTY_EXISTS, NULL);
	zend_string_release(property);
	return result;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_read_property(zend_object *zo, char *name)
{
	zval *owned = emalloc(sizeof(zval));
	ZVAL_UNDEF(owned);

	zval *value = zend_read_property(zo->ce, zo, name, strlen(name), 1, owned);

	if(!value || Z_TYPE_P(value) == IS_UNDEF)
	{
		vrzno_expose_destroy_zval(owned);
		return NULL;
	}

	if(value != owned)
	{
		ZVAL_COPY(owned, value);
	}

	return owned;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_dimension_pointer(zend_array *za, unsigned offset)
{
	return zend_hash_index_find(za, offset);
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_expose_array_value_at(zend_array *za, uint32_t offset)
{
	uint32_t index = 0;
	zval *value;

	ZEND_HASH_FOREACH_VAL(za, value) {
		if(index++ == offset)
		{
			return value;
		}
	} ZEND_HASH_FOREACH_END();

	return NULL;
}

zend_function* EMSCRIPTEN_KEEPALIVE vrzno_expose_method_pointer(zend_object *zo, char *method)
{
	zend_string *zMethod = zend_string_init(method, strlen(method), 0);
	zend_function *zf = zend_std_get_method(&zo, zMethod, 0);
	zend_string_release(zMethod);
	return zf;
}
