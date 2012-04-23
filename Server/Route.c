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

#include "Server.h"

zend_class_entry *ce_buddel_server_route;
static zend_object_handlers server_route_obj_handlers;

static void server_route_dtor(void *object TSRMLS_DC);

static zend_object_value server_route_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_buddel_server_route *r;
    zend_object_value retval;

    r = ecalloc(1, sizeof(*r));
    zend_object_std_init(&r->std, ce TSRMLS_CC);
    r->handler = NULL;
    r->methods = 0;
    r->regexp = NULL;
    r->route = NULL;
    r->casts = NULL;
    retval.handle = zend_objects_store_put(r,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_route_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_route_obj_handlers;
    return retval;
}

static void server_route_dtor(void *object TSRMLS_DC)
{
    struct php_buddel_server_route *r = (struct php_buddel_server_route*)object;

    if (r->handler) {
        zval_ptr_dtor(&r->handler);
    }

    if (r->regexp) {
        efree(r->regexp);
        r->regexp = NULL;
    }

    if (r->route) {
        efree(r->route);
        r->route = NULL;
    }
    
    if (r->casts) {
        zval_ptr_dtor(&r->casts);
    }

    zend_objects_store_del_ref(&r->refhandle TSRMLS_CC);
    zend_object_std_dtor(&r->std TSRMLS_CC);
    efree(r);

}

/**
 * Constructor
 */
static PHP_METHOD(BuddelServerRoute, __construct)
{
    char *route;
    zval *handler;
    int route_len;
    long methods = PHP_BUDDEL_SERVER_ROUTE_METHOD_GET;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "sz|l", &route, &route_len, &handler, &methods)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $route, mixed $handler, int $methods)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_buddel_server_route *r = (struct php_buddel_server_route*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    char *func_name;
    zend_bool is_callable = zend_is_callable(handler, 0, &func_name TSRMLS_CC);
    if (!is_callable) {
        php_buddel_throw_exception(
            ce_buddel_InvalidCallbackException TSRMLS_CC,
            "Handler '%s' is not a valid callback",
            func_name
        );
        efree(func_name);
        return;
    }
    efree(func_name);
    
    zval_add_ref(&handler);
    r->handler = handler;
    
    MAKE_STD_ZVAL(r->casts);
    array_init(r->casts);
    
    if (FAILURE != php_buddel_strpos(route, "<", 0) && FAILURE != php_buddel_strpos(route, ">", 0)) {
        int i;
        for (i = 0; i < route_len; i++) {
            if (route[i] != '<') {
                spprintf(&r->regexp, 0, "%s%c", r->regexp == NULL ? "" : r->regexp, route[i]);
            } else {
                int y = php_buddel_strpos(route, ">", i);
                char *name = php_buddel_substr(route, i + 1, y - (i + 1));
                int pos = php_buddel_strpos(name, ":", 0);
                if (FAILURE != pos) {
                    char *var = php_buddel_substr(name, 0, pos);
                    char *filter = php_buddel_substr(name, pos + 1, strlen(name) - (pos + 1));
                    if (strcmp(filter, "int") == 0) {
                        spprintf(&r->regexp, 0, "%s(?<%s>%s)", r->regexp, var, "-?[0-9]+");
                        add_assoc_long(r->casts, var, IS_LONG);
                    } else if (0 == strcmp(filter, "float")) {
                        spprintf(&r->regexp, 0, "%s(?<%s>%s)", r->regexp, var, "-?[0-9.]+");
                        add_assoc_long(r->casts, var, IS_DOUBLE);
                    } else if (0 == strcmp(filter, "path")) {
                        spprintf(&r->regexp, 0, "%s(?<%s>%s)", r->regexp, var, ".+?");
                        add_assoc_long(r->casts, var, IS_PATH);
                    } else if (0 == (pos = php_buddel_strpos(filter, "re:", 0))) {
                        char *reg = php_buddel_substr(filter, pos + 3, strlen(filter) - (pos + 3));
                        spprintf(&r->regexp, 0, "%s(?<%s>%s)", r->regexp, var, reg);
                        efree(reg);
                    }
                    efree(filter);
                    efree(var);
                    
                } else {
                    spprintf(&r->regexp, 0, "%s(?<%s>[^/]+)", r->regexp, name);
                }
                efree(name);
                i = y;
            }
        }
        spprintf(&r->regexp, 0, "\1^%s$\1", r->regexp);
    }
    
    r->route = estrndup(route, route_len);
    
    if (methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_ALL) {
        r->methods = methods;
    } else {
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "Unexpected methods",
            func_name
        );
    }

}

