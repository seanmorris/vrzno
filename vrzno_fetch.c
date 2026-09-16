#include "vrzno_private.h"
#include <vrzno_fetch_js.h>

typedef struct {
	vrzno_target_id targetId;
	size_t fpos;
} php_stream_fetch_data;

static ssize_t php_stream_fetch_read(php_stream *stream, char *buf, size_t count)
{
	php_stream_fetch_data *self = (php_stream_fetch_data*)stream->abstract;

	ssize_t read = vrzno_js_fetch_read(self->targetId, buf, self->fpos, count);

	self->fpos += read;
	stream->eof = read ? 0 : 1;
	return read;
}

static int php_stream_fetch_close(php_stream *stream, int close_handle)
{
	php_stream_fetch_data *self = (php_stream_fetch_data*)stream->abstract;

	vrzno_js_fetch_release(self->targetId);

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
	vrzno_target_id contextId = 0;
	bool ignoreErrors = false;
	if(context)
	{
		contextId = vrzno_js_fetch_context();

		if((tmpzval = php_stream_context_get_option(context, "http", "method")) != NULL)
		{
			if(Z_TYPE_P(tmpzval) == IS_STRING)
			{
				vrzno_js_fetch_method(contextId, Z_STRVAL_P(tmpzval));
			}
		}

		if((tmpzval = php_stream_context_get_option(context, "http", "header")) != NULL)
		{
			zval *tmpheader = NULL;
			if (Z_TYPE_P(tmpzval) == IS_ARRAY)
			{
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(tmpzval), tmpheader) {
					if(Z_TYPE_P(tmpheader) == IS_STRING)
					{
						vrzno_js_fetch_header(contextId, Z_STRVAL_P(tmpheader));
					}
				} ZEND_HASH_FOREACH_END();
			}
			else if(Z_TYPE_P(tmpzval) == IS_STRING && Z_STRLEN_P(tmpzval))
			{
				vrzno_js_fetch_headers(contextId, Z_STRVAL_P(tmpzval));
			}
		}

		if((tmpzval = php_stream_context_get_option(context, "http", "content")) != NULL)
		{
			if(Z_TYPE_P(tmpzval) == IS_STRING)
			{
				vrzno_js_fetch_body(contextId, Z_STRVAL_P(tmpzval), Z_STRLEN_P(tmpzval));
			}
		}

		if((tmpzval = php_stream_context_get_option(context, "http", "ignore_errors")) != NULL)
		{
			ignoreErrors = zend_is_true(tmpzval);

			vrzno_js_fetch_ignore_errors(contextId, ignoreErrors);
		}
	}

	php_stream_fetch_data *self;
	php_stream *stream = NULL;

	char **headersv = NULL;
	size_t headersc = 0;

	self = emalloc(sizeof(*self));
	self->fpos = 0;
	self->targetId = php_stream_fetch_real_open(path, contextId, sizeof(char*), &headersv, &headersc);

	if(context)
	{
		php_stream_notify_info(context, PHP_STREAM_NOTIFY_CONNECT, NULL, 0);
	}

	int status = vrzno_js_fetch_status(self->targetId);

	bool failed = status < 0 || (!ignoreErrors && status >= 400);

	if(failed)
	{
		if(status == 403)
		{
			if(context)
			{
				php_stream_notify_error(context, PHP_STREAM_NOTIFY_AUTH_RESULT, "HTTP 403 Auth Error", status);
			}
			php_stream_wrapper_log_error(wrapper, options, "HTTP request failed with status 403");
		}
		else if(status >= 400 && status < 500)
		{
			if(context)
			{
				php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "HTTP client error", status);
			}
			php_stream_wrapper_log_error(wrapper, options, "HTTP request failed with status %d", status);
		}
		else if(status >= 500 && status < 600)
		{
			if(context)
			{
				php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "HTTP server error", status);
			}
			php_stream_wrapper_log_error(wrapper, options, "HTTP request failed with status %d", status);
		}
		else if(status >= 600)
		{
			if(context)
			{
				php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "Unknown HTTP error", status);
			}
			php_stream_wrapper_log_error(wrapper, options, "HTTP request failed with status %d", status);
		}
		else
		{
			if(context)
			{
				php_stream_notify_error(context, PHP_STREAM_NOTIFY_FAILURE, "JavaScript fetch failed", status);
			}
			php_stream_wrapper_log_error(wrapper, options, "JavaScript fetch failed");
		}
	}

	zval response_header;
	array_init(&response_header);

	for(int i = 0; i < headersc; i++)
	{
		if(status >= 0)
		{
			zval http_response;
			char *line = headersv[i];
			ZVAL_STRINGL(&http_response, line, strlen(line));
			zend_hash_next_index_insert(Z_ARRVAL(response_header), &http_response);
		}

		free(headersv[i]); // malloc()'d in php_stream_fetch_real_open
	}

	free(headersv); // malloc()'d in php_stream_fetch_real_open

	if(failed)
	{
		if(FAILURE == zend_set_local_var_str(
			"http_response_header",
			sizeof("http_response_header") - 1,
			&response_header,
			0
		))
		{
			zval_ptr_dtor(&response_header);
		}
		vrzno_js_fetch_release(self->targetId);
		efree(self);
		return NULL;
	}

	stream = php_stream_alloc(&php_stream_fetch_io_ops, self, NULL, mode);
	if(!stream)
	{
		zval_ptr_dtor(&response_header);
		vrzno_js_fetch_release(self->targetId);
		efree(self);
		return NULL;
	}

	ZVAL_COPY(&stream->wrapperdata, &response_header);

	if(FAILURE == zend_set_local_var_str(
		"http_response_header",
		sizeof("http_response_header") - 1,
		&response_header,
		0
	))
	{
		zval_ptr_dtor(&response_header);
	}

	stream->flags |= PHP_STREAM_FLAG_NO_BUFFER;
	stream->flags |= PHP_STREAM_FLAG_NO_SEEK;
	stream->eof = 0;

	if(opened_path)
	{
		*opened_path = zend_string_init(path, strlen(path), 0);
	}

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
