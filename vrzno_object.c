zend_class_entry *vrzno_class_entry;
zend_object_handlers vrzno_object_handlers;

ZEND_BEGIN_ARG_INFO_EX(arginfo___invoke, 0, 0, -1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___call, 0, 0, 2)
	ZEND_ARG_INFO(0, method_name)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___get, 0, 0, 1)
	ZEND_ARG_INFO(0, property_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___construct, 0, 0, -1)
ZEND_END_ARG_INFO()

static const zend_function_entry vrzno_vrzno_methods[] = {
	PHP_ME(Vrzno, __invoke,    arginfo___invoke,    ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __call,      arginfo___call,      ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __get,       arginfo___get,       ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __construct, arginfo___construct, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

vrzno_object *vrzno_fetch_object(zend_object *obj) {
	return (vrzno_object*)((char*)(obj) - XtOffsetOf(vrzno_object, zo));
}

static struct _zend_object *vrzno_create_object(zend_class_entry *class_type)
{
	vrzno_object *vrzno = zend_object_alloc(sizeof(vrzno_object), class_type);

    zend_object_std_init(&vrzno->zo, class_type);

    vrzno->zo.handlers = &vrzno_object_handlers;
    vrzno->targetId = (long) EM_ASM_INT({
		const _class = Module._classes.get($0);

		if(_class)
		{
			return Module.targets.getId(_class);
		}

		return Module.targets.getId(globalThis);
	}, class_type);

	vrzno->isConstructor = 0;
	vrzno->isFunction = 0;

	return &vrzno->zo;
}

static struct _zend_class_entry *vrzno_create_class(long targetId)
{
	zend_class_entry ce;
	zend_class_entry *vrzno_subclass_entry;

	char name[256];

	snprintf(name, 256, "Vrzno:::{%ld}", targetId);

	INIT_CLASS_ENTRY(ce, name, vrzno_vrzno_methods);
	vrzno_subclass_entry = zend_register_internal_class(&ce);

	vrzno_subclass_entry->create_object = vrzno_create_object;
	vrzno_subclass_entry->get_iterator  = vrzno_array_get_iterator;
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
	vrzno_subclass_entry->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

	vrzno_subclass_entry->ce_flags |= ZEND_ACC_LINKED;

	vrzno_subclass_entry->parent = vrzno_class_entry;

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
	zend_string *attribute_name_AllowDynamicProperties_class_vrzno = zend_string_init_interned("AllowDynamicProperties", sizeof("AllowDynamicProperties") - 1, 1);
	zend_add_class_attribute(vrzno_subclass_entry, attribute_name_AllowDynamicProperties_class_vrzno, 0);
	zend_string_release(attribute_name_AllowDynamicProperties_class_vrzno);
#endif

	return vrzno_subclass_entry;
}

static vrzno_object *vrzno_create_object_for_target(int target_id, int isFunction, bool isConstructor)
{
	zend_class_entry *ce = vrzno_class_entry;

	if(isConstructor)
	{
		zend_class_entry *existing = EM_ASM_INT({
			const target = Module.targets.get($0);
			return Module.classes.get(target);
		}, target_id);

		if(!existing)
		{
			ce = vrzno_create_class(target_id);

			EM_ASM({
				const target = Module.targets.get($0);
				Module.classes.set(target, $1);
				Module._classes.set($1, target);
			}, target_id, ce);
		}
		else
		{
			ce = existing;
		}
	}

	vrzno_object *vrzno = zend_object_alloc(sizeof(vrzno_object), ce);

    zend_object_std_init(&vrzno->zo, ce);

	vrzno->zo.handlers = &vrzno_object_handlers;
    vrzno->targetId = target_id;
	vrzno->isConstructor = isConstructor;
	vrzno->isFunction = isFunction;

	return vrzno;
}

static void vrzno_object_free(zend_object *zobj)
{
	// vrzno_object *vrzno = vrzno_fetch_object(zobj);
	// EM_ASM({
	// 	console.log('FREE', $0, $1, $2);
	// 	// Module.fRegistry.unregister(Module.targets.get($1));
	// 	// Module.targets.remove($1);
	// }, vrzno, zobj, &vrzno->zo);

	zend_object_std_dtor(zobj);
}

zval *vrzno_read_property(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;
	char *name = ZSTR_VAL(member);

	zval *scalar_result = EM_ASM_PTR({
		const target = Module.targets.get($0);
		const property = UTF8ToString($1);

		if(!(property in target))
		{
			return Module.jsToZval(undefined);
		}

		if(target[property] === null)
		{
			return Module.jsToZval(null);
		}

		const result = target[property];

		if(!result || !['function','object'].includes(typeof result))
		{
			return Module.jsToZval(result);
		}

		return 0;

	}, targetId, name);

	if(scalar_result)
	{
		ZVAL_COPY(rv, scalar_result);
		return rv;
	}

	free(scalar_result);

	int obj_index = EM_ASM_INT({
		const target   = Module.targets.get($0);
		const property = UTF8ToString($1);
		const result   = target[property];
		const zvalPtr  = $2;

		if(result && ['function','object'].includes(typeof result))
		{
			let index = Module.targets.getId(result);

			if(!Module.targets.has(result))
			{
				index = Module.targets.add(result);
				Module.zvalMap.set(result, zvalPtr);
				Module._zvalMap.set(zvalPtr, result);
			}

			return index;
		}

		return 0;

	}, targetId, name, rv);

	if(!obj_index)
	{
		ZVAL_BOOL(rv, obj_index);
		return rv;
	}

	int isFunction = EM_ASM_INT({ return 'function' === typeof (Module.targets.get($0)) ? $0 : 0; }, obj_index);
	int isConstructor = 0;

	if(isFunction)
	{
		isConstructor = (bool) EM_ASM_INT({
			return !!((Module.targets.get($0)).prototype && (Module.targets.get($0)).prototype.constructor);
		}, obj_index);
	}

	vrzno_object *retVrzno = vrzno_create_object_for_target(obj_index, isFunction, isConstructor);

	ZVAL_OBJ(rv, &retVrzno->zo);
	return rv;
}

zval *vrzno_write_property(zend_object *object, zend_string *member, zval *newValue, void **cache_slot)
{
	char *name = ZSTR_VAL(member);
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;

	char *errstr = NULL;
	zend_fcall_info_cache fcc;

	bool isCallable = zend_is_callable_ex(newValue, NULL, 0, NULL, &fcc, &errstr);

	if(Z_TYPE_P(newValue) == IS_OBJECT && Z_OBJCE_P(newValue) == vrzno_class_entry)
	{
		isCallable = vrzno_fetch_object(Z_OBJ_P(newValue))->isFunction;
	}

	if(Z_TYPE_P(newValue) == IS_OBJECT && Z_OBJCE_P(newValue)->parent == vrzno_class_entry)
	{
		isCallable = vrzno_fetch_object(Z_OBJ_P(newValue))->isFunction;
	}

	if(isCallable)
	{
		Z_ADDREF_P(newValue);

		EM_ASM({ (() =>{
			const target   = Module.targets.get($0);
			const property = UTF8ToString($1);
			const funcPtr  = $2;
			const zvalPtr  = $3;
			const paramCount = $4;

			target[property] = Module.callableToJs(funcPtr);

			// Module.fRegistry.register(target[property], zvalPtr, target[property]);

		})() }, targetId, name, fcc.function_handler, newValue, fcc.function_handler->common.num_args);

		return newValue;
	}

	switch(Z_TYPE_P(newValue))
	{
		case IS_OBJECT:
			Z_ADDREF_P(newValue);
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				const zvalPtr = $2;

				if(!Module.targets.has(target[property]))
				{
					target[property] = Module.marshalObject(zvalPtr);
				}

			})() }, targetId, name, newValue);
			break;

		case IS_UNDEF:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				delete target[property];
			})() }, targetId, name);
			break;

		case IS_NULL:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				target[property] = null;
			})() }, targetId, name);
			break;

		case IS_FALSE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				target[property] = false;
			})() }, targetId, name);
			break;

		case IS_TRUE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				target[property] = true;
			})() }, targetId, name);
			break;

		case IS_DOUBLE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				target[property] = $2;
			})() }, targetId, name, Z_DVAL_P(newValue));
			break;

		case IS_LONG:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				target[property] = $2;
			})() }, targetId, name, Z_LVAL_P(newValue));
			break;

		case IS_STRING:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				const newValue = UTF8ToString($2);
				target[property] = newValue;
			})() }, targetId, name, Z_STRVAL_P(newValue));
			break;
	}

	return newValue;
}

