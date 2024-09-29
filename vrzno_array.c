typedef struct vrzno_array_iterator {
	zend_object_iterator it;
	zend_long key;
	zval value;
} vrzno_array_iterator;

static void vrzno_array_it_dtor(zend_object_iterator *iter)
{
	zval_ptr_dtor(&iter->data);
}

static int vrzno_array_it_valid(zend_object_iterator *it)
{
	vrzno_array_iterator *iter = (vrzno_array_iterator*)it;

	zend_object  *object = Z_OBJ(it->data);
	vrzno_object *vrzno  = vrzno_fetch_object(object);

	long valid = (long) EM_ASM_INT({
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

		if(Array.isArray(target) || ArrayBuffer.isView(target))
		{
			if(property >=0 && property < target.length)
			{
				return 1;
			}
		}

		return 0;

	}, vrzno->targetId, iter->key);

	if(valid)
	{
		return SUCCESS;
	}

	return FAILURE;
}

static zval *vrzno_array_it_get_current_data(zend_object_iterator *it)
{
	vrzno_array_iterator *iter = (vrzno_array_iterator*)it;

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

		return Module.jsToZval(target[property], rv);

	}, vrzno_fetch_object(Z_OBJ(it->data))->targetId, iter->key, &iter->value);

	return &iter->value;
}

static void vrzno_array_it_get_current_key(zend_object_iterator *it, zval *key)
{
	vrzno_array_iterator *iter = (vrzno_array_iterator*)it;
	ZVAL_LONG(key, iter->key);
}

static void vrzno_array_it_move_forward(zend_object_iterator *it)
{
	vrzno_array_iterator *iter = (vrzno_array_iterator*)it;
	iter->key++;
}

static void vrzno_array_it_rewind(zend_object_iterator *it)
{
	vrzno_array_iterator *iter = (vrzno_array_iterator*)it;
	iter->key = 0;
}

static void vrzno_array_invalidate_current(zend_object_iterator *it)
{
	vrzno_array_iterator *iter = (vrzno_array_iterator *)it;

	if (Z_TYPE(iter->value) != IS_UNDEF) {
		zval_ptr_dtor(&iter->value);
		ZVAL_UNDEF(&iter->value);
	}
}

// static HashTable *vrzno_array_it_get_gc(zend_object_iterator *iter, zval **table, int *n)
// {}

static const zend_object_iterator_funcs vrzno_array_it_funcs = {
	vrzno_array_it_dtor,
	vrzno_array_it_valid,
	vrzno_array_it_get_current_data,
	vrzno_array_it_get_current_key,
	vrzno_array_it_move_forward,
	vrzno_array_it_rewind,
	vrzno_array_invalidate_current,
	NULL, // vrzno_array_it_get_gc,
};

static zend_object_iterator *vrzno_array_get_iterator(zend_class_entry *ce, zval *zv, int by_ref)
{
	vrzno_array_iterator *iterator = emalloc(sizeof(vrzno_array_iterator)); // @todo: Figure if we need to clear this.
	zend_iterator_init(&iterator->it);

	ZVAL_OBJ_COPY(&iterator->it.data, Z_OBJ_P(zv));
	iterator->it.funcs = &vrzno_array_it_funcs;
	// iterator->by_ref = by_ref;

	return &iterator->it;
}
