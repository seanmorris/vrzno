#include "vrzno_private.h"

#include "sapi/phpdbg/phpdbg.h"
#include "sapi/phpdbg/phpdbg_cmd.h"
#include "zend_builtin_functions.h"

__attribute__((weak)) int phpdbg_arm_auto_global(zval *ptrzv);
__attribute__((weak)) void phpdbg_restore_frame(void);
__attribute__((weak)) ZEND_EXTERN_MODULE_GLOBALS(phpdbg);

/* Symbol scopes for vrzno_dbg_dump_symbols(). 0 and 1 match the former bool argument. */
#define VRZNO_DBG_SCOPE_LOCALS       0
#define VRZNO_DBG_SCOPE_SUPERGLOBALS 1
#define VRZNO_DBG_SCOPE_GLOBALS      2

/*
 * Version of the debugger exports in this file, so JavaScript callers can pick
 * a code path without probing for each symbol.
 *
 * 1: this function is absent. vrzno_dbg_dump_symbols() takes a bool, and
 *    vrzno_dbg_dump_backtrace() is the only frame listing.
 * 2: adds vrzno_dbg_dump_frames(), scope 2 for vrzno_dbg_dump_symbols(), and
 *    the vrzno_dbg_zval_*() helpers.
 */
int EMSCRIPTEN_KEEPALIVE vrzno_dbg_api_version(void)
{
	return 2;
}

/* The sizing pass and the writing pass of a symbol dump must agree on this. */
static bool vrzno_dbg_symbol_in_scope(zend_string *name, int scope)
{
	if(scope == VRZNO_DBG_SCOPE_GLOBALS)
	{
		return true;
	}

	return scope == VRZNO_DBG_SCOPE_SUPERGLOBALS
		? zend_is_auto_global(name)
		: !zend_is_auto_global(name);
}

static char *vrzno_dbg_put_word(char *cur, size_t value)
{
	memcpy(cur, &value, sizeof value);

	return cur + sizeof value;
}

static char *vrzno_dbg_put_string(char *cur, const char *value, size_t len)
{
	cur = vrzno_dbg_put_word(cur, len);
	memcpy(cur, value, len);
	cur[len] = 0;

	return cur + len + 1;
}

/*
 * Dumps a symbol table as [byte length][zval pointer][name length][name\0]...
 * scope: 0 for the current frame's variables, 1 for superglobals, 2 for
 * every global variable, user-defined ones included.
 */
