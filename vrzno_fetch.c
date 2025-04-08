typedef struct {
	jstarget* targetId;
	size_t fpos;
} php_stream_fetch_data;

static ssize_t php_stream_fetch_read(php_stream *stream, char *buf, size_t count)
{
	php_stream_fetch_data *self = (php_stream_fetch_data*)stream->abstract;

	ssize_t read = EM_ASM_PTR({
		const target = Module.targets.get($0);
		const dest = $1;
		const fpos = $2;
		let count = $3;

		if(target.status >= 400 && !target.context.ignoreErrors)
		{
			return 0;
		}

		if(fpos >= target.buffer.length)
		{
			count = 0;
		}
		else if(fpos + count > target.buffer.length)
		{
			count = target.buffer.length - fpos;
		}

		if(count)
		{
			Module.HEAPU8.set(target.buffer.slice(fpos, fpos + count), dest);
		}

		return count;

	}, self->targetId, buf, self->fpos, count);

	self->fpos += read;
	stream->eof = read ? 0 : 1;
	return read;
}

static int php_stream_fetch_close(php_stream *stream, int close_handle)
{
	php_stream_fetch_data *self = (php_stream_fetch_data*)stream->abstract;

	EM_ASM({
		const parsed = Module.targets.get($0);
		Module.tacked.delete(parsed);
	}, self->targetId);

	efree(self);

	return 0;
}

const php_stream_ops php_stream_fetch_io_ops = {
	NULL, /* write */
	php_stream_fetch_read,
	php_stream_fetch_close,
	NULL, /* flush */
	"fetch",
	NULL, /* seek */
	NULL, /* cast */
	NULL, /* stat */
	NULL  /* set_option */
};

EM_ASYNC_JS(ssize_t, php_stream_fetch_real_open, (
	const char *path,
	jstarget *_context,
	size_t ptrsize,
	char ***headersv,
	size_t *headersc
), {
	const pathString = UTF8ToString(path);
	const context = Module.targets.get(_context) || {};

	try
	{
		const response = await fetch(pathString, context);
		const buffer = new Uint8Array( await response.arrayBuffer() );
		const status = response.status;

		const headerLines = [...response.headers.entries()].map(([key, val]) => `${key}: ${val}`);
		headerLines.unshift(`HTTP/1.1 ${response.status} ${response.statusText}`);

		const headersloc = _malloc(ptrsize * headerLines.length); // free()'d in php_stream_fetch_open
		setValue(headersv, headersloc, '*');
		setValue(headersc, headerLines.length, 'i32');

		let i = 0;
		for(const line of headerLines)
		{
			const len = lengthBytesUTF8(line) + 1;
			const loc = _malloc(len); // free()'d in php_stream_fetch_open
			stringToUTF8(line, loc, len);
			setValue(headersloc + (i * ptrsize), loc, 'i' + (8 * ptrsize));
			i++;
		}

		const parsed = {status, buffer, context};
		Module.tacked.add(parsed);
		Module.tacked.delete(context);
		return Module.targets.add(parsed);
	}
	catch(error)
	{
		const parsed = {status: -1, buffer: new TextEncoder().encode(error), context};
		Module.tacked.add(parsed);
		Module.tacked.delete(context);
		return Module.targets.add(parsed);
	}
});

