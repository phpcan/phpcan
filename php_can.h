/*
  +----------------------------------------------------------------------+
  | PHP Version 5.3                                                      |
  +----------------------------------------------------------------------+
  | Copyright (c) 2002-2011 Dmitri Vinogradov                            |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Dmitri Vinogradov <dmitri.vinogradov@gmail.com>             |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_CAN_H
#define PHP_CAN_H

#include "zend.h"
#include "zend_interfaces.h"
#include "version.h"

#include <evhttp.h>

extern zend_module_entry can_module_entry;
#define can_module_ptr &can_module_entry 
#define phpext_can_ptr can_module_ptr

#define PHP_CAN_NS "Can"
#define PHP_CAN_SERVER_NS ZEND_NS_NAME(PHP_CAN_NS, "Server")
#define PHP_CAN_CLIENT_NS ZEND_NS_NAME(PHP_CAN_NS, "Client")

#if PHP_VERSION_ID < 50399
#define zchar char
#define ZEND_LITERAL_KEY_DC
#define ZEND_LITERAL_KEY_CC
#else
#define zchar const char
#define ZEND_LITERAL_KEY_DC , const zend_literal *key
#define ZEND_LITERAL_KEY_CC , key
#endif

#ifndef CHECK_ZVAL_NULL_PATH
#define CHECK_ZVAL_NULL_PATH(p) (Z_STRLEN_P(p) != strlen(Z_STRVAL_P(p)))
#endif

ZEND_BEGIN_MODULE_GLOBALS(can) 
    struct event_base *can_event_base;
ZEND_END_MODULE_GLOBALS(can)
        
#ifdef ZTS
#define CAN_G(v) TSRMG(can_globals_id, zend_can_globals *, v)
#else
#define CAN_G(v) (can_globals.v)
#endif

PHP_MINIT_FUNCTION(can);
PHP_MSHUTDOWN_FUNCTION(can);
PHP_RINIT_FUNCTION(can);
PHP_RSHUTDOWN_FUNCTION(can);
PHP_MINFO_FUNCTION(can);

int php_can_strpos(char *haystack, char *needle, int offset);
char * php_can_substr(char *str, int f, int l);
char * php_can_method_name(int type);
zval * php_can_strtr_array(char *str, int slen, HashTable *hash);

#ifdef PHP_WIN32
#define PHP_CAN_API __declspec(dllexport)
#else
#define PHP_CAN_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

extern struct event_base *can_event_base;

#define PHP_CAN_FOREACH(array, value)                            \
    char *strkey; ulong numkey; int keytype;                        \
    if (zend_hash_num_elements(Z_ARRVAL_P(array)) > 0)              \
        for (                                                       \
            zend_hash_internal_pointer_reset(Z_ARRVAL_P(array));    \
            zend_hash_get_current_data(                             \
                Z_ARRVAL_P(array), (void **) &value                 \
            ) == SUCCESS &&                                         \
            (keytype = zend_hash_get_current_key(                   \
                Z_ARRVAL_P(array), &strkey, &numkey, 0              \
            )) != HASH_KEY_NON_EXISTANT;                            \
            zend_hash_move_forward(Z_ARRVAL_P(array))               \
        )

// class macros
#define PHP_CAN_REGISTER_CLASS(ppce, class_name, obj_ctor, flist) \
    php_can_register_std_class(ppce, class_name, obj_ctor, flist TSRMLS_CC);

#define PHP_CAN_REGISTER_SUBCLASS(ppce, parent_ce, class_name, obj_ctor, flist) \
    php_can_register_sub_class(ppce, parent_ce, class_name, obj_ctor, flist TSRMLS_CC);

// class constants macros
#define PHP_CAN_REGISTER_CLASS_CONST_STRING(ce, name, value) \
    zend_declare_class_constant_string(ce, name, strlen(name), value TSRMLS_CC);

#define PHP_CAN_REGISTER_CLASS_CONST_LONG(ce, name, value) \
    zend_declare_class_constant_long(ce, name, strlen(name), (long) value TSRMLS_CC);

// class properties macros
#define PHP_CAN_REGISTER_PROPERTY(ce, property, flags) \
    zend_declare_property_null(ce, property, strlen(property), flags TSRMLS_CC);

#define PHP_CAN_REGISTER_PROPERTY_BOOL(ce, property, value, flags) \
    zend_declare_property_bool(ce, property, strlen(property), (long) value, flags TSRMLS_CC);

#define PHP_CAN_REGISTER_PROPERTY_LONG(ce, property, value, flags) \
    zend_declare_property_long(ce, property, strlen(property), (long) value, flags TSRMLS_CC);

#define PHP_CAN_REGISTER_PROPERTY_STRING(ce, property, value, flags) \
    zend_declare_property_string(ce, property, strlen(property), (char *) value, flags TSRMLS_CC);

#define PHP_CAN_REGISTER_PROPERTY_STRINGL(ce, property, value, length, flags) \
    zend_declare_property_stringl(ce, property, strlen(property), (char *) value, length, flags TSRMLS_CC);

#define PHP_CAN_READ_PROPERTY(object, property) \
    zend_read_property(Z_OBJCE_P(object), object, property, strlen(property), 1 TSRMLS_CC)

#define PHP_CAN_UPDATE_PROPERTY(object, property, value) \
    zend_update_property(Z_OBJCE_P(object), object, property, strlen(property), value TSRMLS_CC)

#define PHP_CAN_UPDATE_PROPERTY_STRING(object, property, value) \
    zend_update_property_string(Z_OBJCE_P(object), object, property, strlen(property), value TSRMLS_CC)

#define PHP_CAN_UPDATE_PROPERTY_STRINGL(object, property, value, length) \
    zend_update_property_stringl(Z_OBJCE_P(object), object, property, strlen(property), value, length TSRMLS_CC)

#define PHP_CAN_UPDATE_PROPERTY_BOOL(object, property, value) \
    zend_update_property_bool(Z_OBJCE_P(object), object, property, strlen(property), value TSRMLS_CC)

#define PHP_CAN_UPDATE_PROPERTY_LONG(object, property, value) \
    zend_update_property_long(Z_OBJCE_P(object), object, property, strlen(property), value TSRMLS_CC)

#define PHP_CAN_UPDATE_PROPERTY_DOUBLE(object, property, value) \
    zend_update_property_double(Z_OBJCE_P(object), object, property, strlen(property), value TSRMLS_CC)

#define PHP_CAN_UPDATE_PROPERTY_NULL(object, property) \
    zend_update_property_null(Z_OBJCE_P(object), object, property, strlen(property) TSRMLS_CC)

#define zend_call_method_with_3_params(obj, obj_ce, fn_proxy, function_name, retval, arg1, arg2, arg3) \
    php_can_call_method(obj, obj_ce, fn_proxy, function_name, sizeof(function_name)-1, retval, 3, arg1, arg2, arg3, NULL, NULL TSRMLS_CC)

#define PRINT_ZVAL(zv) \
    php_printf("%s:%d   PRINT_ZVAL %x, refcount=%d, is_ref=%d, value = ", \
        __FILE__, __LINE__, zv, Z_REFCOUNT_P(zv), Z_ISREF_P(zv)); \
    Z_ADDREF_P((zv)); \
    zend_print_zval_r((zv), 0 TSRMLS_CC); \
    Z_DELREF_P((zv));

#endif  /* PHP_CAN_H */
