#include "vrzno_private.h"

zend_class_entry *vrzno_class_entry;
zend_object_handlers vrzno_object_handlers;

vrzno_object *vrzno_fetch_object(zend_object *obj)
{
	return (vrzno_object*)((char*)(obj) - XtOffsetOf(vrzno_object, zo));
}

zend_object *vrzno_create_object(zend_class_entry *class_type)
{
	vrzno_object *vrzno = zend_object_alloc(sizeof(vrzno_object), class_type);

	zend_object_std_init(&vrzno->zo, class_type);
	object_properties_init(&vrzno->zo, class_type);

	vrzno->zo.handlers = &vrzno_object_handlers;
	vrzno->targetId = (vrzno_target_id) EM_ASM_INT({
		const _class = Module._classes.get($0);

		if(_class)
		{
			return 0;
		}

		return Module.targets.add(globalThis);
	}, class_type);

	return &vrzno->zo;
}

static struct _zend_class_entry *vrzno_create_class(vrzno_target_id targetId)
{
	zend_class_entry ce;
	zend_class_entry *vrzno_subclass_entry;

	char name[256];

	snprintf(name, 256, "Vrzno:::{%u}", targetId);

	INIT_CLASS_ENTRY(ce, name, NULL);
	vrzno_subclass_entry = zend_register_internal_class_ex(&ce, vrzno_class_entry);

	return vrzno_subclass_entry;
}

vrzno_object *vrzno_create_object_for_target(vrzno_target_id targetId, bool isConstructor)
{
	zend_class_entry *ce = vrzno_class_entry;

	if(isConstructor)
	{
		zend_class_entry *existing = EM_ASM_PTR({
			const target = Module.targets.get($0);
			return Module.classes.get(target);
		}, targetId);

		if(!existing)
		{
			ce = vrzno_create_class(targetId);

			EM_ASM({
				const target = Module.targets.get($0);
				Module.classes.set(target, $1);
				Module._classes.set($1, target);
			}, targetId, ce);
		}
		else
		{
			ce = existing;
		}
	}

	vrzno_object *vrzno = zend_object_alloc(sizeof(vrzno_object), ce);

	zend_object_std_init(&vrzno->zo, ce);
	object_properties_init(&vrzno->zo, ce);

	vrzno->zo.handlers = &vrzno_object_handlers;
	vrzno->targetId = targetId;

	return vrzno;
}


void vrzno_object_free(zend_object *zobj)
{
	EM_ASM({
		if($0)
		{
			Module.targets.remove($0);
		}
	}, vrzno_fetch_object(zobj)->targetId);

	zend_object_std_dtor(zobj);
}

zval *vrzno_read_property(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	vrzno_target_id targetId = vrzno->targetId;
	char *name = ZSTR_VAL(member);
	ZVAL_NULL(rv);

	EM_ASM({
		try
		{
			const target = Module.targets.get($0);
			const property = UTF8ToString($1);
			Module.jsToZval(target[property], $2);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}

	}, targetId, name, rv);

	return rv;
}

zval *vrzno_write_property(zend_object *object, zend_string *member, zval *newValue, void **cache_slot)
{
	char *name = ZSTR_VAL(member);
	vrzno_object *vrzno = vrzno_fetch_object(object);
	vrzno_target_id targetId = vrzno->targetId;

	EM_ASM({
		try
		{
			const target = Module.targets.get($0);
			const property = UTF8ToString($1);

			if($3 === 0)
			{
				delete target[property];
				return;
			}

			target[property] = Module.zvalToJS($2);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}
	}, targetId, name, newValue, Z_TYPE_P(newValue));

	return newValue;
}

zval *vrzno_read_dimension(zend_object *object, zval *offset, int type, zval *rv)
{
	ZVAL_NULL(rv);

	if(!offset)
	{
		return rv;
	}

	if(Z_TYPE_P(offset) == IS_LONG)
	{
		EM_ASM({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				Module.jsToZval(target[$1], $2);
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
			}
		}, vrzno_fetch_object(object)->targetId, Z_LVAL_P(offset), rv);
	}
	else if(Z_TYPE_P(offset) == IS_STRING)
	{
		EM_ASM({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				Module.jsToZval(target[UTF8ToString($1)], $2);
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
			}
		}, vrzno_fetch_object(object)->targetId, Z_STRVAL_P(offset), rv);
	}
	else
	{
		zend_type_error("Vrzno offsets must be integers or strings");
	}

	return rv;
}

