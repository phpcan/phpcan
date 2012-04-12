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

#define add_to_method_map(METHOD) { \
    if (!zend_hash_exists(Z_ARRVAL_P(router->methods), METHOD, sizeof(METHOD))) {      \
        zval *tmp; MAKE_STD_ZVAL(tmp); array_init(tmp);                                \
        add_assoc_zval(router->methods, METHOD, tmp);                                  \
    }                                                                                  \
    zval **arr;                                                                        \
    if (SUCCESS == zend_hash_find(Z_ARRVAL_P(router->methods),                         \
                METHOD, sizeof(METHOD), (void **)&arr)) {                              \
        if (route->regexp == NULL) {                                                   \
            add_assoc_long(*arr, route->route, numkey);                                \
        } else {                                                                       \
            add_assoc_long(*arr, route->regexp, numkey);                               \
        }                                                                              \
    }                                                                                  \
}

zend_class_entry *ce_buddel_server_router;
static zend_object_handlers server_router_obj_handlers;

static void server_router_dtor(void *object TSRMLS_DC);

static zend_object_value server_router_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_buddel_server_router *r;
    zend_object_value retval;

    r = ecalloc(1, sizeof(*r));
    zend_object_std_init(&r->std, ce TSRMLS_CC);
    r->routes = NULL;
    r->methods = NULL;
    retval.handle = zend_objects_store_put(r,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_router_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_router_obj_handlers;
    return retval;
}

static void server_router_dtor(void *object TSRMLS_DC)
{
    struct php_buddel_server_router *r = (struct php_buddel_server_router*)object;

    if (r->routes) {
        zval_ptr_dtor(&r->routes);
    }

    if (r->methods) {
        zval_ptr_dtor(&r->methods);
    }

    zend_objects_store_del_ref(&r->refhandle TSRMLS_CC);
    zend_object_std_dtor(&r->std TSRMLS_CC);
    efree(r);

}

static int array_key_compare(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f;
	Bucket *s;
	zval result;
	zval first;
	zval second;

	f = *((Bucket **) a);
	s = *((Bucket **) b);

	if (f->nKeyLength == 0) {
		Z_TYPE(first) = IS_LONG;
		Z_LVAL(first) = f->h;
	} else {
		Z_TYPE(first) = IS_STRING;
		Z_STRVAL(first) = (char*)f->arKey;
		Z_STRLEN(first) = f->nKeyLength - 1;
	}

	if (s->nKeyLength == 0) {
		Z_TYPE(second) = IS_LONG;
		Z_LVAL(second) = s->h;
	} else {
		Z_TYPE(second) = IS_STRING;
		Z_STRVAL(second) = (char*)s->arKey;
		Z_STRLEN(second) = s->nKeyLength - 1;
	}

	if (compare_function(&result, &first, &second TSRMLS_CC) == FAILURE) {
		return 0;
	}

	if (Z_TYPE(result) == IS_DOUBLE) {
		if (Z_DVAL(result) < 0) {
			return -1;
		} else if (Z_DVAL(result) > 0) {
			return 1;
		} else {
			return 0;
		}
	}

	convert_to_long(&result);

	if (Z_LVAL(result) < 0) {
		return -1;
	} else if (Z_LVAL(result) > 0) {
		return 1;
	}

	return 0;
}

static int array_reverse_key_compare(const void *a, const void *b TSRMLS_DC)
{
	return array_key_compare(a, b TSRMLS_CC) * -1;
}

static void add_route(struct php_buddel_server_router *router, 
        struct php_buddel_server_route *route, ulong numkey TSRMLS_DC)
{
    if (router->methods == NULL) {
        MAKE_STD_ZVAL(router->methods);
        array_init(router->methods);
    }

    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_GET) {
        add_to_method_map("GET");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_POST) {
        add_to_method_map("POST");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_HEAD) {
        add_to_method_map("HEAD");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_PUT) {
        add_to_method_map("PUT");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_DELETE) {
        add_to_method_map("DELETE");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_OPTIONS) {
        add_to_method_map("OPTIONS");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_TRACE) {
        add_to_method_map("TRACE");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_CONNECT) {
        add_to_method_map("CONNECT");
    }
    if (route->methods & PHP_BUDDEL_SERVER_ROUTE_METHOD_PATCH) {
        add_to_method_map("PATCH");
    }

    zend_hash_sort(Z_ARRVAL_P(router->methods), zend_qsort, array_reverse_key_compare, 0 TSRMLS_CC);
}

/**
 * Constructor
 */
static PHP_METHOD(BuddelServerRouting, __construct)
{
    zval *routes;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "a", &routes)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(array $routes)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_buddel_server_router *router = (struct php_buddel_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    zval **zroute;
    PHP_BUDDEL_FOREACH(routes, zroute) {
        
        if (Z_TYPE_PP(zroute) != IS_OBJECT || Z_OBJCE_PP(zroute) != ce_buddel_server_route) {
            php_buddel_throw_exception(
                ce_buddel_InvalidParametersException TSRMLS_CC,
                "Route must be instance of '%s', '%s' given", 
                    ce_buddel_server_route->name, Z_OBJCE_PP(zroute)->name
            );
            return;
        }
                
        struct php_buddel_server_route *route = (struct php_buddel_server_route*)
                zend_object_store_get_object((*zroute) TSRMLS_CC);
        
        add_route(router, route, numkey TSRMLS_CC);
    }

    zval_add_ref(&routes);
    router->routes = routes;
}

/**
 * Add route
 */
static PHP_METHOD(BuddelServerRouting, addRoute)
{
    zval *zroute;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "O", &zroute, ce_buddel_server_route)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(Route $route)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_buddel_server_router *router = (struct php_buddel_server_router*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    struct php_buddel_server_route *route = (struct php_buddel_server_route*)
        zend_object_store_get_object(zroute TSRMLS_CC);
    
    ulong numkey = (ulong) zend_hash_num_elements(Z_ARRVAL_P(router->routes));
    
    add_route(router, route, numkey TSRMLS_CC);
    
    zval_add_ref(&zroute);
    add_next_index_zval(router->routes, zroute);
    
}

static zend_function_entry server_router_methods[] = {
    PHP_ME(BuddelServerRouting, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRouting, addRoute,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_router_init(TSRMLS_D)
{
    memcpy(&server_router_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_router_obj_handlers.clone_obj = NULL;

    // class \Buddel\Server\Router
    PHP_BUDDEL_REGISTER_CLASS(
        &ce_buddel_server_router,
        ZEND_NS_NAME(PHP_BUDDEL_SERVER_NS, "Router"),
        server_router_ctor,
        server_router_methods
    );
}

PHP_MINIT_FUNCTION(buddel_server_router)
{
    server_router_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(buddel_server_router)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(buddel_server_router)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(buddel_server_router)
{
    return SUCCESS;
}