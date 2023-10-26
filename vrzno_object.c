zend_class_entry *vrzno_class_entry;
zend_object_handlers vrzno_object_handlers;

typedef struct {
	bool isFunction;
	long targetId;
	zend_object zo;
} vrzno_object;

static inline vrzno_object *vrzno_fetch_object(zend_object *obj) {
	return (vrzno_object*)((char*)(obj) - XtOffsetOf(vrzno_object, zo));
}

static struct _zend_object *vrzno_create_object(zend_class_entry *class_type)
{
    vrzno_object *vrzno = zend_object_alloc(sizeof(vrzno_object), class_type);

    zend_object_std_init(&vrzno->zo, class_type);

    vrzno->zo.handlers = &vrzno_object_handlers;
    vrzno->targetId = (long) EM_ASM_INT({ return Module.targets.add(globalThis); });

	return &vrzno->zo;
}

static vrzno_object *vrzno_create_object_for_target(int target_id, bool isFunction)
{
    vrzno_object *vrzno = zend_object_alloc(sizeof(vrzno_object), vrzno_class_entry);

    zend_object_std_init(&vrzno->zo, vrzno_class_entry);

    vrzno->zo.handlers = &vrzno_object_handlers;
    vrzno->targetId = target_id;
	vrzno->isFunction = (bool) isFunction;

	// EM_ASM({
	// 	const target = Module.targets.get($0);
	// 	if(!target) return;
	// 	Module.targets.tack(target);
	// }, vrzno->targetId);

	return vrzno;
}

static void vrzno_object_free(zend_object *zobj)
{
	vrzno_object *vrzno = vrzno_fetch_object(zobj);

	// EM_ASM({
	// 	const target = Module.targets.get($0);
	// 	if(!target) return;
	// 	Module.targets.untack();
	// }, vrzno->targetId);

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

	// EM_ASM({ console.log('READ1', $0, $1, $2); }, targetId, object, vrzno);

	char *scalar_result = EM_ASM_INT({

		const target = Module.targets.get($0);

		const property = UTF8ToString($1);
		const result = target[property];

		// console.log('READ1', {aa: $0, target, property, result});

		if(!result || !['function','object'].includes(typeof result))
		{
			const jsRet    = 'OK' + String(result);
			const len      = lengthBytesUTF8(jsRet) + 1;
			const strLoc   = _malloc(len);

			stringToUTF8(jsRet, strLoc, len);

			return strLoc;
		}

		const jsRet    = 'XX';
		const len      = lengthBytesUTF8(jsRet) + 1;
		const strLoc   = _malloc(len);

		stringToUTF8(jsRet, strLoc, len);

		return strLoc;

	}, targetId, name, type);

	if(strlen(scalar_result) > 1 && *scalar_result == 'O')
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

		// console.log('READING', {aa: $0, target, property, result});

		if(result && ['function','object'].includes(typeof result))
		{
			let index = Module.targets.getId(result);

			if(!Module.targets.has(result))
			{
				index = Module.targets.add(result);

				result[Module.hasZval] = zvalPtr;

				if(Module.targets.hasId(index))
				{
					// Module.fRegistry.unregister(Module.targets.get(index));
				}

				// Module.fRegistry.register(result, zvalPtr, result);
			}

			// console.log({index, result});

			return index;
		}

		return 0;

	}, targetId, name, rv);

	if(!obj_result)
	{
		ZVAL_BOOL(rv, obj_result);

		return rv;
	}

	bool isFunction = (bool) EM_ASM_INT({ 'function' === typeof (Module.targets.get($0)); }, targetId);

	vrzno_object *retVrzno = vrzno_create_object_for_target(obj_result, isFunction);

	// EM_ASM({ console.log('READ2', $0, $1, $2) }, obj_result, retVrzno, targetId);

	ZVAL_OBJ(rv, &retVrzno->zo);
	return rv;
}