void vrzno_write_dimension(zend_object *object, zval *offset, zval *newValue)
{
	vrzno_target_id targetId = vrzno_fetch_object(object)->targetId;

	if(!offset)
	{
		EM_ASM({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				target[target.length] = Module.zvalToJS($1);
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
			}
		}, targetId, newValue);
	}
	else if(Z_TYPE_P(offset) == IS_LONG)
	{
		EM_ASM({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				target[$1] = Module.zvalToJS($2);
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
			}
		}, targetId, Z_LVAL_P(offset), newValue);
	}
	else if(Z_TYPE_P(offset) == IS_STRING)
	{
		EM_ASM({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				target[UTF8ToString($1)] = Module.zvalToJS($2);
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
			}
		}, targetId, Z_STRVAL_P(offset), newValue);
	}
	else
	{
		zend_type_error("Vrzno offsets must be integers or strings");
	}
}

int vrzno_has_dimension(zend_object *object, zval *offset, int check_empty)
{
	vrzno_target_id targetId = vrzno_fetch_object(object)->targetId;

	if(!offset)
	{
		return 0;
	}

	if(Z_TYPE_P(offset) == IS_LONG)
	{
		return EM_ASM_INT({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				const value = target[$1];
				return $2 ? Module.vrznoPhpTruthy(value) : value !== null && value !== undefined;
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
				return 0;
			}
		}, targetId, Z_LVAL_P(offset), check_empty);
	}

	if(Z_TYPE_P(offset) == IS_STRING)
	{
		return EM_ASM_INT({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				const value = target[UTF8ToString($1)];
				return $2 ? Module.vrznoPhpTruthy(value) : value !== null && value !== undefined;
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
				return 0;
			}
		}, targetId, Z_STRVAL_P(offset), check_empty);
	}

	return 0;
}

void vrzno_unset_property(zend_object *object, zend_string *member, void **cache_slot)
{
	EM_ASM({ (() =>{
		try
		{
			const target = Module.targets.get($0);
			const property = UTF8ToString($1);
			delete target[property];
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}
	})() }, vrzno_fetch_object(object)->targetId, ZSTR_VAL(member));
}

void vrzno_unset_dimension(zend_object *object, zval *offset)
{
	if(!offset)
	{
		return;
	}

	if(Z_TYPE_P(offset) == IS_LONG)
	{
		EM_ASM({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				delete target[$1];
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
			}
		}, vrzno_fetch_object(object)->targetId, Z_LVAL_P(offset));
	}
	else if(Z_TYPE_P(offset) == IS_STRING)
	{
		EM_ASM({
			try
			{
				const target = Module.vrznoArrayView(Module.targets.get($0));
				delete target[UTF8ToString($1)];
			}
			catch(error)
			{
				Module.vrznoThrowRuntimeError(error);
			}
		}, vrzno_fetch_object(object)->targetId, Z_STRVAL_P(offset));
	}
}

HashTable *vrzno_get_properties_for(zend_object *object, zend_prop_purpose purpose)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	vrzno_target_id targetId = vrzno->targetId;

	char *js_ret = EM_ASM_PTR({
		const target = Module.targets.get($0);
		let json;

		if(typeof target === 'function')
		{
			json = JSON.stringify({});
		}
		else
		{
			try{ json = JSON.stringify({...target}); }
			catch { json = JSON.stringify({}); }
		}

		const str = String(json);
		const len = 1 + lengthBytesUTF8(str);
		const loc = _malloc(len);

		stringToUTF8(str, loc, len);

		return loc;

	}, targetId);

	zval js_object;
	ZVAL_UNDEF(&js_object);

	if(php_json_decode_ex(&js_object, js_ret, strlen(js_ret), PHP_JSON_OBJECT_AS_ARRAY, PHP_JSON_PARSER_DEFAULT_DEPTH) == FAILURE
		|| Z_TYPE(js_object) != IS_ARRAY)
	{
		if(Z_TYPE(js_object) != IS_UNDEF)
		{
			zval_ptr_dtor(&js_object);
		}
		array_init(&js_object);
	}

	free(js_ret);

	return Z_ARR(js_object);
}

int vrzno_has_property(zend_object *object, zend_string *member, int has_set_exists, void **cache_slot)
{
	return EM_ASM_INT({
		try
		{
			const target = Module.targets.get($0);
			const property = UTF8ToString($1);
			const mode = $2;

			if(!Reflect.has(target, property))
			{
				return false;
			}

			if(mode === 2)
			{
				return true;
			}

			const value = target[property];
			return mode === 1
				? Module.vrznoPhpTruthy(value)
				: value !== null && value !== undefined;
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
			return false;
		}
	}, vrzno_fetch_object(object)->targetId, ZSTR_VAL(member), has_set_exists);
}

