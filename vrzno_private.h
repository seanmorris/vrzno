/* Internal declarations shared by the Vrzno translation units. */

#ifndef VRZNO_PRIVATE_H
#define VRZNO_PRIVATE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <emscripten.h>

#include "php.h"
#include "php_ini.h"
#include "SAPI.h"

#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "../json/php_json.h"
#include "../json/php_json_encoder.h"
#include "../json/php_json_parser.h"
#include "ext/spl/spl_exceptions.h"

#include "zend_API.h"
#include "zend_alloc.h"
#include "zend_closures.h"
#include "zend_errors.h"
#include "zend_exceptions.h"
#include "zend_hash.h"
#include "zend_interfaces.h"
#include "zend_types.h"

#if PHP_MAJOR_VERSION >= 8
# include "zend_attributes.h"
#else
# include <stdbool.h>
#endif

#include "php_vrzno.h"

#if UINTPTR_MAX != UINT32_MAX
# error "Vrzno requires Emscripten's wasm32 memory model"
#endif

/* For compatibility with older PHP versions. */
#ifndef ZEND_PARSE_PARAMETERS_NONE
# define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

extern zend_class_entry *vrzno_class_entry;
extern zend_object_handlers vrzno_object_handlers;
extern const php_stream_wrapper php_stream_fetch_wrapper;

zend_object *vrzno_create_object(zend_class_entry *class_type);
vrzno_object *vrzno_create_object_for_target(
	vrzno_target_id target_id,
	bool is_constructor
);
void vrzno_object_free(zend_object *object);

zval *vrzno_read_property(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv);
zval *vrzno_write_property(zend_object *object, zend_string *member, zval *new_value, void **cache_slot);
zval *vrzno_read_dimension(zend_object *object, zval *offset, int type, zval *rv);
void vrzno_write_dimension(zend_object *object, zval *offset, zval *new_value);
int vrzno_has_dimension(zend_object *object, zval *offset, int check_empty);
void vrzno_unset_property(zend_object *object, zend_string *member, void **cache_slot);
void vrzno_unset_dimension(zend_object *object, zval *offset);
HashTable *vrzno_get_properties_for(zend_object *object, zend_prop_purpose purpose);
int vrzno_has_property(zend_object *object, zend_string *member, int has_set_exists, void **cache_slot);

zend_object_iterator *vrzno_array_get_iterator(zend_class_entry *ce, zval *zv, int by_ref);

zval *vrzno_expose_copy_zval(zval *source);
void vrzno_expose_destroy_zval(zval *zv);

zval *vrzno_exec_callback(zend_function *function, zval *argv, int argc, zend_object *object);
zval *vrzno_exec_zval_callback(zval *callback, zval *argv, int argc);

#endif