char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_symbols(int scope)
{
	if(!EG(current_execute_data) || !EG(current_execute_data)->func)
	{
		fprintf(stderr, "No active op array: %s:%d\n", __FILE__, __LINE__);
		return NULL;
	}

	zend_array *symtable;

	if(scope != VRZNO_DBG_SCOPE_LOCALS)
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
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(symtable, varName, zv)
#else
		ZEND_HASH_FOREACH_STR_KEY_VAL(symtable, varName, zv)
#endif
		{
			if(!varName) continue;

			if(vrzno_dbg_symbol_in_scope(varName, scope))
			{
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

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_STR_KEY_VAL(symtable, varName, zv)
#else
		ZEND_HASH_FOREACH_STR_KEY_VAL(symtable, varName, zv)
#endif
		{
			if(!varName) continue;

			if(vrzno_dbg_symbol_in_scope(varName, scope))
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
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_PTR(EG(zend_constants), zc)
#else
		ZEND_HASH_FOREACH_PTR(EG(zend_constants), zc)
#endif
		{
			if(ZEND_CONSTANT_MODULE_NUMBER(zc) == PHP_USER_CONSTANT)
			{
				len += sizeof(void*)     // zval pointer
					+ sizeof(size_t)     // name length
					+ ZSTR_LEN(zc->name) // name
					+ 1;                 // NUL
			}
		} ZEND_HASH_FOREACH_END();

		output = malloc(sizeof(len) + len);
		char *cur = output;

		void *p = len;
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_PTR(EG(zend_constants), zc)
#else
		ZEND_HASH_FOREACH_PTR(EG(zend_constants), zc)
#endif
		{
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
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_PTR(EG(class_table), ce)
#else
		ZEND_HASH_FOREACH_PTR(EG(class_table), ce)
#endif
		{
			if (ce->type != ZEND_USER_CLASS) continue;

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

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_PTR(EG(class_table), ce)
#else
		ZEND_HASH_FOREACH_PTR(EG(class_table), ce)
#endif
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
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_PTR(EG(function_table), zf)
#else
		ZEND_HASH_FOREACH_PTR(EG(function_table), zf)
#endif
		{
			if (zf->type != ZEND_USER_FUNCTION) continue;

			zend_op_array *op_array = &zf->op_array;

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

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_PTR(EG(function_table), zf)
#else
		ZEND_HASH_FOREACH_PTR(EG(function_table), zf)
#endif
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
	size_t len = 0;
	char *output = NULL;

	zend_try
	{
#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_STR_KEY(&EG(included_files), filename)
#else
		ZEND_HASH_FOREACH_STR_KEY(&EG(included_files), filename)
#endif
		{
			len += sizeof(size_t) + ZSTR_LEN(filename) + 1;
		} ZEND_HASH_FOREACH_END();

		output = malloc(sizeof(len) + len);
		char *cur = output;

		void *p = len;
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
		ZEND_HASH_MAP_FOREACH_STR_KEY(&EG(included_files), filename)
#else
		ZEND_HASH_FOREACH_STR_KEY(&EG(included_files), filename)
#endif
		{
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
		len += sizeof(size_t)               // name length
			+ (file ? Z_STRLEN_P(file) : 0) // name
			+ 1                             // NUL
			+ sizeof(uint32_t);             // line number

		file = zend_hash_str_find(Z_ARRVAL_P(tmp), ZEND_STRL("file"));
		zend_hash_move_forward_ex(Z_ARRVAL(zbacktrace), &position);
	}

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

		p = (uint32_t)Z_LVAL_P(line);
		memcpy(cur, &p, sizeof p);
		cur += sizeof p;
	}

	zval_ptr_dtor_nogc(&zbacktrace);
	zend_string_release(Z_STR(startFile));

	return output;
}

int EMSCRIPTEN_KEEPALIVE vrzno_dbg_switch_frame(int frame)
{
	if(!phpdbg_restore_frame)
	{
		fprintf(stderr, "Not running in PHPDPG SAPI.\n");
		return -1;
	}

	if(PHPDBG_FRAME(num) == frame)
	{
		return PHPDBG_FRAME(num);
	}

	int oldFrame = PHPDBG_FRAME(num);

	zend_execute_data *execute_data = PHPDBG_FRAME(num)
		? PHPDBG_FRAME(execute_data)
		: EG(current_execute_data);

	int i = 0;

	zend_try
	{
		while(execute_data)
		{
			if(i++ == frame)
			{
				break;
			}

			do
			{
				execute_data = execute_data->prev_execute_data;
			} while(execute_data && execute_data->opline == NULL);
		}
	}
	zend_catch
	{
		fprintf(stderr, "Couldn't switch frames, invalid data source: %s:%d\n", __FILE__, __LINE__);
		return -1;
	} zend_end_try();

	if(execute_data == NULL)
	{
		fprintf(stderr, "No frame #%d\n", frame);
		return -1;
	}

	phpdbg_restore_frame();

	if(frame > 0)
	{
		PHPDBG_FRAME(num) = frame;
		PHPDBG_FRAME(execute_data) = EG(current_execute_data);
		EG(current_execute_data) = execute_data;
	}

	return oldFrame;
}

/* Steps to the caller the way vrzno_dbg_switch_frame() counts frames. */
static zend_execute_data *vrzno_dbg_caller(zend_execute_data *execute_data)
{
	do
	{
		execute_data = execute_data->prev_execute_data;
	} while(execute_data && execute_data->opline == NULL);

	return execute_data;
}

typedef struct {
	const char *file;
	size_t file_len;
	const char *function;
	size_t function_len;
	const char *class_name;
	size_t class_len;
	uint32_t line;
} vrzno_dbg_frame;

static void vrzno_dbg_read_frame(zend_execute_data *execute_data, vrzno_dbg_frame *frame)
{
	zend_function *func = execute_data->func;

	memset(frame, 0, sizeof *frame);
	frame->file = frame->function = frame->class_name = "";

	if(!func)
	{
		return;
	}

	if(func->common.function_name)
	{
		frame->function = ZSTR_VAL(func->common.function_name);
		frame->function_len = ZSTR_LEN(func->common.function_name);
	}

	if(func->common.scope)
	{
		frame->class_name = ZSTR_VAL(func->common.scope->name);
		frame->class_len = ZSTR_LEN(func->common.scope->name);
	}

	if(!ZEND_USER_CODE(func->type) || !execute_data->opline)
	{
		return;
	}

	if(func->op_array.filename)
	{
		frame->file = ZSTR_VAL(func->op_array.filename);
		frame->file_len = ZSTR_LEN(func->op_array.filename);
	}

	/* Same rule as zend_get_executed_lineno(). */
	if(EG(exception)
		&& execute_data->opline->opcode == ZEND_HANDLE_EXCEPTION
		&& execute_data->opline->lineno == 0
		&& EG(opline_before_exception)
	){
		frame->line = EG(opline_before_exception)->lineno;
	}
	else
	{
		frame->line = execute_data->opline->lineno;
	}
}

/*
 * Dumps the stack from the innermost frame, whichever frame is selected, as
 * [byte length] then per frame
 * [file length][file\0][line][function length][function\0][class length][class\0][flags].
 * Flag bit 0 marks the frame selected with vrzno_dbg_switch_frame(). The
 * function is empty for top-level code and the class is empty outside a class.
 *
 * Frames are counted exactly as vrzno_dbg_switch_frame() counts them, so a
 * frame's position in this dump is the index to switch to.
 */
char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_dump_frames(void)
{
	zend_execute_data *top = EG(current_execute_data);
	zend_execute_data *execute_data;
	vrzno_dbg_frame frame;
	size_t len = 0;
	int selected = 0;
	int i;

	if(phpdbg_restore_frame && PHPDBG_FRAME(num) > 0)
	{
		/* While a frame is selected, phpdbg keeps the real one here. */
		top = PHPDBG_FRAME(execute_data);
		selected = PHPDBG_FRAME(num);
	}

	for(execute_data = top; execute_data; execute_data = vrzno_dbg_caller(execute_data))
	{
		vrzno_dbg_read_frame(execute_data, &frame);

		len += sizeof(size_t) + frame.file_len + 1
			+ sizeof(size_t)
			+ sizeof(size_t) + frame.function_len + 1
			+ sizeof(size_t) + frame.class_len + 1
			+ sizeof(size_t);
	}

	char *output = malloc(sizeof(len) + len);

	if(!output)
	{
		return NULL;
	}

	char *cur = vrzno_dbg_put_word(output, len);

	for(execute_data = top, i = 0; execute_data; execute_data = vrzno_dbg_caller(execute_data), i++)
	{
		vrzno_dbg_read_frame(execute_data, &frame);

		cur = vrzno_dbg_put_string(cur, frame.file, frame.file_len);
		cur = vrzno_dbg_put_word(cur, frame.line);
		cur = vrzno_dbg_put_string(cur, frame.function, frame.function_len);
		cur = vrzno_dbg_put_string(cur, frame.class_name, frame.class_len);
		cur = vrzno_dbg_put_word(cur, i == selected ? 1 : 0);
	}

	return output;
}

/* Follows a symbol-table slot or reference to the value it holds. */
static zval *vrzno_dbg_resolve(zval *zv)
{
	ZVAL_DEREF(zv);

	if(Z_TYPE_P(zv) == IS_INDIRECT)
	{
		zv = Z_INDIRECT_P(zv);
		ZVAL_DEREF(zv);
	}

	return zv;
}

/* The Zend type of the value: IS_UNDEF (0) for a variable not assigned yet. */
int EMSCRIPTEN_KEEPALIVE vrzno_dbg_zval_type(zval *zv)
{
	return Z_TYPE_P(vrzno_dbg_resolve(zv));
}

/* Whether the variable is a PHP reference. */
int EMSCRIPTEN_KEEPALIVE vrzno_dbg_zval_is_reference(zval *zv)
{
	if(Z_TYPE_P(zv) == IS_INDIRECT)
	{
		zv = Z_INDIRECT_P(zv);
	}

	return Z_ISREF_P(zv) ? 1 : 0;
}

/* The class name of an object, or NULL for anything else. */
const char* EMSCRIPTEN_KEEPALIVE vrzno_dbg_zval_class(zval *zv)
{
	zv = vrzno_dbg_resolve(zv);

	return Z_TYPE_P(zv) == IS_OBJECT ? ZSTR_VAL(Z_OBJCE_P(zv)->name) : NULL;
}

/* Elements in an array or bytes in a string; -1 for anything else. */
int EMSCRIPTEN_KEEPALIVE vrzno_dbg_zval_count(zval *zv)
{
	zv = vrzno_dbg_resolve(zv);

	switch(Z_TYPE_P(zv))
	{
		case IS_ARRAY:
			return (int) zend_hash_num_elements(Z_ARRVAL_P(zv));

		case IS_STRING:
			return (int) Z_STRLEN_P(zv);

		default:
			return -1;
	}
}