PHP_METHOD(Vrzno, __get)
{
	zval *object = getThis();
	zend_object *zObject = Z_OBJ_P(object);
	vrzno_object *vrzno = vrzno_fetch_object(zObject);

	char *js_property_name = "";
	size_t js_property_name_len = 0;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_property_name, js_property_name_len)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_NULL(return_value);
	EM_ASM({
		try
		{
			const target = Module.targets.get($0);
			const propertyName = UTF8ToString($1);
			Module.jsToZval(target[propertyName], $2);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}
	}, vrzno->targetId, js_property_name, return_value);

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_METHOD(Vrzno, __call)
{
	zval *object = getThis();
	zend_object *zObject = Z_OBJ_P(object);
	vrzno_object *vrzno = vrzno_fetch_object(zObject);

	HashTable *argv;

	char *method_name = "";
	size_t method_name_len = 0;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(method_name, method_name_len)
		Z_PARAM_ARRAY_HT(argv)
	ZEND_PARSE_PARAMETERS_END();

	size_t size = sizeof(zval*);
	int argc = zend_hash_num_elements(argv);
	int i = 0;

	zval **args = argc ? safe_emalloc((size_t) argc, sizeof(zval*), 0) : NULL;

	if(argc)
	{
		zval *arg;

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_PACKED_FOREACH_VAL(argv, arg) {
#else
		ZEND_HASH_FOREACH_VAL(argv, arg) {
#endif
			args[i] = arg;
			i++;
		} ZEND_HASH_FOREACH_END();
	}

	ZVAL_NULL(return_value);
	EM_ASM({
		try
		{
			const target = Module.targets.get($0);
			const methodName = UTF8ToString($1);
			const argp = $2;
			const argc = $3;
			const size = $4;
			const args = [];

			for(let i = 0; i < argc; i++)
			{
				const loc = argp + i * size;
				const ptr = Module.getValue(loc, '*');
				args.push(Module.zvalToJS(ptr));
			}

			Module.jsToZval(target[methodName](...args), $5);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}

	}, vrzno->targetId, method_name, args, argc, size, return_value);

	if(args)
	{
		efree(args);
	}

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_METHOD(Vrzno, __invoke)
{
	zval *object = getThis();
	zend_object *zObject = Z_OBJ_P(object);
	vrzno_object *vrzno = vrzno_fetch_object(zObject);;

	int argc = 0;
	zval *argv;

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', argv, argc)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_NULL(return_value);
	EM_ASM({
		try
		{
			const target = Module.targets.get($0);
			const argv = $1;
			const argc = $2;
			const size = $3;
			const args = [];

			for(let i = 0; i < argc; i++)
			{
				args.push(Module.zvalToJS(argv + i * size));
			}

			Module.jsToZval(target(...args), $4);
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
		}

	}, vrzno->targetId, argv, argc, sizeof(zval), return_value);

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_METHOD(Vrzno, __construct)
{
	zval *object = getThis();
	zend_object *zObject = Z_OBJ_P(object);
	vrzno_object *vrzno = vrzno_fetch_object(zObject);

	if(Z_OBJCE_P(object) == vrzno_class_entry)
	{
		return;
	}

	zval *argv;
	int argc = 0;

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', argv, argc)
	ZEND_PARSE_PARAMETERS_END();

	vrzno->targetId = EM_ASM_INT({
		try
		{
			const constructor = Module._classes.get($0);
			const argv = $1;
			const argc = $2;
			const size = $3;
			const args = [];

			for(let i = 0; i < argc; i++)
			{
				args.push(Module.zvalToJS(argv + i * size));
			}

			const instance = new constructor(...args);
			const index = Module.targets.add(instance);
			Module.tacked.add(instance);

			return index;
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
			return 0;
		}

	}, Z_OBJCE_P(object), argv, argc, sizeof(zval));

	if(EG(exception))
	{
		RETURN_THROWS();
	}
}

PHP_METHOD(Vrzno, __toString)
{
	zval *object = getThis();
	zend_object *zObject = Z_OBJ_P(object);
	vrzno_object *vrzno = vrzno_fetch_object(zObject);

	if(zend_parse_parameters_none() == FAILURE)
	{
		RETURN_THROWS();
	}

	char *str = EM_ASM_PTR({
		try
		{
			const target = Module.targets.get($0);
			const str = String(target);
			const len = 1 + lengthBytesUTF8(str);
			const loc = _malloc(len);

			stringToUTF8(str, loc, len);

			return loc;
		}
		catch(error)
		{
			Module.vrznoThrowRuntimeError(error);
			return 0;
		}
	}, vrzno->targetId);

	if(!str)
	{
		RETURN_THROWS();
	}

	zend_string *zstr = zend_string_init(str, strlen(str), 0);
	RETVAL_STR(zstr);
	free(str);
}
