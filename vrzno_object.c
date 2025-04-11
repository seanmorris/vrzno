zend_class_entry *vrzno_class_entry;
zend_object_handlers vrzno_object_handlers;

ZEND_BEGIN_ARG_INFO_EX(arginfo___get, 0, 0, 1)
	ZEND_ARG_INFO(0, property_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___call, 0, 0, 2)
	ZEND_ARG_INFO(0, method_name)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___invoke, 0, 0, -1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___construct, 0, 0, -1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___destruct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo___toString, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry vrzno_vrzno_methods[] = {
	PHP_ME(Vrzno, __get,       arginfo___get,       ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __call,      arginfo___call,      ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __invoke,    arginfo___invoke,    ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __construct, arginfo___construct, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __destruct,  arginfo___destruct,  ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __toString,  arginfo___toString,  ZEND_ACC_PUBLIC)
	PHP_FE_END
};

vrzno_object *vrzno_fetch_object(zend_object *obj)
{
	return (vrzno_object*)((char*)(obj) - XtOffsetOf(vrzno_object, zo));
}

static struct _zend_object *vrzno_create_object(zend_class_entry *class_type)
{
	vrzno_object *vrzno = zend_object_alloc(sizeof(vrzno_object), class_type);

    zend_object_std_init(&vrzno->zo, class_type);

    vrzno->zo.handlers = &vrzno_object_handlers;
    vrzno->targetId = (jstarget*) EM_ASM_INT({
		const _class = Module._classes.get($0);

		if(_class)
		{
			return Module.targets.getId(_class);
		}

		return Module.targets.add(globalThis);
	}, class_type);

	vrzno->isConstructor = 0;
	vrzno->isFunction = 0;

	return &vrzno->zo;
}

static struct _zend_class_entry *vrzno_create_class(jstarget *targetId)
{
	zend_class_entry ce;
	zend_class_entry *vrzno_subclass_entry;

	char name[256];

	snprintf(name, 256, "Vrzno:::{%p}", targetId);

	INIT_CLASS_ENTRY(ce, name, vrzno_vrzno_methods);
	vrzno_subclass_entry = zend_register_internal_class_ex(&ce, vrzno_class_entry);

	return vrzno_subclass_entry;
}

static vrzno_object *vrzno_create_object_for_target(jstarget *targetId, jstarget *isFunction, bool isConstructor)
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

	vrzno->zo.handlers = &vrzno_object_handlers;
    vrzno->targetId = targetId;
	vrzno->isConstructor = isConstructor;
	vrzno->isFunction = isFunction;

	return vrzno;
}


static void vrzno_object_free(zend_object *zobj)
{
	EM_ASM({
		const target = Module.targets.get($0);
		if(target)
		{
			Module.tacked.delete(target);
		}
	}, vrzno_fetch_object(zobj)->targetId);

	zend_object_std_dtor(zobj);
}

zval *vrzno_read_property(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	jstarget *targetId = vrzno->targetId;
	char *name = ZSTR_VAL(member);

	EM_ASM({
		const target = Module.targets.get($0);
		const property = UTF8ToString($1);
		const rv = $2;

		if(!(property in target))
		{
			return Module.jsToZval(undefined, rv);
		}

		Module.jsToZval(target[property], rv);

	}, targetId, name, rv);

	return rv;
}

