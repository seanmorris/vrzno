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
	const response = await fetch(pathString, context);
	const buffer = new Uint8Array( await response.arrayBuffer() );
	const status = response.status;

	const headerLines = [...response.headers.entries()].map(([key, val]) => `${key}: ${val}`);
	headerLines.unshift(`HTTP/1.1 ${response.status} ${response.statusText}`);

	const headersloc = _malloc(ptrsize * headerLines.length);
	setValue(headersv, headersloc, '*');
	setValue(headersc, headerLines.length, 'i32');

	let i = 0;
	for(const line of headerLines)
	{
		const len = lengthBytesUTF8(line);
		const loc = _malloc(len);
		stringToUTF8(line, loc, len);
		setValue(headersloc + (i * ptrsize), loc, 'i' + (8 * ptrsize));
		i++;
	}

	const parsed = {status, buffer, context};
	Module.tacked.add(parsed);
	Module.tacked.delete(context);
	return Module.targets.add(parsed);
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

			} }, context, Z_STRVAL_P(tmpzval));
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

							console.log(context.headers);
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

						console.log(context.headers);
					});
				})() }, contextId, Z_STRVAL_P(tmpheader));
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

	char **headersv;
	size_t headersc;

	self = emalloc(sizeof(*self));
	self->fpos = 0;
	self->targetId = php_stream_fetch_real_open(path, context, sizeof(char*), &headersv, &headersc);

	php_stream_notify_info(context, PHP_STREAM_NOTIFY_CONNECT, NULL, 0);

	if(!ignoreErrors)
	{
		int status = EM_ASM_INT({ {
			const {status} = Module.targets.get($0);
			return status;
		} }, self->targetId);

		if(status == 403)
		{
			php_stream_notify_error(context, PHP_STREAM_NOTIFY_AUTH_RESULT, "HTTP 403 Auth Error!", status);
		}
		else if(status >= 400)
		{
			php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "HTTP 4XX Error!", status);
		}
	}

	zval *response_header = NULL;
	array_init(response_header);

	for(int i = 0; i < headersc; i++)
	{
		zval http_response;
		char *line = headersv[i];
		ZVAL_STRINGL(&http_response, line, strlen(line));
		zend_hash_next_index_insert(Z_ARRVAL_P(response_header), &http_response);
		free(line);
	}

	stream = php_stream_alloc(&php_stream_fetch_io_ops, self, NULL, mode);
	ZVAL_COPY(&stream->wrapperdata, response_header);
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