zval *vrzno_write_property(zend_object *object, zend_string *member, zval *newValue, void **cache_slot)
{
	char         *name     = ZSTR_VAL(member);
	vrzno_object *vrzno    = vrzno_fetch_object(object);
	long          targetId = vrzno->targetId;

	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = UTF8ToString($1);
		const newValue = $2;
		// console.log('WRITING', {aa: $0, target, property, newValue});
	})() }, targetId, name, newValue);

	// php_debug_zval_dump(newValue, 5);

	char *errstr = NULL;
	zend_fcall_info_cache fcc;

	bool isCallable = zend_is_callable_ex(newValue, NULL, 0, NULL, &fcc, &errstr);

	if(Z_TYPE_P(newValue) == IS_OBJECT && Z_OBJCE_P(newValue) == vrzno_class_entry)
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

			// console.log('WRITING FUNCTION', {aa: $0, target, property, funcPtr, zvalPtr, paramCount});

			target[property] = Module.callableToJs(funcPtr, null);

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

static zval *vrzno_read_dimension(zend_object *object, zval *offset, int type, zval *rv)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	      long targetId = vrzno->targetId;
	         long index = Z_LVAL_P(offset);

	// EM_ASM({ console.log('READD1', $0) }, targetId);

	char *scalar_result = EM_ASM_INT({

		const target = Module.targets.get($0);

		const property = $1;
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

	free(scalar_result);

	int obj_result = EM_ASM_INT({

		const target   = Module.targets.get($0);
		const property = UTF8ToString($1);
		const result   = target[property];
		const zvalPtr  = $2;

		// console.log('READING', {aa: $0, target, property, result});

		if(result && ['function','object'].includes(typeof result))
		{
			let index = Module.targets.getId(result);

			if(!Module.targets.has(result))
			{
				index = Module.targets.add(result);

				result[Module.hasZval] = zvalPtr;

				if(Module.targets.hasId(index))
				{
					// Module.fRegistry.unregister(Module.targets.get(index));
				}

				// Module.fRegistry.register(result, zvalPtr, result);
			}

			// console.log({index, result});

			return index;
		}

		return 0;

	}, targetId, index, rv);

	if(!obj_result)
	{
		ZVAL_BOOL(rv, obj_result);

		return rv;
	}

	bool isFunction = (bool) EM_ASM_INT({ 'function' === typeof (Module.targets.get($0)); }, targetId);

	vrzno_object *retVrzno = vrzno_create_object_for_target(obj_result, isFunction);

	// EM_ASM({ console.log('READD2', $0, $1, $2) }, obj_result, retVrzno, targetId);

	ZVAL_OBJ(rv, &retVrzno->zo);
	return rv;
}

static void vrzno_write_dimension(zend_object *object, zval *offset, zval *newValue)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	      long targetId = vrzno->targetId;
	         long index = Z_LVAL_P(offset);

	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = $1;
		const newValue = $2;
		// console.log('WRITING', {aa: $0, target, property, newValue});
	})() }, targetId, index, newValue);

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
		// EM_ASM({ console.log('INC_ZVAL_1a', $0, $1); }, newValue, Z_REFCOUNTED_P(newValue));

		EM_ASM({ (() =>{
			const target   = Module.targets.get($0);
			const property = $1;
			const funcPtr  = $2;
			const zvalPtr  = $3;

			// console.log('WRITING FUNCTION', {aa: $0, target, property, funcPtr});

			target[property] = Module.callableToJs(funcPtr);

			// Module.fRegistry.register(target[property], zvalPtr, target[property]);

		})() }, targetId, index, fcc.function_handler, newValue);

		return;
	}

	// php_debug_zval_dump(newValue, 5);

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

