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

#define add_to_maps(METHOD) { \
    if (!zend_hash_exists(Z_ARRVAL_P(router->method_routes), METHOD, sizeof(METHOD))) {\
        zval *tmp; MAKE_STD_ZVAL(tmp); array_init(tmp);                                \
        add_assoc_zval(router->method_routes, METHOD, tmp);                            \
    }                                                                                  \
    zval **arr;                                                                        \
    if (SUCCESS == zend_hash_find(Z_ARRVAL_P(router->method_routes),                   \
                METHOD, sizeof(METHOD), (void **)&arr)) {                              \
        if (route->regexp == NULL) {                                                   \
            add_assoc_long(*arr, route->route, numkey);                                \
        } else {                                                                       \
            add_assoc_long(*arr, route->regexp, numkey);                               \
        }                                                                              \
    }                                                                                  \
}

zend_class_entry *ce_can_server_router;
static zend_object_handlers server_router_obj_handlers;

static void server_router_dtor(void *object TSRMLS_DC);

static zend_object_value server_router_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_can_server_router *router;
    zend_object_value retval;

    router = ecalloc(1, sizeof(*router));
    zend_object_std_init(&router->std, ce TSRMLS_CC);
    object_properties_init(&router->std, ce);
    router->pos = -1;
    router->routes = NULL;
    router->method_routes = NULL;
    router->route_methods = NULL;
    retval.handle = zend_objects_store_put(router,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_router_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_router_obj_handlers;
    return retval;
}

static void server_router_dtor(void *object TSRMLS_DC)
{
    struct php_can_server_router *router = (struct php_can_server_router*)object;

    if (router->routes) {
        zval_ptr_dtor(&router->routes);
    }

    if (router->method_routes) {
        zval_ptr_dtor(&router->method_routes);
    }
    
    if (router->route_methods) {
        zval_ptr_dtor(&router->route_methods);
    }

    zend_objects_store_del_ref(&router->refhandle TSRMLS_CC);
    zend_object_std_dtor(&router->std TSRMLS_CC);
    efree(router);

}

static void add_route(struct php_can_server_router *router, 
        struct php_can_server_route *route, ulong numkey TSRMLS_DC)
{
    if (router->method_routes == NULL) {
        MAKE_STD_ZVAL(router->method_routes);
        array_init(router->method_routes);
    }
    
    if (router->route_methods == NULL) {
        MAKE_STD_ZVAL(router->route_methods);
        array_init(router->route_methods);
    }
    add_assoc_long(router->route_methods, route->regexp != NULL ? route->regexp : route->route, route->methods);

    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_GET) {
        add_to_maps("GET");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_POST) {
        add_to_maps("POST");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_HEAD) {
        add_to_maps("HEAD");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_PUT) {
        add_to_maps("PUT");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_DELETE) {
        add_to_maps("DELETE");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_OPTIONS) {
        add_to_maps("OPTIONS");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_TRACE) {
        add_to_maps("TRACE");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_CONNECT) {
        add_to_maps("CONNECT");
    }
    if (route->methods & PHP_CAN_SERVER_ROUTE_METHOD_PATCH) {
        add_to_maps("PATCH");
    }
}

/**
 * Constructor
 */
static PHP_METHOD(CanServerRouter, __construct)
{
    zval *routes = NULL;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "|a", &routes)) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(array $routes)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server_router *router = (struct php_can_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (routes != NULL) {
        zval **zroute;
        PHP_CAN_FOREACH(routes, zroute) {
            
            if (Z_TYPE_PP(zroute) != IS_OBJECT || !instanceof_function(Z_OBJCE_PP(zroute), ce_can_server_route TSRMLS_CC)) {
                php_can_throw_exception(
                    ce_can_InvalidParametersException TSRMLS_CC,
                    "Route must be instance of '%s'", ce_can_server_route->name
                );
                return;
            }

            struct php_can_server_route *route = (struct php_can_server_route*)
                    zend_object_store_get_object((*zroute) TSRMLS_CC);

            add_route(router, route, numkey TSRMLS_CC);
        }

        zval_add_ref(&routes);
        router->routes = routes;
    } else {
        MAKE_STD_ZVAL(router->routes);
        array_init(router->routes);
    }
}

/**
 * Add route
 */
static PHP_METHOD(CanServerRouter, addRoute)
{
    zval *zroute;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "O", &zroute, ce_can_server_route)) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(Route $route)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_router *router = (struct php_can_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    struct php_can_server_route *route = (struct php_can_server_route*)
        zend_object_store_get_object(zroute TSRMLS_CC);
    
    ulong numkey = (ulong) zend_hash_num_elements(Z_ARRVAL_P(router->routes));
    
    add_route(router, route, numkey TSRMLS_CC);
    
    zval_add_ref(&zroute);
    add_next_index_zval(router->routes, zroute);
    
}

/**
 * Return the current element
 */
static PHP_METHOD(CanServerRouter, current)
{
    struct php_can_server_router *router = (struct php_can_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    zval **zroute;
    if (FAILURE != zend_hash_index_find(Z_ARRVAL_P(router->routes), router->pos, (void **)&zroute)) {
        RETURN_ZVAL(*zroute, 1, 0);
    }
    RETURN_FALSE;
}

/**
 * Return the key of the current element
 */
static PHP_METHOD(CanServerRouter, key)
{
    struct php_can_server_router *router = (struct php_can_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    RETURN_LONG(router->pos);
}

/**
 * Move forward to next element
 */
static PHP_METHOD(CanServerRouter, next)
{
    struct php_can_server_router *router = (struct php_can_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    router->pos++;
}

/**
 * Rewind the Iterator to the first element
 */
static PHP_METHOD(CanServerRouter, rewind)
{
    struct php_can_server_router *router = (struct php_can_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    router->pos = 0;
}

/**
 * Checks if current position is valid
 */
static PHP_METHOD(CanServerRouter, valid)
{
    struct php_can_server_router *router = (struct php_can_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (router->pos > zend_hash_num_elements(Z_ARRVAL_P(router->routes)) - 1) {
        RETURN_FALSE;
    }    
    RETURN_TRUE;
}

static zend_function_entry server_router_methods[] = {
    PHP_ME(CanServerRouter, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRouter, addRoute,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRouter, current,     NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRouter, key,         NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRouter, next,        NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRouter, rewind,      NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRouter, valid,       NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_router_init(TSRMLS_D)
{
    memcpy(&server_router_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_router_obj_handlers.clone_obj = NULL;

    // class \Can\Server\Router implements Iterator
    PHP_CAN_REGISTER_CLASS(
        &ce_can_server_router,
        ZEND_NS_NAME(PHP_CAN_SERVER_NS, "Router"),
        server_router_ctor,
        server_router_methods
    );
    zend_class_implements(ce_can_server_router TSRMLS_CC, 1, zend_ce_iterator);
}

PHP_MINIT_FUNCTION(can_server_router)
{
    server_router_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(can_server_router)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(can_server_router)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(can_server_router)
{
    return SUCCESS;
}