zval *vrzno_read_dimension(zend_object *object, zval *offset, int type, zval *rv)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;
	long index = Z_LVAL_P(offset);

	char *scalar_result = EM_ASM_INT({

		let target = Module.targets.get($0);
		const property = $1;

		if(target instanceof ArrayBuffer)
		{
			if(!Module.bufferMaps.has(target))
			{
				Module.bufferMaps.set(target, new Uint8Array(target));
			}

			target = Module.bufferMaps.get(target);
		}

		if(!(property in target))
		{
			const jsRet    = 'UN';
			const len      = lengthBytesUTF8(jsRet) + 1;
			const strLoc   = _malloc(len);

			stringToUTF8(jsRet, strLoc, len);

			return strLoc;
		}

		if(target[property] === null)
		{
			const jsRet    = 'NU';
			const len      = lengthBytesUTF8(jsRet) + 1;
			const strLoc   = _malloc(len);

			stringToUTF8(jsRet, strLoc, len);

			return strLoc;
		}

		const result = target[property];

		if(!result || !['function','object'].includes(typeof result))
		{
			const jsRet  = 'OK' + String(result);
			const len    = lengthBytesUTF8(jsRet) + 1;
			const strLoc = _malloc(len);

			stringToUTF8(jsRet, strLoc, len);

			return strLoc;
		}

		const jsRet   = 'XX';
		const len    = lengthBytesUTF8(jsRet) + 1;
		const strLoc = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId, index);

	if(strlen(scalar_result) > 1 && *scalar_result == 'O')
	{
		ZVAL_STRING(rv, scalar_result + 2);
		free(scalar_result);
		return rv;
	}

	if(strlen(scalar_result) > 1 && *scalar_result == 'U')
	{
		ZVAL_UNDEF(rv);
		free(scalar_result);
		return rv;
	}
	else if(strlen(scalar_result) > 1 && *scalar_result == 'N')
	{
		ZVAL_NULL(rv);
		free(scalar_result);
		return rv;
	}
	else if(strlen(scalar_result) > 1 && scalar_result[0] == 'O')
	{
		ZVAL_STRING(rv, scalar_result + 2);
		free(scalar_result);
		return rv;
	}

	free(scalar_result);

	int obj_result = EM_ASM_INT({
		const target   = Module.targets.get($0);
		const property = UTF8ToString($1);
		const result   = target[property];
		const zvalPtr  = $2;

		if(result && ['function','object'].includes(typeof result))
		{
			let index = Module.targets.getId(result);

			if(!Module.targets.has(result))
			{
				index = Module.targets.add(result);
				Module.zvalMap.set(result, zvalPtr);
				Module._zvalMap.set(zvalPtr, result);
			}

			return index;
		}

		return 0;

	}, targetId, index, rv);

	if(!obj_result)
	{
		ZVAL_BOOL(rv, obj_result);
		return rv;
	}

	int isFunction = EM_ASM_INT({ return 'function' === typeof (Module.targets.get($0)) ? $0 : 0; }, obj_result);
	int isConstructor = 0;

	if(isFunction)
	{
		isConstructor = (bool) EM_ASM_INT({
			return !!((Module.targets.get($0)).prototype && (Module.targets.get($0)).prototype.constructor);
		}, obj_result);
	}

	vrzno_object *retVrzno = vrzno_create_object_for_target(obj_result, isFunction, isConstructor);

	ZVAL_OBJ(rv, &retVrzno->zo);
	return rv;
}