static int vrzno_has_dimension(zend_object *object, zval *offset, int check_empty)
{
	         long index = Z_LVAL_P(offset);
	vrzno_object *vrzno = vrzno_fetch_object(object);
	      long targetId = vrzno->targetId;

	// EM_ASM({ console.log('HASD', $0) }, targetId);

	return EM_ASM_INT({

		const target      = Module.targets.get($0);
		const property    = $1;
		const check_empty = $2;

		if(!check_empty)
		{
			return property in target
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

	// bool isScalar = false;

	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = UTF8ToString($1);
		// console.log('DELETING', {target, property});
		delete target[property];
	})() }, targetId, name);
}

void vrzno_unset_dimension(zend_object *object, zval *offset)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
          long targetId = vrzno->targetId;
	         long index = Z_LVAL_P(offset);

	// bool isScalar = false;

	EM_ASM({ (() =>{
		const target = Module.targets.get($0);
		const property = $1;
		// console.log('DELETING', {target, property});
		delete target[property];
	})() }, targetId, index);
}

static HashTable *vrzno_get_properties_for(zend_object *object, zend_prop_purpose purpose)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	long targetId = vrzno->targetId;

	// EM_ASM({ console.log('SCAN1', $0, $1, $2); }, targetId, object, vrzno);

	char *js_ret = EM_ASM_INT({

		const target = Module.targets.get($0);

		if(typeof target === 'function')
		{
			json = JSON.stringify({__targetId:$0});
		}
		else
		{
			try{ json = JSON.stringify({__targetId:$0, ...target}); }
			catch { json = JSON.stringify({__targetId:$0}); }
		}

		// console.log('SCANNING', {aa: $0, target});

		const jsRet  = String(json);
		const len    = lengthBytesUTF8(jsRet) + 1;
		const strLoc = _malloc(len);

		// console.log(jsRet);

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
	        char *name     = ZSTR_VAL(member);
	         long targetId = vrzno->targetId;

	return EM_ASM_INT({
		const target   = Module.targets.get($0);
		const property = UTF8ToString($1);
		// console.log('CHECKING', {target, property, result});
		return  property in target;
	}, targetId, name);
}

static zend_string *vrzno_get_class_name(zend_object *object)
{
	vrzno_object *vrzno = vrzno_fetch_object(object);
	         long targetId = vrzno->targetId;

	char *className = EM_ASM_INT({

		const target = Module.targets.get($0);
		const name   = (target.constructor && target.constructor.name) || 'Object';
		// console.log('CLASSNAME', {target, id:$0, name});

		const len     = lengthBytesUTF8(name) + 1;
		const namePtr = _malloc(name);

		stringToUTF8(name, namePtr, len);

		return namePtr;

	}, targetId);

	zend_string *retVal = zend_string_init(className, strlen(className), 0);

	free(*className);

	return retVal;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo___invoke, 0, 0, -1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___call, 0, 0, 2)
	ZEND_ARG_INFO(0, method_name)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo___get, 0, 0, 1)
	ZEND_ARG_INFO(0, property_name)
ZEND_END_ARG_INFO()

static const zend_function_entry vrzno_vrzno_methods[] = {
	// PHP_ME(Vrzno, __construct, arginfo___call,   ZEND_ACC_PUBLIC)
	// PHP_ME(Vrzno, __destruct,  arginfo___call,   ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __invoke,    arginfo___invoke, ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __call,      arginfo___call,   ZEND_ACC_PUBLIC)
	PHP_ME(Vrzno, __get,       arginfo___get,    ZEND_ACC_PUBLIC)
	PHP_FE_END
};