zval *vrzno_write_property(zend_object *object, zend_string *member, zval *newValue, void **cache_slot)
{
	char *name = ZSTR_VAL(member);
	vrzno_object *vrzno = vrzno_fetch_object(object);
	jstarget *targetId = vrzno->targetId;

	bool isCallable = 0;
	char *errstr = NULL;
	zend_fcall_info_cache fcc;

	if(Z_TYPE_P(newValue) == IS_OBJECT && Z_OBJCE_P(newValue) == vrzno_class_entry)
	{
		isCallable = vrzno_fetch_object(Z_OBJ_P(newValue))->isFunction;
	}
	else if(Z_TYPE_P(newValue) == IS_OBJECT && Z_OBJCE_P(newValue)->parent == vrzno_class_entry)
	{
		isCallable = vrzno_fetch_object(Z_OBJ_P(newValue))->isFunction;
	}
	else if(Z_TYPE_P(newValue) != IS_STRING)
	{
		isCallable = zend_is_callable_ex(newValue, NULL, 0, NULL, &fcc, &errstr);
	}

	if(isCallable)
	{
		EM_ASM({ (() =>{
			const target   = Module.targets.get($0);
			const property = UTF8ToString($1);
			const funcPtr  = $2;
			const zvalPtr  = $3;
			const paramCount = $4;

			target[property] = Module.callableToJs(funcPtr);

			const gc = Module.ccall(
				'vrzno_expose_closure'
				, 'number'
				, ['number']
				, [funcPtr]
			);

			Module.refcountRegistry.register(target[property], gc, target[property]);

		})() }, targetId, name, fcc.function_handler, newValue, fcc.function_handler->common.num_args);

		return newValue;
	}

	switch(Z_TYPE_P(newValue))
	{
		case IS_OBJECT:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				const zvalPtr = $2;

				const zo = Module.ccall(
					'vrzno_expose_object'
					, 'number'
					, ['number']
					, [zvalPtr]
				);

				target[property] = Module.marshalZObject(zo);

			})() }, targetId, name, newValue);
			break;

		case IS_ARRAY:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = UTF8ToString($1);
				const zvalPtr = $2;

				const za = Module.ccall(
					'vrzno_expose_array'
					, 'number'
					, ['number']
					, [zvalPtr]
				);

				target[property] = Module.marshalZArray(za);

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
	EM_ASM({
		let target = Module.targets.get($0);
		const property = $1;
		const rv = $2;

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
			return Module.jsToZval(undefined, rv);
		}

		Module.jsToZval(target[property], rv);

	}, vrzno_fetch_object(object)->targetId, Z_LVAL_P(offset), rv);

	return rv;
}

void vrzno_write_dimension(zend_object *object, zval *offset, zval *newValue)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	jstarget *targetId = vrzno->targetId;
	long index = Z_LVAL_P(offset);

	bool isCallable = 0;
	char *errstr = NULL;
	zend_fcall_info_cache fcc;

	if(Z_OBJCE_P(newValue) == vrzno_class_entry)
	{
		isCallable = vrzno_fetch_object(Z_OBJ_P(newValue))->isFunction;
	}
	else if(Z_OBJCE_P(newValue)->parent == vrzno_class_entry)
	{
		isCallable = vrzno_fetch_object(Z_OBJ_P(newValue))->isFunction;
	}
	else if(Z_TYPE_P(newValue) != IS_STRING)
	{
		isCallable = zend_is_callable_ex(newValue, NULL, 0, NULL, &fcc, &errstr);
	}

	if(isCallable)
	{
		EM_ASM({ (() => {
			const target   = Module.targets.get($0);
			const property = $1;
			const funcPtr  = $2;
			const zvalPtr  = $3;

			target[property] = Module.callableToJs(funcPtr);

			const gc = Module.ccall(
				'vrzno_expose_closure'
				, 'number'
				, ['number']
				, [funcPtr]
			);

			Module.refcountRegistry.register(target[property], gc, target[property]);

		})() }, targetId, index, fcc.function_handler, newValue);

		return;
	}

	switch(Z_TYPE_P(newValue))
	{
		case IS_OBJECT:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				const zvalPtr = $2;

				const zo = Module.ccall(
					'vrzno_expose_object'
					, 'number'
					, ['number']
					, [zvalPtr]
				);

				target[property] = Module.marshalZObject(zo);

			})() }, targetId, index, newValue);
			break;

		case IS_ARRAY:
			EM_ASM({ (() =>{
				const target = Module.targets.get($0);
				const property = $1;
				const zvalPtr = $2;

				const za = Module.ccall(
					'vrzno_expose_array'
					, 'number'
					, ['number']
					, [zvalPtr]
				);

				target[property] = Module.marshalZArray(za);

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
	return EM_ASM_INT({
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

	}, vrzno_fetch_object(object)->targetId, Z_LVAL_P(offset), check_empty);
}

void vrzno_unset_property(zend_object *object, zend_string *member, void **cache_slot)
{
	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = UTF8ToString($1);
		delete target[property];
	})() }, vrzno_fetch_object(object)->targetId, ZSTR_VAL(member));
}