void vrzno_write_dimension(zend_object *object, zval *offset, zval *newValue)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;
	long index = Z_LVAL_P(offset);

	zend_fcall_info_cache fcc;
	char *errstr = NULL;

	bool isCallable = zend_is_callable_ex(newValue, NULL, 0, NULL, &fcc, &errstr);

	if(Z_OBJCE_P(newValue) == vrzno_class_entry)
	{
		isCallable = vrzno_fetch_object(Z_OBJ_P(newValue))->isFunction;
	}

	if(isCallable)
	{
		Z_ADDREF_P(newValue);

		EM_ASM({ (() =>{
			const target   = Module.targets.get($0);
			const property = $1;
			const funcPtr  = $2;
			const zvalPtr  = $3;

			target[property] = Module.callableToJs(funcPtr);
			// Module.fRegistry.register(target[property], zvalPtr, target[property]);
		})() }, targetId, index, fcc.function_handler, newValue);

		return;
	}

	switch(Z_TYPE_P(newValue))
	{
		case IS_OBJECT:

			// Z_ADDREF_P(newValue);

			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				const zvalPtr = $2;

				if(!Module.targets.has(target[property]))
				{
					target[property] = Module.marshalObject(zvalPtr);
				}

			})() }, targetId, index, newValue);
			break;

		case IS_UNDEF:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				delete target[property];
			})() }, targetId, index);
			break;

		case IS_NULL:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				target[property] = null;
			})() }, targetId, index);
			break;

		case IS_FALSE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				target[property] = false;
			})() }, targetId, index);
			break;

		case IS_TRUE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				target[property] = true;
			})() }, targetId, index);
			break;

		case IS_DOUBLE:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				target[property] = $2;
			})() }, targetId, index, Z_DVAL_P(newValue));
			break;

		case IS_LONG:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				target[property] = $2;
			})() }, targetId, index, Z_LVAL_P(newValue));
			break;

		case IS_STRING:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				const newValue = UTF8ToString($2);
				target[property] = newValue;
			})() }, targetId, index, Z_STRVAL_P(newValue));
			break;
	}
}

