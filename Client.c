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

#include "Client.h"

#include <event.h>
#include <evhttp.h>

zend_class_entry *ce_can_client;
static zend_object_handlers client_obj_handlers;

static struct event_base *event_base = NULL;

static void client_dtor(void *object TSRMLS_DC);

static zend_object_value client_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_can_client *client;
    zend_object_value retval;

    client = ecalloc(1, sizeof(*client));
    client->base = NULL;
    client->evcon = NULL;
    client->status = 0;
    client->method = EVHTTP_REQ_GET;
    client->uri = NULL;
    client->handler = NULL;
    client->headers = NULL;
    zend_object_std_init(&client->std, ce TSRMLS_CC);

    retval.handle = zend_objects_store_put(client,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            client_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &client_obj_handlers;
    return retval;
}

static void client_dtor(void *object TSRMLS_DC)
{
    struct php_can_client *client = (struct php_can_client*)object;
    
    if (client->base && client->base != can_event_base) {
        event_base_free(client->base);
        client->base = NULL;
    }
    
    if (client->evcon) {
        evhttp_connection_free(client->evcon);
        client->evcon = NULL;
    }
    
    if (client->uri) {
        evhttp_uri_free(client->uri);
        client->uri = NULL;
    }
    
    if (client->handler) {
        zval_ptr_dtor(&client->handler);
        client->handler = NULL;
    }
    
    if (client->headers) {
        zval_ptr_dtor(&client->headers);
        client->headers = NULL;
    }
    
    zend_objects_store_del_ref(&client->refhandle TSRMLS_CC);
    zend_object_std_dtor(&client->std TSRMLS_CC);
    efree(client);
}

static void response_handler(struct evhttp_request *req, void *arg)
{
    TSRMLS_FETCH();
    
    zval *zresponse, *args[1], retval;
    struct php_can_client *client = (struct php_can_client*)arg;

    if (client->base != can_event_base) {
        event_base_loopexit(client->base, 0);
    }
    
    if (!client || !client->handler) {
        return;
    }
    
    // create response object
    MAKE_STD_ZVAL(zresponse);
    object_init_ex(zresponse, ce_can_client_response);
    Z_SET_REFCOUNT_P(zresponse, 1);
    struct php_can_client_response *response = (struct php_can_client_response *)
        zend_object_store_get_object(zresponse TSRMLS_CC);
    if (req) {
        response->req = req;
    }
    
    args[0] = zresponse;
    Z_ADDREF_P(args[0]);

    if (call_user_function(EG(function_table), NULL, client->handler, &retval, 1, args TSRMLS_CC) == SUCCESS) {
        zval_dtor(&retval);
    }
    Z_DELREF_P(args[0]);
    
    zval_ptr_dtor(&zresponse);
}

/**
 * 
 * Constructor.
 */
static PHP_METHOD(CanClient, __construct)
{
    zval *url, *method = NULL, *headers = NULL;
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z|za", &url, &method, &headers)
        || Z_TYPE_P(url) != IS_STRING
        || Z_STRLEN_P(url) == 0
        || (method && (Z_TYPE_P(method) != IS_LONG || Z_LVAL_P(method) < 1))
        || (headers && Z_TYPE_P(headers) != IS_ARRAY)
    ) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $url[, integer $method = %s::METHOD_GET[, array $options = array()]])",
            class_name, space, get_active_function_name(TSRMLS_C), class_name
        );
        return;
    }
    
    struct php_can_client *client = (struct php_can_client*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    client->uri = evhttp_uri_parse(Z_STRVAL_P(url));
    if (!client->uri) {
	php_can_throw_exception(
            ce_can_RuntimeException TSRMLS_CC,
            "Cannot parse url %s", Z_STRVAL_P(url)
        );
        return;
    }
    
    if (method != NULL) {
        client->method = Z_LVAL_P(method);
    }
    
    if (headers != NULL) {
        zval **item;
        PHP_CAN_FOREACH(headers, item) {
            if (keytype != HASH_KEY_IS_STRING || Z_TYPE_PP(item) != IS_STRING) {
                php_can_throw_exception(
                    ce_can_InvalidParametersException TSRMLS_CC,
                    "Header key and value must be a strings"
                );
                return;
            }
        }
        zval_add_ref(&headers);
        client->headers = headers;
    } else {
        MAKE_STD_ZVAL(client->headers);
        array_init(client->headers);
    }
    
}

/**
 * Send request to the server.
 */
static PHP_METHOD(CanClient, send)
{
    zval *handler;
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
        "z", &handler)
    ) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(callback $handler)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    char *func_name;
    zend_bool is_callable = zend_is_callable(handler, 0, &func_name TSRMLS_CC);
    if (!is_callable) {
        php_can_throw_exception(
            ce_can_InvalidCallbackException TSRMLS_CC,
            "Handler '%s' is not a valid callback",
            func_name
        );
        efree(func_name);
        return;
    }
    efree(func_name);
    
    struct php_can_client *client = (struct php_can_client*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    zval_add_ref(&handler);
    client->handler = handler;

    const char *host = evhttp_uri_get_host(client->uri);
    const char *path = evhttp_uri_get_path(client->uri);
    const char *query = evhttp_uri_get_query(client->uri);
    int port = evhttp_uri_get_port(client->uri);
    port = port == -1 ? 80 : port;
    
    client->base = can_event_base != NULL ? 
        can_event_base : event_base_new();

    client->evcon = evhttp_connection_base_new(client->base, NULL, host, port);

    struct evhttp_request *req = evhttp_request_new(response_handler, client);
    
    zval **item;
    PHP_CAN_FOREACH(client->headers, item) {
        evhttp_add_header(req->output_headers, (const char*)strkey, (const char*)Z_STRVAL_PP(item));
    }
    
    if (NULL == evhttp_find_header(req->output_headers, "Host")) {
        evhttp_add_header(req->output_headers, "Host", host);
    }

    char *uri = NULL;
    spprintf(&uri, 0, "%s%s", path, query ? query : "");
    evhttp_make_request(client->evcon, req, client->method, (const char*)uri);
    efree(uri);

    if (client->base != can_event_base) {
        event_base_dispatch(client->base);
    }
}


static zend_function_entry client_methods[] = {
    PHP_ME(CanClient, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanClient, send,        NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void client_init(TSRMLS_D)
{
    memcpy(&client_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    client_obj_handlers.clone_obj = NULL;

    // class \Can\Client
    PHP_CAN_REGISTER_CLASS(
        &ce_can_client,
        ZEND_NS_NAME(PHP_CAN_NS, "Client"),
        client_ctor,
        client_methods
    );
    
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_GET",     EVHTTP_REQ_GET);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_POST",    EVHTTP_REQ_POST);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_HEAD",    EVHTTP_REQ_HEAD);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_PUT",     EVHTTP_REQ_PUT);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_DELETE",  EVHTTP_REQ_DELETE);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_OPTIONS", EVHTTP_REQ_OPTIONS);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_TRACE",   EVHTTP_REQ_TRACE);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_CONNECT", EVHTTP_REQ_CONNECT);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_client, "METHOD_PATCH",   EVHTTP_REQ_PATCH);
}

PHP_MINIT_FUNCTION(can_client)
{
    client_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(can_client)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(can_client)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(can_client)
{
    return SUCCESS;
}