php_stream *php_stream_fetch_open(
	php_stream_wrapper *wrapper,
	const char *path,
	const char *mode,
	int options,
	zend_string **opened_path,
	php_stream_context *context STREAMS_DC
){
	if(strpbrk(mode, "awx+"))
	{
		return NULL; // Fetch wrapper does not support writeable connections
	}

	zval *tmpzval;
	jstarget *contextId = NULL;
	bool ignoreErrors = false;
	if(context)
	{
		contextId = EM_ASM_INT({
			const context = {};
			Module.tacked.add(context);
			return Module.targets.add(context);
		});

		if((tmpzval = php_stream_context_get_option(context, "http", "method")) != NULL)
		{
			EM_ASM({ {
				const context = Module.targets.get($0);
				const method = UTF8ToString($1);
				context.method = method;
			} }, contextId, Z_STRVAL_P(tmpzval));
		}

		if((tmpzval = php_stream_context_get_option(context, "http", "header")) != NULL)
		{
			zval *tmpheader = NULL;
			if (Z_TYPE_P(tmpzval) == IS_ARRAY)
			{
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(tmpzval), tmpheader) {
					if(Z_TYPE_P(tmpheader) == IS_STRING)
					{
						EM_ASM({ (() => {
							const context = Module.targets.get($0);
							const headerLine = UTF8ToString($1);
							const colon = headerLine.indexOf(':');

							const key = headerLine.substr(0, colon).trim();
							const val = headerLine.substr(1 + colon).trim();

							context.headers = context.headers ?? {};
							context.headers[key] = val;
						})() }, contextId, Z_STRVAL_P(tmpheader));
					}
				} ZEND_HASH_FOREACH_END();
			}
			else if(Z_TYPE_P(tmpzval) == IS_STRING && Z_STRLEN_P(tmpzval))
			{
				EM_ASM({ (() => {
					const context = Module.targets.get($0);
					const headerLines = UTF8ToString($1);

					headerLines.split('\n').forEach(headerLine => {
						const context = Module.targets.get($0);
						const colon = headerLine.indexOf(':');

						const key = headerLine.substr(0, colon).trim();
						const val = headerLine.substr(1 + colon).trim();

						context.headers = context.headers ?? {};
						context.headers[key] = val;
					});
				})() }, contextId, Z_STRVAL_P(tmpzval));
			}
		}

		if((tmpzval = php_stream_context_get_option(context, "http", "content")) != NULL)
		{
			EM_ASM({ (() => {
				const context = Module.targets.get($0);
				context.body = Module.HEAPU8.slice($1, $1 + $2);
			})() }, contextId, Z_STRVAL_P(tmpzval), Z_STRLEN_P(tmpzval));
		}

		if((tmpzval = php_stream_context_get_option(context, "http", "ignore_errors")) != NULL)
		{
			ignoreErrors = zend_is_true(tmpzval);

			EM_ASM({ {
				const context = Module.targets.get($0);
				context.ignoreErrors = $1;
			} }, contextId, ignoreErrors);
		}
	}

	php_stream_fetch_data *self;
	php_stream *stream = NULL;

	char **headersv = NULL;
	size_t headersc = 0;

	self = emalloc(sizeof(*self));
	self->fpos = 0;
	self->targetId = php_stream_fetch_real_open(path, contextId, sizeof(char*), &headersv, &headersc);

	php_stream_notify_info(context, PHP_STREAM_NOTIFY_CONNECT, NULL, 0);

	int status = EM_ASM_INT({ {
		const parsed = Module.targets.get($0);
		if(parsed.status < 0)
		{
			Module.tacked.delete(parsed);
		}
		return parsed.status;
	} }, self->targetId);

	if(!ignoreErrors)
	{
		if(status == 403)
		{
			php_stream_notify_error(context, PHP_STREAM_NOTIFY_AUTH_RESULT, "HTTP 403 Auth Error!", status);
		}
		else if(status >= 400 && status < 500)
		{
			php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "HTTP 4XX Error!", status);
			php_stream_wrapper_log_error(wrapper, options, "HTTP %d Error!", status);
		}
		else if(status >= 500 && status < 600)
		{
			php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "HTTP 5XX Error!", status);
			php_stream_wrapper_log_error(wrapper, options, "HTTP %d Error!", status);
		}
		else if(status >= 600)
		{
			php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "HTTP UNKNOWN Error!", status);
			php_stream_wrapper_log_error(wrapper, options, "HTTP %d Error!", status);
		}
		else if(status == -1)
		{
			php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "Unexpected HTTP Error: %d", status);
			php_stream_wrapper_log_error(wrapper, options, "Unexpected HTTP Error: %d", status);
		}
	}

	zval *response_header = NULL;
	array_init(response_header);

	for(int i = 0; i < headersc; i++)
	{
		if(status >= 0)
		{
			zval http_response;
			char *line = headersv[i];
			ZVAL_STRINGL(&http_response, line, strlen(line));
			zend_hash_next_index_insert(Z_ARRVAL_P(response_header), &http_response);
		}

		free(headersv[i]); // malloc()'d in php_stream_fetch_real_open
	}

	free(headersv); // malloc()'d in php_stream_fetch_real_open

	if(status == -1)
	{
		return NULL;
	}

	stream = php_stream_alloc(&php_stream_fetch_io_ops, self, NULL, mode);

	ZVAL_COPY(&stream->wrapperdata, response_header);

	zend_set_local_var_str(
		"http_response_header",
		-1 + sizeof("http_response_header"),
		&stream->wrapperdata,
		0
	);

	stream->flags |= PHP_STREAM_FLAG_NO_BUFFER;
	stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
	stream->eof = 0;

	return stream;
}

static const php_stream_wrapper_ops php_stream_fetch_wops = {
	php_stream_fetch_open,
	NULL,	/* close */
	NULL,	/* fstat */
	NULL,	/* stat */
	NULL,	/* opendir */
	"vrzno fetch wrapper",
	NULL,	/* unlink */
	NULL,	/* rename */
	NULL,	/* mkdir */
	NULL,	/* rmdir */
	NULL	/* metadata */
};

const php_stream_wrapper php_stream_fetch_wrapper = {
	&php_stream_fetch_wops,
	NULL,
	1 /* is_url */
};
