static void vrzno_handle_closer(pdo_dbh_t *dbh)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	EM_ASM({

		console.log('CLOSE', $0);

	}, handle->targetId);
}

static bool vrzno_handle_preparer(pdo_dbh_t *dbh, zend_string *sql, pdo_stmt_t *stmt, zval *driver_options)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;
	pdo_vrzno_stmt *vStmt = emalloc(sizeof(pdo_vrzno_stmt));

	stmt->methods = &vrzno_stmt_methods;
	stmt->driver_data = vStmt;

	const char *sqlString = ZSTR_VAL(sql);

	zval *prepared = EM_ASM_PTR({

		const target = Module.targets.get($0);
		const query  = UTF8ToString($1);

		if(Module.pdoDriver && Module.pdoDriver.prepare)
		{
			const prepared = Module.pdoDriver.prepare(target, query);
			const zval = Module.jsToZval(prepared);

			return zval;
		}

		return null;

	}, handle->targetId, sqlString);

	if(prepared)
	{
		vStmt->db   = handle;
		vStmt->stmt = vrzno_fetch_object(Z_OBJ_P(prepared));

		return true;
	}

	return false;
}

static zend_long vrzno_handle_doer(pdo_dbh_t *dbh, const zend_string *sql)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;
	const char *sqlString = ZSTR_VAL(sql);

	return (zend_long) EM_ASM_INT({

		console.log('DO', $0, UTF8ToString($1));

		const target = Module.targets.get($0);
		const query  = UTF8ToString($1);

		if(Module.pdoDriver && Module.pdoDriver.doer)
		{
			return Module.pdoDriver.doer(target, query);
		}

		return 1;

	}, handle->targetId, &sqlString);
}

static zend_string *vrzno_handle_quoter(pdo_dbh_t *dbh, const zend_string *unquoted, enum pdo_param_type paramtype)
{
	// pdo_vrzno_db_handle *handle = dbh->driver_data;
	const char *unquotedChar = ZSTR_VAL(unquoted);
	zend_string *quoted = zend_string_init(unquotedChar, strlen(unquotedChar), 0);
	return quoted;
}

static bool vrzno_handle_begin(pdo_dbh_t *dbh)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	return (bool) EM_ASM_INT({

		console.log('BEGIN TXN', $0);

		return true;

	}, handle->targetId);
}

static bool vrzno_handle_commit(pdo_dbh_t *dbh)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	return (bool) EM_ASM_INT({

		console.log('COMMIT TXN', $0);

		return true;

	}, handle->targetId);
}

static bool vrzno_handle_rollback(pdo_dbh_t *dbh)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	return (bool) EM_ASM_INT({

		console.log('ROLLBACK TXN', $0);

		return true;

	}, handle->targetId);
}

static bool pdo_vrzno_set_attr(pdo_dbh_t *dbh, zend_long attr, zval *val)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	return (bool) EM_ASM_INT({

		console.log('SET ATTR', $0, $1, $2);

		return true;

	}, handle->targetId, attr, val);
}

static zend_string *pdo_vrzno_last_insert_id(pdo_dbh_t *dbh, const zend_string *name)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;
	const char *nameStr = ZSTR_VAL(name);

	return zend_ulong_to_str(EM_ASM_INT({

		console.log('LAST INSERT ID', $0, UTF8ToString($1));

		return 0;

	}, handle->targetId, &nameStr));
}

static void pdo_vrzno_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	EM_ASM({

		console.log('FETCH ERROR FUNC', $0, $1, $2);

	}, handle->targetId, stmt, info);
}

static int pdo_vrzno_get_attr(pdo_dbh_t *dbh, zend_long attr, zval *return_value)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	return EM_ASM_INT({

		console.log('GET ATTR', $0, $1, $2);

		return 0;

	}, handle->targetId, attr, return_value);
}

static void pdo_vrzno_request_shutdown(pdo_dbh_t *dbh)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	EM_ASM({

		console.log('SHUTDOWN', $0);

	}, handle->targetId);
}

static void pdo_vrzno_get_gc(pdo_dbh_t *dbh, zend_get_gc_buffer *gc_buffer)
{
	pdo_vrzno_db_handle *handle = dbh->driver_data;

	EM_ASM({

		console.log('GET GC', $0, $1);

	}, handle->targetId, gc_buffer);
}

static const struct pdo_dbh_methods vrzno_db_methods = {
	vrzno_handle_closer,
	vrzno_handle_preparer,
	vrzno_handle_doer,
	vrzno_handle_quoter,
	vrzno_handle_begin,
	vrzno_handle_commit,
	vrzno_handle_rollback,
	pdo_vrzno_set_attr,
	pdo_vrzno_last_insert_id,
	pdo_vrzno_fetch_error_func,
	pdo_vrzno_get_attr,
	NULL,	/* check_liveness: not needed */
	NULL, //get_driver_methods,
	pdo_vrzno_request_shutdown,
	NULL, /* in transaction, use PDO's internal tracking mechanism */
	pdo_vrzno_get_gc
};

static int pdo_vrzno_db_handle_factory(pdo_dbh_t *dbh, zval *driver_options)
{
	pdo_vrzno_db_handle *handle;
	int ret = 0;

	handle = pecalloc(1, sizeof(pdo_vrzno_db_handle), dbh->is_persistent);

	handle->einfo.errcode = 0;
	handle->einfo.errmsg = NULL;
	dbh->driver_data = handle;

	// dbh->skip_param_evt = 0x7F ^ (1 << PDO_PARAM_EVT_EXEC_PRE);

	handle->targetId = strtol(dbh->data_source, NULL, NULL);

	dbh->methods = &vrzno_db_methods;

	ret = 1;

	return ret;
}

const pdo_driver_t pdo_vrzno_driver = {
	PDO_DRIVER_HEADER(vrzno),
	pdo_vrzno_db_handle_factory
};