PHP_METHOD(Vrzno, __call)
{
	zval         *object   = getThis();
	zend_object  *zObject  = object->value.obj;
	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
	long          targetId = vrzno->targetId;

	// EM_ASM({ console.log('CALL1', $0, $1, $2) }, targetId, vrzno, zObject);

	char   *js_method_name = "";
	size_t  js_method_name_len = 0;

	HashTable *js_argv;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(js_method_name, js_method_name_len)
		Z_PARAM_ARRAY_HT(js_argv)
	ZEND_PARSE_PARAMETERS_END();

	int js_argc = zend_hash_num_elements(js_argv);
	int size = sizeof(zval);

	zval *zvalPtrs = NULL;

	if(js_argc)
	{
		zvalPtrs = (zval*) emalloc(js_argc * sizeof(zval));
		zval *data;
		int i = 0;

		ZEND_HASH_PACKED_FOREACH_VAL(js_argv, data) {
			ZVAL_NULL(&zvalPtrs[i]);
			ZVAL_COPY_VALUE(&zvalPtrs[i], data);
			i++;
		} ZEND_HASH_FOREACH_END();
	}

	zval *js_ret = EM_ASM_INT({

		const target      = Module.targets.get($0);
		const method_name = UTF8ToString($1);
		const argv = $2;
		const argc = $3;
		const size = $4;

		const args = [];

		// console.log({argv, argc, size});

		for(let i = 0; i < argc; i++)
		{
			args.push(Module.zvalToJS(argv + i * size));
		}

		// console.log({args});

		const jsRet = target[method_name](...args);

		// console.log({jsRet});

		const retZval = Module.jsToZval(jsRet);

		// console.log({retZval});

		return retZval;

	}, targetId, js_method_name, zvalPtrs, js_argc, size);

	if(js_argc)
	{
		// EM_ASM({ console.log('CALL - FREE', $0) }, zvalPtrs);
		// efree(zvalPtrs);
	}

	if(!js_ret)
	{
		return;
	}

	RETURN_ZVAL(js_ret, 0, 0);
}

PHP_METHOD(Vrzno, __invoke)
{
	zval         *object   = getThis();
	zend_object  *zObject  = object->value.obj;
	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
	long          targetId = vrzno->targetId;

	// EM_ASM({ console.log('ICALL1', $0, $1, $2) }, targetId, zObject, vrzno);

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

	// EM_ASM({ console.log('ICALL2', $0, $1) }, js_argc, i);

	zval *js_ret = EM_ASM_INT({

		const target = Module.targets.get($0);
		const argv   = $1;
		const argc   = $2;
		const size   = $3;

		const args   = [];

		// console.log({argv, argc, size});

		for(let i = 0; i < argc; i++)
		{
			args.push(Module.zvalToJS(argv + i * size));
		}

		// console.log('ICALLING', {aa: $0, target, args});

		const jsRet = target(...args);
		return Module.jsToZval(jsRet);

	}, targetId, zvalPtrs, js_argc, size);

	if(js_argc)
	{
		// efree(zvalPtrs);
	}

	ZVAL_UNDEF(return_value);

	if(!js_ret)
	{
		return;
	}

	ZVAL_COPY(return_value, js_ret);
}

PHP_METHOD(Vrzno, __get)
{
	zval         *object   = getThis();
	zend_object  *zObject  = object->value.obj;
	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
	long          targetId = vrzno->targetId;

	// EM_ASM({ console.log('GET1', $0) }, targetId);

	char   *js_property_name = "";
	size_t  js_property_name_len = 0;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(js_property_name, js_property_name_len)
	ZEND_PARSE_PARAMETERS_END();

	zval *js_ret = EM_ASM_INT({

		const target        = Module.targets.get($0);
		const property_name = UTF8ToString($1);

		// console.log('GET2', $0, {target, property_name});

		target[property_name] = newValueJson;

		const jsRet = target[property_name];
		return Module.jsToZval(jsRet);

	}, targetId, js_property_name);

	ZVAL_UNDEF(return_value);
	ZVAL_COPY(return_value, js_ret);
}


// PHP_METHOD(Vrzno, __construct)
// {
// 	zval         *object   = getThis();
// 	zend_object  *zObject  = object->value.obj;
// 	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
// 	long          targetId = vrzno->targetId;
// }

// PHP_METHOD(Vrzno, __destruct)
// {
// 	zval         *object   = getThis();
// 	zend_object  *zObject  = object->value.obj;
// 	vrzno_object *vrzno    = vrzno_fetch_object(zObject);
// 	long          targetId = vrzno->targetId;
// }
