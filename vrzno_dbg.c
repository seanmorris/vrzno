#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2

#include "zend_builtin_functions.h"

__attribute__((weak)) int phpdbg_arm_auto_global(zval *ptrzv);

char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_symbols(bool show_globals)
{
	if(!EG(current_execute_data) || !EG(current_execute_data)->func)
	{
		fprintf(stderr, "No active op array: %s:%d\n", __FILE__, __LINE__);
		return NULL;
	}

	zend_array *symtable;

	if(show_globals)
	{
		if(phpdbg_arm_auto_global)
		{
			zend_hash_apply(CG(auto_globals), (apply_func_t) phpdbg_arm_auto_global);
		}
		symtable = &EG(symbol_table);
	}
	else if(!(symtable = zend_rebuild_symbol_table()))
	{
		fprintf(stderr, "No active symbol table: %s:%d\n", __FILE__, __LINE__);
		return NULL;
	}

	zend_string *varName;
	zval *zv;
	size_t count = 0;
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
		ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(symtable, varName, zv)
		{
			if(!varName) continue;

			if(show_globals ? zend_is_auto_global(varName) : !zend_is_auto_global(varName))
			{
				count++;
				len += sizeof(void*)    // zval pointer
					+ sizeof(size_t)    // name length
					+ ZSTR_LEN(varName) // name
					+ 1;                // NUL
			}
		} ZEND_HASH_FOREACH_END();

		output = malloc(sizeof(len) + len);
		char *cur = output;

		void *p = len;
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;

		ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(symtable, varName, zv)
		{
			if(!varName) continue;

			if(show_globals ? zend_is_auto_global(varName) : !zend_is_auto_global(varName))
			{
				p = zv;
				memcpy(cur, &p, sizeof p);
				cur += sizeof p;

				p = ZSTR_LEN(varName);
				memcpy(cur, &p, sizeof p);
				cur += sizeof p;

				strcpy(cur, ZSTR_VAL(varName));
				cur += ZSTR_LEN(varName) + 1;
			}
		} ZEND_HASH_FOREACH_END();
	}
	zend_catch
	{
		fprintf(stderr, "Cannot fetch all data from the symbol table, invalid data source: %s:%d\n", __FILE__, __LINE__);
	}
	zend_end_try();

	return output;
}

char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_constants(void)
{
	zend_constant *zc;
	size_t count = 0;
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
		ZEND_HASH_MAP_FOREACH_PTR(EG(zend_constants), zc) {
			if(ZEND_CONSTANT_MODULE_NUMBER(zc) == PHP_USER_CONSTANT)
			{
				len += sizeof(void*)     // zval pointer
					+ sizeof(size_t)     // name length
					+ ZSTR_LEN(zc->name) // name
					+ 1;                 // NUL
				count++;
			}
		} ZEND_HASH_FOREACH_END();

		output = malloc(sizeof(len) + len);
		char *cur = output;

		void *p = len;
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;

		ZEND_HASH_MAP_FOREACH_PTR(EG(zend_constants), zc) {
			if(ZEND_CONSTANT_MODULE_NUMBER(zc) == PHP_USER_CONSTANT)
			{
				if(!zc->name) continue;

				p = &zc->value;
				memcpy(cur, &p, sizeof p);
				cur += sizeof p;

				p = ZSTR_LEN(zc->name);
				memcpy(cur, &p, sizeof p);
				cur += sizeof p;

				strcpy(cur, ZSTR_VAL(zc->name));
				cur += ZSTR_LEN(zc->name) + 1;
			}
		} ZEND_HASH_FOREACH_END();
	}
	zend_catch
	{
		fprintf(stderr, "Cannot fetch all the constants, invalid data source: %s:%d\n", __FILE__, __LINE__);
	} zend_end_try();

	return output;
}

char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_classes(void)
{
	zend_class_entry *ce;
	size_t count = 0;
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
		ZEND_HASH_MAP_FOREACH_PTR(EG(class_table), ce)
		{
			if (ce->type != ZEND_USER_CLASS) continue;

			count++;
			len += sizeof(void*)
				+ sizeof(size_t)
				+ (ce->info.user.filename ? ZSTR_LEN(ce->info.user.filename) : 0) + 1
				+ sizeof(size_t)
				+ ZSTR_LEN(ce->name)
				+ 1;

		} ZEND_HASH_FOREACH_END();

		output = malloc(sizeof(len) + len);
		char *cur = output;

		void *p = len;
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;

		ZEND_HASH_MAP_FOREACH_PTR(EG(class_table), ce)
		{
			if (ce->type != ZEND_USER_CLASS) continue;

			size_t filenameLen = (ce->info.user.filename ? ZSTR_LEN(ce->info.user.filename) : 0);
			size_t nameLen = ZSTR_LEN(ce->name);

			p = filenameLen;
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			strcpy(cur, ce->info.user.filename ? ZSTR_VAL(ce->info.user.filename) : "");
			cur += filenameLen + 1;

			p = ce->info.user.line_start;
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			p = nameLen;
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			strcpy(cur, ZSTR_VAL(ce->name));
			cur += nameLen + 1;

		} ZEND_HASH_FOREACH_END();
	}
	zend_catch
	{
		fprintf(stderr, "Not all classes could be fetched, possibly invalid data source: %s:%d\n", __FILE__, __LINE__);
	} zend_end_try();

	return output;
}