/**
 * Get route
 */
static PHP_METHOD(BuddelServerRoute, getRoute)
{
    zend_bool as_regexp = 0;
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "|b", &as_regexp)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s([bool $as_regexp])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_buddel_server_route *r = (struct php_buddel_server_route*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    RETURN_STRING(as_regexp ? r->regexp : r->route, 1);
}


/**
 * Get HTTP method this route applies to
 */
static PHP_METHOD(BuddelServerRoute, getMethod)
{
    zend_bool as_regexp = 0;
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "|b", &as_regexp)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s([bool $as_regexp])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_buddel_server_route *r = (struct php_buddel_server_route*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (as_regexp) {
        char *retval = NULL;
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_GET) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "GET");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_POST) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "POST");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_HEAD) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "HEAD");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_PUT) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "PUT");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_DELETE) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "DELETE");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_OPTIONS) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "OPTIONS");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_TRACE) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "TRACE");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_CONNECT) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "CONNECT");
        }
        if (r->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_PATCH) {
            spprintf(&retval, 0, "%s%s", retval == NULL ? "" : "|", "PATCH");
        }
        int len = spprintf(&retval, 0, "(%s)", retval);
        RETVAL_STRINGL(retval, len, 0);
    } else {
        RETVAL_LONG(r->methods);
    }
  
}

/**
 * Get request handler associated with this route
 */
static PHP_METHOD(BuddelServerRoute, getHandler)
{
    struct php_buddel_server_route *r = (struct php_buddel_server_route*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    RETURN_ZVAL(r->handler, 1, 0);
}

static zend_function_entry server_route_methods[] = {
    PHP_ME(BuddelServerRoute, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRoute, getRoute,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRoute, getMethod,   NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRoute, getHandler,  NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_route_init(TSRMLS_D)
{
    memcpy(&server_route_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_route_obj_handlers.clone_obj = NULL;

    // class \Buddel\Server\Route
    PHP_BUDDEL_REGISTER_CLASS(
        &ce_buddel_server_route,
        ZEND_NS_NAME(PHP_BUDDEL_SERVER_NS, "Route"),
        server_route_ctor,
        server_route_methods
    );

    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_GET",     PHP_BUDDEL_SERVER_ROUTE_METHOD_GET);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_POST",    PHP_BUDDEL_SERVER_ROUTE_METHOD_POST);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_HEAD",    PHP_BUDDEL_SERVER_ROUTE_METHOD_HEAD);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_PUT",     PHP_BUDDEL_SERVER_ROUTE_METHOD_PUT);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_DELETE",  PHP_BUDDEL_SERVER_ROUTE_METHOD_DELETE);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_OPTIONS", PHP_BUDDEL_SERVER_ROUTE_METHOD_OPTIONS);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_TRACE",   PHP_BUDDEL_SERVER_ROUTE_METHOD_TRACE);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_CONNECT", PHP_BUDDEL_SERVER_ROUTE_METHOD_CONNECT);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_route, "METHOD_PATCH",   PHP_BUDDEL_SERVER_ROUTE_METHOD_PATCH);
}

PHP_MINIT_FUNCTION(buddel_server_route)
{
    server_route_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(buddel_server_route)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(buddel_server_route)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(buddel_server_route)
{
    return SUCCESS;
}