void vrzno_unset_dimension(zend_object *object, zval *offset)
{
	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = $1;
		delete target[property];
	})() }, vrzno_fetch_object(object)->targetId, Z_LVAL_P(offset));
}

HashTable *vrzno_get_properties_for(zend_object *object, zend_prop_purpose purpose)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	jstarget *targetId = vrzno->targetId;

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

		const str = String(json);
		const len = 1 + lengthBytesUTF8(str);
		const loc = _malloc(len);

		stringToUTF8(str, loc, len);

		return loc;

	}, targetId);

	php_json_parser parser;
	zval js_object;

	php_json_parser_init(&parser, &js_object, js_ret, strlen(js_ret), PHP_JSON_OBJECT_AS_ARRAY, PHP_JSON_PARSER_DEFAULT_DEPTH);
	int parseFailed = php_json_yyparse(&parser);

	if(parseFailed)
	{
		// FAIL
	}

	free(js_ret);

	return Z_ARR(js_object);
}

int vrzno_has_property(zend_object *object, zend_string *member, int has_set_exists, void **cache_slot)
{
	return EM_ASM_INT({
		const target = Module.targets.get($0);
		const property = UTF8ToString($1);
		return  property in target;
	}, vrzno_fetch_object(object)->targetId, ZSTR_VAL(member));
}

zend_string *vrzno_get_class_name(const zend_object *object)
{
	char *className = EM_ASM_INT({

		const target = Module.targets.get($0);
		const str = (target.constructor && target.constructor.name) || 'Object';
		const loc = 1 + lengthBytesUTF8(name);
		const len = _malloc(name);

		stringToUTF8(str, loc, len);

		return namePtr;

	}, vrzno_fetch_object((zend_object*) object)->targetId);

	zend_string *retVal = zend_string_init(className, strlen(className), 0);

	free(*className);

	return retVal;
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

	EM_ASM({
		const target = Module.targets.get($0);
		const property_name = UTF8ToString($1);
		const rv = $2;
		return Module.jsToZval(target[property_name], rv);
	}, vrzno->targetId, js_property_name, return_value);
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

	zval *args[argc];

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

	EM_ASM({
		const target = Module.targets.get($0);
		const method_name = UTF8ToString($1);
		const argp = $2;
		const argc = $3;
		const size = $4;
		const rv   = $5;

		const args = [];

		for(let i = 0; i < argc; i++)
		{
			const loc = argp + i * size;
			const ptr = Module.getValue(loc, '*');
			const arg = Module.zvalToJS(ptr);
			args.push(arg);
		}

		Module.jsToZval(target[method_name](...args), rv);

	}, vrzno->targetId, method_name, args, argc, size, return_value);
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

	EM_ASM({
		const target = Module.targets.get($0);
		const argv = $1;
		const argc = $2;
		const size = $3;
		const rv = $4;

		const args = [];

		for(let i = 0; i < argc; i++)
		{
			args.push(Module.zvalToJS(argv + i * size));
		}

		return Module.jsToZval(target(...args), rv);

	}, vrzno->targetId, argv, argc, sizeof(zval), return_value);
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
		Module.tacked.add(_object);

		return index;

	}, Z_OBJCE_P(object), argv, argc, sizeof(zval));
}

PHP_METHOD(Vrzno, __destruct)
{
	zval *object = getThis();
	zend_object *zObject = Z_OBJ_P(object);
	vrzno_object *vrzno = vrzno_fetch_object(zObject);

	EM_ASM({
		const target = Module.targets.get($0);
		Module.tacked.delete(target);
		Module.targets.remove(target);
	}, vrzno->targetId);
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
		const target = Module.targets.get($0);
		const str = String(target);
		const len = 1 + lengthBytesUTF8(str);
		const loc = _malloc(len);

		stringToUTF8(str, loc, len);

		return loc;
	}, vrzno->targetId);

	zend_string *zstr = zend_string_init(str, strlen(str), 0);
	RETVAL_STR_COPY(zstr);
	free(str);
}
