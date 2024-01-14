/* vrzno extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "php_vrzno.h"
#include "../pdo/php_pdo_driver.h"
#include "../json/php_json.h"
#include "../json/php_json_encoder.h"
#include "../json/php_json_parser.h"
#include "zend_API.h"
#include "zend_types.h"
#include "zend_closures.h"
#include <emscripten.h>
#include "zend_attributes.h"
#include "zend_hash.h"

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

#include "vrzno_object.c"
#include "vrzno_array.c"
#include "vrzno_expose.c"
#include "vrzno_functions.c"
#include "vrzno_db_statement.c"
#include "vrzno_db.c"

PHP_RINIT_FUNCTION(vrzno)
{
	return SUCCESS;
}

PHP_MINIT_FUNCTION(vrzno)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Vrzno", vrzno_vrzno_methods);
	vrzno_class_entry = zend_register_internal_class(&ce);

	vrzno_class_entry->create_object = vrzno_create_object;
	vrzno_class_entry->get_iterator  = vrzno_array_get_iterator;
	vrzno_class_entry->ce_flags     |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;

	zend_string *attribute_name_AllowDynamicProperties_class_vrzno = zend_string_init_interned("AllowDynamicProperties", sizeof("AllowDynamicProperties") - 1, 1);
	zend_add_class_attribute(vrzno_class_entry, attribute_name_AllowDynamicProperties_class_vrzno, 0);
	zend_string_release(attribute_name_AllowDynamicProperties_class_vrzno);

	memcpy(&vrzno_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

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
	vrzno_object_handlers.get_class_name     = vrzno_get_class_name;
#if defined(ZTS) && defined(COMPILE_DL_VRZNO)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return php_pdo_register_driver(&pdo_vrzno_driver);
}

PHP_MINFO_FUNCTION(vrzno)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "vrzno support", "enabled");
	php_info_print_table_end();
}

zend_module_entry vrzno_module_entry = {
	STANDARD_MODULE_HEADER,
	"vrzno",
	vrzno_functions,           /* zend_function_entry */
	PHP_MINIT(vrzno),          /* PHP_MINIT - Module initialization */
	NULL,                      /* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(vrzno),          /* PHP_RINIT - Request initialization */
	NULL,                      /* PHP_RSHUTDOWN - Request shutdown */
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

int EMSCRIPTEN_KEEPALIVE vrzno_exec_callback(zend_function *fptr, zval **argv, int argc)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	zval params[argc];

	fci.size             = sizeof(fci);
	ZVAL_UNDEF(&fci.function_name);
	fci.object           = NULL;
	fci.retval           = (zval*) emalloc(sizeof(zval));
	fci.params           = params;
	fci.named_params     = 0;
	fci.param_count      = argc;

	fcc.function_handler = fptr;
	fcc.calling_scope    = NULL;
	fcc.called_scope     = NULL;
	fcc.object           = NULL;

	if(argv)
	{
		int i;
		for(i = 0; i < argc; i++)
		{
			params[i] = *argv[i];
		}
	}

	// EM_ASM({ console.log('exec', $0, $1, $2) }, fptr, argv, argc);


	if(zend_call_function(&fci, &fcc) == SUCCESS)
	{
		// zval *retZval = (zval*) emalloc(sizeof(zval));

		// ZVAL_UNDEF(retZval);
		// ZVAL_COPY_VALUE(retZval, &retval);

		// return retZval;

		return fci.retval;
	}

	return NULL;
}

int EMSCRIPTEN_KEEPALIVE vrzno_del_callback(zend_function *fptr)
{
	GC_DELREF(ZEND_CLOSURE_OBJECT(fptr));
	return 0;
}
