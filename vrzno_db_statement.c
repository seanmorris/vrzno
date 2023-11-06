static int pdo_vrzno_stmt_dtor(pdo_stmt_t *stmt)
{
	pdo_vrzno_stmt *vStmt = (pdo_vrzno_stmt*)stmt->driver_data;

	efree(vStmt);

	return 1;
}

EM_ASYNC_JS(zval*, pdo_vrzno_real_stmt_execute, (long targetId), {

	let statement = Module.targets.get(targetId);

	if(!Module.PdoParams.has(statement))
	{
		Module.PdoParams.set(statement, []);
	}

	const paramList = Module.PdoParams.get(statement);

	const bound = paramList.length
		? statement.bind(...paramList)
		: statement;

	const result = await bound.run();

	Module.PdoParams.delete(statement);

	if(!result.success)
	{
		return false;
	}

	return Module.jsToZval(result.results);

});

static int pdo_vrzno_stmt_execute(pdo_stmt_t *stmt)
{
	pdo_vrzno_stmt *vStmt = (pdo_vrzno_stmt*)stmt->driver_data;

	stmt->column_count = 0;
	vStmt->row_count = 0;
	// stmt->executed = 1;
	vStmt->curr = 0;
	vStmt->done = 0;

	zval *results = pdo_vrzno_real_stmt_execute(vStmt->stmt->targetId);

	if(results)
	{
		vStmt->row_count = EM_ASM_INT({

			const results = Module.targets.get($0);

			if(results)
			{

				return results.length;
			}

			return 0;

		}, vrzno_fetch_object(Z_OBJ_P(results))->targetId);

		if(vStmt->row_count)
		{
			stmt->column_count = EM_ASM_INT({

				const results = Module.targets.get($0);

				if(results.length)
				{

					return Object.keys(results[0]).length;
				}

				return 0;

			}, vrzno_fetch_object(Z_OBJ_P(results))->targetId);
		}

		vStmt->results = results;
		return true;
	}

	return false;
}

static int pdo_vrzno_stmt_fetch(pdo_stmt_t *stmt, enum pdo_fetch_orientation ori, zend_long offset)
{
	pdo_vrzno_stmt *vStmt = (pdo_vrzno_stmt*)stmt->driver_data;

	if(!vStmt->results)
	{
		return false;
	}

	zval *row = EM_ASM_PTR({
		const targetId = $0;
		const target = Module.targets.get(targetId);
		const current = $1;

		if(current >= target.length)
		{
			return null;
		}

		return Module.jsToZval(target[current]);

	}, vrzno_fetch_object(Z_OBJ_P(vStmt->results))->targetId, vStmt->curr);

	if(row)
	{
		vStmt->curr++;
	}
	else
	{
		vStmt->done = 1;
	}

	return row;
}

static int pdo_vrzno_stmt_describe_col(pdo_stmt_t *stmt, int colno)
{
	pdo_vrzno_stmt *vStmt = (pdo_vrzno_stmt*)stmt->driver_data;

	if(colno >= stmt->column_count) {
		return 0;
	}

	char *colName = EM_ASM_PTR({

		const results = Module.targets.get($0);

		if(results.length)
		{
			const jsRet = Object.keys(results[0])[$1];

			const len    = lengthBytesUTF8(jsRet) + 1;
			const strLoc = _malloc(len);

			stringToUTF8(jsRet, strLoc, len);

			return strLoc;
		}

		return 0;

	}, vrzno_fetch_object(Z_OBJ_P(vStmt->results))->targetId, colno);


	if(!colName)
	{
		return 0;
	}

	stmt->columns[colno].name = zend_string_init(colName, strlen(colName), 0);
	stmt->columns[colno].maxlen = SIZE_MAX;
	stmt->columns[colno].precision = 0;

	free(colName);

	return 1;
}

static int pdo_vrzno_stmt_get_col(pdo_stmt_t *stmt, int colno, zval *zv, enum pdo_param_type *type)
{
	pdo_vrzno_stmt *vStmt = (pdo_vrzno_stmt*)stmt->driver_data;

	if (!vStmt->stmt)
	{
		return 0;
	}

	if(colno >= stmt->column_count)
	{
		return 0;
	}

	zval *jeRet = EM_ASM_PTR({

		const results = Module.targets.get($0);
		const current = -1 + $1;
		const colno   = $2;

		if(current >= results.length)
		{
			return null;
		}

		const result = results[current];

		const key = Object.keys(result)[$2];

		const zval = Module.jsToZval(result[key]);

		return zval;

	}, vrzno_fetch_object(Z_OBJ_P(vStmt->results))->targetId, vStmt->curr, colno);

	if(!zv)
	{
		return 0;
	}

	ZVAL_COPY(zv, jeRet);

	return 1;
}

static int pdo_vrzno_stmt_param_hook(pdo_stmt_t *stmt, struct pdo_bound_param_data *param, enum pdo_param_event event_type)
{
	if(event_type != PDO_PARAM_EVT_ALLOC)
	{
		return true;
	}

	pdo_vrzno_stmt *vStmt = (pdo_vrzno_stmt*)stmt->driver_data;

	EM_ASM({
		const statement = Module.targets.get($0);
		const paramVal  = Module.zvalToJS($1);

		if(!Module.PdoParams.has(statement))
		{
			Module.PdoParams.set(statement, []);
		}

		const paramList = Module.PdoParams.get(statement);

		paramList.push(paramVal);

	}, vStmt->stmt->targetId, param->parameter);

	return true;
}

static int pdo_vrzno_stmt_get_attribute(pdo_stmt_t *stmt, zend_long attr, zval *val)
{
	EM_ASM({ console.log('GET ATTR', $0, $1, $2); }, stmt, attr, val);
	return 1;
}

static int pdo_vrzno_stmt_col_meta(pdo_stmt_t *stmt, zend_long colno, zval *return_value)
{
	EM_ASM({ console.log('COL META', $0, $1, $2); }, stmt, colno, return_value);
	return 1;
}

static int pdo_vrzno_stmt_cursor_closer(pdo_stmt_t *stmt)
{
	EM_ASM({ console.log('CLOSE', $0, $1, $2); }, stmt);
	return 1;
}

const struct pdo_stmt_methods vrzno_stmt_methods = {
	pdo_vrzno_stmt_dtor,
	pdo_vrzno_stmt_execute,
	pdo_vrzno_stmt_fetch,
	pdo_vrzno_stmt_describe_col,
	pdo_vrzno_stmt_get_col,
	pdo_vrzno_stmt_param_hook,
	NULL, /* set_attr */
	pdo_vrzno_stmt_get_attribute, /* get_attr */
	pdo_vrzno_stmt_col_meta,
	NULL, /* next_rowset */
	pdo_vrzno_stmt_cursor_closer
};