int vrzno_has_dimension(zend_object *object, zval *offset, int check_empty)
{
	long index = Z_LVAL_P(offset);
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;

	return EM_ASM_INT({
		console.log('HASD', $0);

		const target      = Module.targets.get($0);
		const property    = $1;
		const check_empty = $2;

		if(Array.isArray(target))
		{
			return typeof target[property] !== 'undefined';
		}

		if(target instanceof ArrayBuffer)
		{
			if(!Module.bufferMaps.has(target))
			{
				Module.bufferMaps.set(target, new Uint8Array(target));
			}

			const targetBytes = Module.bufferMaps.get(target);

			return targetBytes[property] !== 'undefined';
		}

		if(!check_empty)
		{

			return property in target;
		}
		else
		{
			return !!target[property];
		}

	}, targetId, index, check_empty);
}

void vrzno_unset_property(zend_object *object, zend_string *member, void **cache_slot)
{
	char *name = ZSTR_VAL(member);
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;

	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = UTF8ToString($1);
		delete target[property];
	})() }, targetId, name);
}

void vrzno_unset_dimension(zend_object *object, zval *offset)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;
	long index = Z_LVAL_P(offset);

	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = $1;
		delete target[property];
	})() }, targetId, index);
}

HashTable *vrzno_get_properties_for(zend_object *object, zend_prop_purpose purpose)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;

	char *js_ret = EM_ASM_INT({
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

		const jsRet  = String(json);
		const len    = lengthBytesUTF8(jsRet) + 1;
		const strLoc = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId);

	php_json_parser parser;
	zval js_object;

	php_json_parser_init(&parser, &js_object, js_ret, strlen(js_ret), PHP_JSON_OBJECT_AS_ARRAY, PHP_JSON_PARSER_DEFAULT_DEPTH);

	if(php_json_yyparse(&parser))
	{
		// FAIL
	}

	free(js_ret);

	return Z_ARR(js_object);
}

int vrzno_has_property(zend_object *object, zend_string *member, int has_set_exists, void **cache_slot)
{
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	char *name = ZSTR_VAL(member);
	long targetId = vrzno->targetId;

	return EM_ASM_INT({
		const target   = Module.targets.get($0);
		const property = UTF8ToString($1);
		return  property in target;
	}, targetId, name);
}

zend_string *vrzno_get_class_name(const zend_object *object)
{
	vrzno_object *vrzno = vrzno_fetch_object((zend_object*) object);
	long targetId = vrzno->targetId;

	char *className = EM_ASM_INT({

		const target = Module.targets.get($0);
		const name = (target.constructor && target.constructor.name) || 'Object';
		const len = lengthBytesUTF8(name) + 1;
		const namePtr = _malloc(name);

		stringToUTF8(name, namePtr, len);

		return namePtr;

	}, targetId);

	zend_string *retVal = zend_string_init(className, strlen(className), 0);

	free(*className);

	return retVal;
}