char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_functions(void)
{
	zend_function *zf;
	size_t count = 0;
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
		ZEND_HASH_MAP_FOREACH_PTR(EG(function_table), zf)
		{
			if (zf->type != ZEND_USER_FUNCTION) continue;

			zend_op_array *op_array = &zf->op_array;

			count++;
			len += sizeof(size_t)
				+ (op_array->filename ? ZSTR_LEN(op_array->filename) : 0) + 1
				+ sizeof(size_t)
				+ ZSTR_LEN(op_array->function_name) + 1
				+ sizeof(void*);
		} ZEND_HASH_FOREACH_END();

		output = malloc(sizeof(len) + len);
		char *cur = output;

		void *p = len;
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;

		ZEND_HASH_MAP_FOREACH_PTR(EG(function_table), zf)
		{
			if (zf->type != ZEND_USER_FUNCTION) continue;

			zend_op_array *op_array = &zf->op_array;

			size_t filenameLen = (op_array->filename ? ZSTR_LEN(op_array->filename) : 0);
			size_t nameLen = ZSTR_LEN(op_array->function_name);

			p = filenameLen;
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			strcpy(cur, op_array->filename ? ZSTR_VAL(op_array->filename) : "");
			cur += filenameLen + 1;

			p = op_array->line_start;
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			p = nameLen;
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			strcpy(cur, ZSTR_VAL(op_array->function_name));
			cur += nameLen + 1;

		} ZEND_HASH_FOREACH_END();
	}
	zend_catch
	{
		fprintf(stderr, "Not all functions could be fetched, possibly invalid data source: %s:%d\n", __FILE__, __LINE__);
	} zend_end_try();

	return output;
}

char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_files(void)
{
	zend_string *filename;
	size_t count = 0;
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
		ZEND_HASH_MAP_FOREACH_STR_KEY(&EG(included_files), filename) {
			count++;
			len += sizeof(size_t) + ZSTR_LEN(filename) + 1;
		} ZEND_HASH_FOREACH_END();

		output = malloc(sizeof(len) + len);
		char *cur = output;

		void *p = len;
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;

		ZEND_HASH_MAP_FOREACH_STR_KEY(&EG(included_files), filename) {
			p = ZSTR_LEN(filename);
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			strcpy(cur, ZSTR_VAL(filename));
			cur += ZSTR_LEN(filename) + 1;
		} ZEND_HASH_FOREACH_END();
	}
	zend_catch
	{
		fprintf(stderr, "Could not fetch included file count, invalid data source: %s:%d\n", __FILE__, __LINE__);
	} zend_end_try();

	return output;
}

char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_backtrace(void)
{
	HashPosition position;
	zval zbacktrace;

	zval *tmp;
	zval startLine, startFile;

	const char *startFilename;
	zval *file = &startFile, *line = &startLine;
	int i = 0;

	size_t count = 0;
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
		zend_fetch_debug_backtrace(&zbacktrace, 0, 0, 0);
	}
	zend_catch
	{
		fprintf(stderr, "Couldn't fetch backtrace, invalid data source: %s:%d\n", __FILE__, __LINE__);
	} zend_end_try();

	startFilename = zend_get_executed_filename();

	Z_LVAL(startLine) = zend_get_executed_lineno();
	Z_STR(startFile) = zend_string_init(startFilename, strlen(startFilename), 0);

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL(zbacktrace), &position);
	tmp = zend_hash_get_current_data_ex(Z_ARRVAL(zbacktrace), &position);

	while ((tmp = zend_hash_get_current_data_ex(Z_ARRVAL(zbacktrace), &position)))
	{
		count++;
		len += sizeof(size_t)               // name length
			+ (file ? Z_STRLEN_P(file) : 0) // name
			+ 1                             // NUL
			+ sizeof(uint32_t);             // line number

		file = zend_hash_str_find(Z_ARRVAL_P(tmp), ZEND_STRL("file"));
		zend_hash_move_forward_ex(Z_ARRVAL(zbacktrace), &position);
	}

	count++;
	len += sizeof(size_t)               // name length
		+ (file ? Z_STRLEN_P(file) : 0) // name
		+ 1                             // NUL
		+ sizeof(uint32_t);             // line number

	output = malloc(sizeof(len) + len);
	char *cur = output;

	void *p = len;
	memcpy(cur, &p, sizeof p);
	cur += sizeof p;

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL(zbacktrace), &position);
	tmp = zend_hash_get_current_data_ex(Z_ARRVAL(zbacktrace), &position);

	file = &startFile;
	line = &startLine;

	while ((tmp = zend_hash_get_current_data_ex(Z_ARRVAL(zbacktrace), &position)))
	{
		if(file)
		{
			p = Z_STRLEN_P(file);
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;

			strcpy(cur, Z_STRVAL_P(file));
			cur += Z_STRLEN_P(file) + 1;
			i++;

			p = (uint32_t)Z_LVAL_P(line);
			memcpy(cur, &p, sizeof p);
			cur += sizeof p;
		}

		file = zend_hash_str_find(Z_ARRVAL_P(tmp), ZEND_STRL("file"));
		line = zend_hash_str_find(Z_ARRVAL_P(tmp), ZEND_STRL("line"));
		zend_hash_move_forward_ex(Z_ARRVAL(zbacktrace), &position);
	}

	if(file)
	{
		p = Z_STRLEN_P(file);
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;

		strcpy(cur, Z_STRVAL_P(file));
		cur += Z_STRLEN_P(file) + 1;
		i++;

		p = (uint32_t)Z_LVAL_P(line);
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;
	}

	zval_ptr_dtor_nogc(&zbacktrace);
	zend_string_release(Z_STR(startFile));

	return output;
}

#endif