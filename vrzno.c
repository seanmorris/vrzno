/* vrzno extension for PHP */

#include "vrzno_private.h"
#include <vrzno_js.h>
#include "vrzno_arginfo.h"

PHP_RSHUTDOWN_FUNCTION(vrzno)
{
	vrzno_js_shutdown();

	return SUCCESS;
}

PHP_MINIT_FUNCTION(vrzno)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Vrzno", class_Vrzno_methods);
	vrzno_class_entry = zend_register_internal_class(&ce);

	vrzno_class_entry->create_object = vrzno_create_object;
	vrzno_class_entry->get_iterator  = vrzno_array_get_iterator;

#if PHP_VERSION_ID >= 80100
	vrzno_class_entry->ce_flags |= ZEND_ACC_NOT_SERIALIZABLE;
#else
	vrzno_class_entry->serialize = zend_class_serialize_deny;
	vrzno_class_entry->unserialize = zend_class_unserialize_deny;
#endif

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 2
	vrzno_class_entry->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
	zend_string *attribute_name_AllowDynamicProperties_class_vrzno = zend_string_init_interned("AllowDynamicProperties", sizeof("AllowDynamicProperties") - 1, 1);
	zend_add_class_attribute(vrzno_class_entry, attribute_name_AllowDynamicProperties_class_vrzno, 0);
	zend_string_release(attribute_name_AllowDynamicProperties_class_vrzno);
#endif

	memcpy(&vrzno_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	vrzno_object_handlers.offset = XtOffsetOf(vrzno_object, zo);
	vrzno_object_handlers.clone_obj = NULL;

	vrzno_object_handlers.get_properties_for = vrzno_get_properties_for;
	vrzno_object_handlers.unset_property     = vrzno_unset_property;
	vrzno_object_handlers.write_property     = vrzno_write_property;
	vrzno_object_handlers.read_property      = vrzno_read_property;
	vrzno_object_handlers.free_obj           = vrzno_object_free;
	vrzno_object_handlers.has_property       = vrzno_has_property;
	vrzno_object_handlers.read_dimension     = vrzno_read_dimension;
	vrzno_object_handlers.write_dimension    = vrzno_write_dimension;
	vrzno_object_handlers.has_dimension      = vrzno_has_dimension;
	vrzno_object_handlers.unset_dimension    = vrzno_unset_dimension;
	// vrzno_object_handlers.get_class_name     = vrzno_get_class_name;

	php_register_url_stream_wrapper("http", &php_stream_fetch_wrapper);
	php_register_url_stream_wrapper("https", &php_stream_fetch_wrapper);

#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	vrzno_js_init();

	return SUCCESS;
}

PHP_MINFO_FUNCTION(vrzno)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "Vrzno support for php-wasm", "enabled");
	php_info_print_table_row(2, "Version", PHP_VRZNO_VERSION);
	php_info_print_table_end();

	if (!sapi_module.phpinfo_as_text) {
		php_info_print_box_start(0);
		PUTS("Find <a target = \"_blank\" href=\"https://github.com/seanmorris/php-wasm\">php-wasm</a>");
		PUTS(" & <a target = \"_blank\" href=\"https://github.com/seanmorris/vrzno\">vrzno</a> on github!");
		PUTS("<a target = \"_blank\" href=\"https://github.com/seanmorris/php-wasm\">");
		PUTS("<img border=\"0\" src=\"");
		PUTS(VRZNO_ICON_DATA_URI "\" alt=\"Sean logo\" /></a>\n");
		php_info_print_box_end();
	}
}

zend_module_entry vrzno_module_entry = {
	STANDARD_MODULE_HEADER,
	"vrzno",
	ext_functions,             /* zend_function_entry */
	PHP_MINIT(vrzno),          /* PHP_MINIT - Module initialization */
	NULL,                      /* PHP_MSHUTDOWN - Module shutdown */
	NULL,                      /* PHP_RINIT - Request initialization */
	PHP_RSHUTDOWN(vrzno),      /* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(vrzno),          /* PHP_MINFO - Module info */
	PHP_VRZNO_VERSION,         /* Version */
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_VRZNO
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(vrzno)
#endif

zval* EMSCRIPTEN_KEEPALIVE vrzno_exec_callback(zend_function *func, zval *argv, int argc, zend_object *zo)
{
	zend_fcall_info fci = {0};
	zend_fcall_info_cache fcc = {0};

	fci.size = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);

	fci.retval = emalloc(sizeof(zval));
	ZVAL_UNDEF(fci.retval);
	fci.params = argv;
	fci.named_params = 0;
	fci.param_count = argc;

	fcc.function_handler = func;
	fcc.calling_scope = NULL;
	fcc.called_scope = NULL;

	fci.object = NULL;
	fcc.object = NULL;

	if(zo)
	{
		fci.object = zo;
		fcc.object = zo;
	}

	if(zend_call_function(&fci, &fcc) == SUCCESS)
	{
		return fci.retval;
	}

	vrzno_expose_destroy_zval(fci.retval);
	return NULL;
}

zval* EMSCRIPTEN_KEEPALIVE vrzno_exec_zval_callback(zval *callback, zval *argv, int argc)
{
	zval *retval = emalloc(sizeof(zval));
	ZVAL_UNDEF(retval);

	if(call_user_function(EG(function_table), NULL, callback, retval, (uint32_t) argc, argv) == SUCCESS)
	{
		return retval;
	}

	vrzno_expose_destroy_zval(retval);
	return NULL;
}