PHP_METHOD(Vrzno, __call)
{
	zval *object = getThis();
	zend_object *zObject = object->value.obj;
	vrzno_object *vrzno = vrzno_fetch_object(zObject);
	long targetId = vrzno->targetId;

	HashTable *argv;

	char *method_name = "";
	size_t method_name_len = 0;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(method_name, method_name_len)
		Z_PARAM_ARRAY_HT(argv)
	ZEND_PARSE_PARAMETERS_END();

	int argc = zend_hash_num_elements(argv);
	int size = sizeof(zval*);
	int i = 0;

	zval *args[argc];

	if(argc)
	{
		zval *data;

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_PACKED_FOREACH_VAL(argv, data) {
#else
		ZEND_HASH_FOREACH_VAL(argv, data) {
#endif
			if(Z_REFCOUNTED_P(data))
			{
				args[i] = (zval*) emalloc(argc * sizeof(zval));
				ZVAL_UNDEF(args[i]);
				ZVAL_COPY_VALUE(args[i], data);
			}
			else
			{
				args[i] = data;
			}
			i++;
		} ZEND_HASH_FOREACH_END();
	}

	zval *js_ret = EM_ASM_INT({
		const target = Module.targets.get($0);
		const method_name = UTF8ToString($1);
		const argp = $2;
		const argc = $3;
		const size = $4;

		const args = [];

		for(let i = 0; i < argc; i++)
		{
			const loc = argp + i * size;
			const ptr = Module.getValue(loc, '*');
			const arg = Module.zvalToJS(ptr);
			args.push(arg);
		}

		const jsRet = target[method_name](...args);
		const retZval = Module.jsToZval(jsRet);

		return retZval;

	}, targetId, method_name, args, argc, size);

	if(!js_ret)
	{
		ZVAL_UNDEF(js_ret);
	}

	RETURN_ZVAL(js_ret, 0, 0);
}

PHP_METHOD(Vrzno, __invoke)
{
	zval *object = getThis();
	zend_object *zObject = object->value.obj;
	vrzno_object *vrzno = vrzno_fetch_object(zObject);
	long targetId = vrzno->targetId;

	int js_argc = 0;
	zval *js_argv;

	int size = sizeof(zval);

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', js_argv, js_argc)
	ZEND_PARSE_PARAMETERS_END();

	zval *zvalPtrs = NULL;
	int i = 0;

	if(js_argc)
	{
		zvalPtrs = (zval*) emalloc(js_argc * sizeof(zval));

		for(i = 0; i < js_argc; i++)
		{
			ZVAL_NULL(&zvalPtrs[i]);
			ZVAL_COPY(&zvalPtrs[i], &js_argv[i]);
		}
	}

	zval *js_ret = EM_ASM_INT({

		const target = Module.targets.get($0);
		const argv   = $1;
		const argc   = $2;
		const size   = $3;

		const args   = [];

		for(let i = 0; i < argc; i++)
		{
			args.push(Module.zvalToJS(argv + i * size));
		}

		const jsRet = target(...args);
		return Module.jsToZval(jsRet);

	}, targetId, zvalPtrs, js_argc, size);

	ZVAL_UNDEF(return_value);

	if(!js_ret)
	{
		return;
	}

	ZVAL_COPY(return_value, js_ret);
}

PHP_METHOD(Vrzno, __get)
{
	zval *object = getThis();
	zend_object *zObject = object->value.obj;
	vrzno_object *vrzno = vrzno_fetch_object(zObject);
	long targetId = vrzno->targetId;

	char *js_property_name = "";
	size_t js_property_name_len = 0;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_property_name, js_property_name_len)
	ZEND_PARSE_PARAMETERS_END();

	zval *js_ret = EM_ASM_INT({

		const target        = Module.targets.get($0);
		const property_name = UTF8ToString($1);

		target[property_name] = newValueJson;

		const jsRet = target[property_name];
		return Module.jsToZval(jsRet);

	}, targetId, js_property_name);

	ZVAL_UNDEF(return_value);
	ZVAL_COPY(return_value, js_ret);
}

PHP_METHOD(Vrzno, __construct)
{
	zval         *object   = getThis();
	zend_object  *zObject  = object->value.obj;
	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
	long          targetId = vrzno->targetId;

	if(Z_OBJCE_P(object) == vrzno_class_entry || targetId != 1)
	{
		return;
	}

	zval *argv;
	int argc = 0;

	ZEND_PARSE_PARAMETERS_START(0, -1)
		Z_PARAM_VARIADIC('*', argv, argc)
	ZEND_PARSE_PARAMETERS_END();

	int size = sizeof(zval);

	vrzno->targetId = EM_ASM_PTR({
		const _class = Module._classes.get($0);
		const argv   = $1;
		const argc   = $2;
		const size   = $3;
		const args   = [];

		for(let i = 0; i < argc; i++)
		{
			args.push(Module.zvalToJS(argv + i * size));
		}

		const _object = new _class(...args);
		const index = Module.targets.add(_object);

		return index;

	}, Z_OBJCE_P(object), argv, argc, size);
}
