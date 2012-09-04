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

#ifndef HAVE_TAILQFOREACH
#include <sys/queue.h>
#endif

#include <event.h>
#include <evhttp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

zend_class_entry *ce_can_server_request;
static zend_object_handlers server_request_obj_handlers;
static HashTable *mimetypes = NULL;
static int has_finfo = -1;
static zend_class_entry **finfo_cep = NULL;
static const char *default_mimetype = "text/plain";

static void server_request_dtor(void *object TSRMLS_DC);

static zend_object_value server_request_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_can_server_request *request;
    zend_object_value retval;

    request = ecalloc(1, sizeof(*request));
    zend_object_std_init(&request->std, ce TSRMLS_CC);
    PHP_CAN_INIT_OBJ_PROPS(request, ce);
    request->cookies = NULL;
    request->get = NULL;
    request->post = NULL;
    request->files = NULL;
    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_NONE;
    request->uri = NULL;
    request->query = NULL;
    request->response_code = 0;
    request->response_len = 0;
    request->error = NULL;
    retval.handle = zend_objects_store_put(request,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_request_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_request_obj_handlers;
    return retval;
}

static void server_request_dtor(void *object TSRMLS_DC)
{
    struct php_can_server_request *request = (struct php_can_server_request*)object;

    if (request->req) {
        request->req = NULL;
    }

    if (request->cookies) {
        zval_ptr_dtor(&request->cookies);
    }

    if (request->get) {
        zval_ptr_dtor(&request->get);
    }

    if (request->post) {
        zval_ptr_dtor(&request->post);
    }

    if (request->files) {
        zval_ptr_dtor(&request->files);
    }

    if (request->uri) {
        efree(request->uri);
        request->uri = NULL;
    }

    if (request->query) {
        efree(request->query);
        request->query = NULL;
    }
    
    if (request->error) {
        efree(request->error);
        request->error = NULL;
    }

    zend_objects_store_del_ref(&request->refhandle TSRMLS_CC);
    zend_object_std_dtor(&request->std TSRMLS_CC);
    efree(request);
}

static zval *read_property(zval *object, zval *member, int type ZEND_LITERAL_KEY_DC TSRMLS_DC)
{
    struct php_can_server_request *request;
    zval tmp_member;
    zval *retval;
    zend_object_handlers *std_hnd;
    struct evkeyval *header;
    char * str;

    request = (struct php_can_server_request*)zend_object_store_get_object(object TSRMLS_CC);

    if (member->type != IS_STRING) {
        tmp_member = *member;
        zval_copy_ctor(&tmp_member);
        convert_to_string(&tmp_member);
        member = &tmp_member;
    }

    if (Z_STRLEN_P(member) == (sizeof("method") - 1)
            && !memcmp(Z_STRVAL_P(member), "method", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_STRING(retval, php_can_method_name(request->req->type), 1);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("uri") - 1)
            && !memcmp(Z_STRVAL_P(member), "uri", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_STRING(retval, request->uri != NULL ? request->uri : "", 1);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("query") - 1)
            && !memcmp(Z_STRVAL_P(member), "query", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_STRING(retval, request->query != NULL ? request->query : "", 1);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("protocol") - 1)
            && !memcmp(Z_STRVAL_P(member), "protocol", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        spprintf(&str, 0, "HTTP/%d.%d", request->req->major, request->req->minor);
        ZVAL_STRING(retval, str, 1);
        Z_SET_REFCOUNT_P(retval, 0);
        efree(str);

    } else if (Z_STRLEN_P(member) == (sizeof("remoteAddr") - 1)
            && !memcmp(Z_STRVAL_P(member), "remoteAddr", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        if (request->req->remote_host) {
            ZVAL_STRING(retval, request->req->remote_host, 1);
        } else {
            ZVAL_STRING(retval, "", 1);
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("remotePort") - 1)
            && !memcmp(Z_STRVAL_P(member), "remotePort", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_LONG(retval, (int)request->req->remote_port);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("requestHeaders") - 1)
            && !memcmp(Z_STRVAL_P(member), "requestHeaders", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        for (header = ((request->req->input_headers)->tqh_first);
             header; 
             header = ((header)->next.tqe_next)
        ) {
            add_assoc_string(retval, header->key, header->value, 1);
        }
        Z_SET_REFCOUNT_P(retval, 0);
        
    } else if (Z_STRLEN_P(member) == (sizeof("responseHeaders") - 1)
            && !memcmp(Z_STRVAL_P(member), "responseHeaders", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        for (header = ((request->req->output_headers)->tqh_first);
             header; 
             header = ((header)->next.tqe_next)
        ) {
            add_assoc_string(retval, header->key, header->value, 1);
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("cookies") - 1)
            && !memcmp(Z_STRVAL_P(member), "cookies", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (request->cookies) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(request->cookies),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("get") - 1)
            && !memcmp(Z_STRVAL_P(member), "get", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (request->get) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(request->get),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("post") - 1)
            && !memcmp(Z_STRVAL_P(member), "post", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (request->post) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(request->post),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("files") - 1)
            && !memcmp(Z_STRVAL_P(member), "files", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (request->files) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(request->files),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("status") - 1)
            && !memcmp(Z_STRVAL_P(member), "status", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_LONG(retval, request->status);
        Z_SET_REFCOUNT_P(retval, 0);
        
    } else if (Z_STRLEN_P(member) == (sizeof("time") - 1)
            && !memcmp(Z_STRVAL_P(member), "time", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_DOUBLE(retval, request->time);
        Z_SET_REFCOUNT_P(retval, 0);
        
    } else if (Z_STRLEN_P(member) == (sizeof("responseCode") - 1)
            && !memcmp(Z_STRVAL_P(member), "responseCode", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_LONG(retval, request->response_code);
        Z_SET_REFCOUNT_P(retval, 0);
        
    } else if (Z_STRLEN_P(member) == (sizeof("responseLength") - 1)
            && !memcmp(Z_STRVAL_P(member), "responseLength", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_LONG(retval, request->response_len);
        Z_SET_REFCOUNT_P(retval, 0);


    } else {
        std_hnd = zend_get_std_object_handlers();
        retval = std_hnd->read_property(object, member, type ZEND_LITERAL_KEY_CC TSRMLS_CC);
    }

    if (member == &tmp_member) {
        zval_dtor(member);
    }

    return retval;
}

static HashTable *get_properties(zval *object TSRMLS_DC) /* {{{ */
{

    HashTable *props;
    zval *zv;
    char *str;
    struct evkeyval *header;
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_objects_get_address(object TSRMLS_CC);
    
    
    props = zend_std_get_properties(object TSRMLS_CC);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, php_can_method_name(request->req->type), 1);
    zend_hash_update(props, "method", sizeof("method"), &zv, sizeof(zval), NULL);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, request->uri != NULL ? request->uri : "", 1);
    zend_hash_update(props, "uri", sizeof("uri"), &zv, sizeof(zval), NULL);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, request->query != NULL ? request->query : "", 1);
    zend_hash_update(props, "query", sizeof("query"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    spprintf(&str, 0, "HTTP/%d.%d", request->req->major, request->req->minor);
    ZVAL_STRING(zv, str, 1);
    zend_hash_update(props, "protocol", sizeof("protocol"), &zv, sizeof(zval), NULL);
    efree(str);

    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, request->req->remote_host ? request->req->remote_host : "", 1);
    zend_hash_update(props, "remoteAddr", sizeof("remoteAddr"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    ZVAL_LONG(zv, (int)request->req->remote_port);
    zend_hash_update(props, "remotePort", sizeof("remotePort"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    for (header = ((request->req->input_headers)->tqh_first);
         header; 
         header = ((header)->next.tqe_next)
    ) {
        add_assoc_string(zv, header->key, header->value, 1);
    }
    zend_hash_update(props, "requestHeaders", sizeof("requestHeaders"), &zv, sizeof(zval), NULL);
    
    MAKE_STD_ZVAL(zv);
    array_init(zv);
    for (header = ((request->req->output_headers)->tqh_first);
         header; 
         header = ((header)->next.tqe_next)
    ) {
        add_assoc_string(zv, header->key, header->value, 1);
    }
    zend_hash_update(props, "responseHeaders", sizeof("responseHeaders"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (request->cookies) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(request->cookies),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "cookies", sizeof("cookies"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (request->get) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(request->get),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "get", sizeof("get"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (request->post) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(request->post),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "post", sizeof("post"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (request->files) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(request->files),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "files", sizeof("files"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    ZVAL_LONG(zv, (int)request->status);
    zend_hash_update(props, "status", sizeof("status"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    ZVAL_DOUBLE(zv, request->time);
    zend_hash_update(props, "time", sizeof("time"), &zv, sizeof(zval), NULL);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_LONG(zv, request->response_code);
    zend_hash_update(props, "responseCode", sizeof("responseCode"), &zv, sizeof(zval), NULL);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_LONG(zv, request->response_len);
    zend_hash_update(props, "responseLength", sizeof("responseLength"), &zv, sizeof(zval), NULL);
    
    return props;
}

void write_property(zval *object, zval *member, zval *value ZEND_LITERAL_KEY_DC TSRMLS_DC)
{
    zval tmp_member;
    if (member->type != IS_STRING) {
        tmp_member = *member;
        zval_copy_ctor(&tmp_member);
        convert_to_string(&tmp_member);
        member = &tmp_member;
    }

    php_can_throw_exception(
        ce_can_InvalidOperationException TSRMLS_CC, "Cannot update readonly property %s::$%s", 
            ZEND_NS_NAME(PHP_CAN_SERVER_NS, "Request"), Z_STRVAL_P(member)
    );
    
    if (member == &tmp_member) {
        zval_dtor(member);
    }
}

/**
 * Constructor
 */
static PHP_METHOD(CanServerRequest, __construct)
{
    /* final protected */
}

/**
 * Find request header
 */
static PHP_METHOD(CanServerRequest, findRequestHeader)
{
    zval *header = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z", &header) || Z_TYPE_P(header) != IS_STRING || Z_STRLEN_P(header) == 0) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    const char *value = evhttp_find_header(request->req->input_headers, (const char*)Z_STRVAL_P(header));
    if (value == NULL) {
        RETURN_FALSE;
    }
    RETURN_STRING(value, 1);
}

/**
 * Find response header
 */
static PHP_METHOD(CanServerRequest, findResponseHeader)
{
    zval *header = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z", &header) || Z_TYPE_P(header) != IS_STRING || Z_STRLEN_P(header) == 0) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    const char *value = evhttp_find_header(request->req->output_headers, (const char*)Z_STRVAL_P(header));
    if (value == NULL) {
        RETURN_FALSE;
    }
    RETURN_STRING(value, 1);
}

/**
 * Get raw request data
 */
static PHP_METHOD(CanServerRequest, getRequestBody)
{
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    int buffer_len = EVBUFFER_LENGTH(request->req->input_buffer);
    if (buffer_len > 0) {
        RETURN_STRINGL(EVBUFFER_DATA(request->req->input_buffer), buffer_len, 1);
    }
    RETURN_FALSE;
}

/**
 * Get response body
 */
static PHP_METHOD(CanServerRequest, getResponseBody)
{
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    int buffer_len = EVBUFFER_LENGTH(request->req->output_buffer);
    if (buffer_len > 0) {
        RETURN_STRINGL(EVBUFFER_DATA(request->req->output_buffer), buffer_len, 1);
    }
    RETURN_FALSE;
}

/**
 * Set response body
 */
static PHP_METHOD(CanServerRequest, setResponseBody)
{
    zval *body = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z", &body) || Z_TYPE_P(body) != IS_STRING) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $body)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    evbuffer_free(request->req->output_buffer);
    request->req->output_buffer = evbuffer_new();
    evbuffer_add(request->req->output_buffer, Z_STRVAL_P(body), Z_STRLEN_P(body));
}

/**
 * Add response header
 */
static PHP_METHOD(CanServerRequest, addResponseHeader)
{
    zval *header, *value;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "zz", &header, &value) 
        || Z_TYPE_P(header) != IS_STRING
        || Z_STRLEN_P(header) == 0
        || Z_TYPE_P(value) != IS_STRING
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header, string $value)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (evhttp_add_header(request->req->output_headers, Z_STRVAL_P(header), Z_STRVAL_P(value)) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

/**
 * Remove response header
 */
static PHP_METHOD(CanServerRequest, removeResponseHeader)
{
    zval *header, *value = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z|z", &header, &value) 
        || Z_TYPE_P(header) != IS_STRING
        || Z_STRLEN_P(header) == 0
        || (value != NULL && (Z_TYPE_P(value) != IS_STRING || Z_STRLEN_P(value) == 0))
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header[, string $value])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (value != NULL) {
        char *existing_value = (char *)evhttp_find_header(request->req->output_headers, Z_STRVAL_P(header));
        if (NULL != existing_value && 0 == strcmp(Z_STRVAL_P(value), existing_value)
           && 0 == evhttp_remove_header(request->req->output_headers, Z_STRVAL_P(header))
        ) {
            RETURN_TRUE;
        }
        RETURN_FALSE;
    } 

    if (evhttp_remove_header(request->req->output_headers, Z_STRVAL_P(header)) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

/**
 * Get response headers
 */
static PHP_METHOD(CanServerRequest, getResponseHeaders)
{
    struct evkeyval *header;
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    array_init(return_value);
    
    for (header = ((request->req->output_headers)->tqh_first);
         header; 
         header = ((header)->next.tqe_next)
    ) {
        add_assoc_string(return_value, header->key, header->value, 1);
    }
}

/**
 * Set response status
 */
static PHP_METHOD(CanServerRequest, setResponseStatus)
{
    zval *status;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z", &status) || Z_TYPE_P(status) != IS_LONG) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(int $status)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    if (Z_LVAL_P(status) < 100 || Z_LVAL_P(status) > 599) {
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Unexpected HTTP status, expecting range is 100-599"
        );
        return;
    }

    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    request->response_code = Z_LVAL_P(status);
}

/**
 * Redirect client to new location
 */
static PHP_METHOD(CanServerRequest, redirect)
{
    zval *location, *status = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z|z", &location, &status) 
        || Z_TYPE_P(location) != IS_STRING
        || Z_STRLEN_P(location) == 0 
        || (status && (Z_TYPE_P(status) != IS_LONG || Z_LVAL_P(status) < 300 || Z_LVAL_P(status) > 399))
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $location[, int $status = 302])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (evhttp_add_header(request->req->output_headers, "Location", Z_STRVAL_P(location)) != 0) {
        RETURN_FALSE;
    }
    request->response_code = status ? Z_LVAL_P(status) : 302;
    RETURN_TRUE;
}

/**
 * Set cookie
 */
static PHP_METHOD(CanServerRequest, setCookie)
{
    zval *name, *value, 
         *expires = NULL, *path = NULL, *domain = NULL, 
         *secure = NULL, *httponly = NULL, *url_encode = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "zz|zzzzzz", &name, &value, &expires, &path, &domain, &secure, &httponly, &url_encode) 
        || Z_TYPE_P(name) != IS_STRING
        || Z_STRLEN_P(name) == 0
        || Z_TYPE_P(value) != IS_STRING
        || (expires && (Z_TYPE_P(expires) != IS_LONG || Z_LVAL_P(expires) < 0))
        || (path && Z_TYPE_P(path) != IS_STRING)
        || (domain && Z_TYPE_P(domain) != IS_STRING)
        || (secure && Z_TYPE_P(secure) != IS_BOOL)
        || (httponly && Z_TYPE_P(httponly) != IS_BOOL)
        || (url_encode && Z_TYPE_P(url_encode) != IS_BOOL)
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $name [, string $value [, int $expire = 0 [, string $path "
            "[, string $domain [, bool $secure = false [, bool $httponly = false [, bool $url_encode = false]]]]]]])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    char *cookie, *encoded_value = NULL;
    int len = Z_STRLEN_P(name);
    char *dt;

    if (strpbrk(Z_STRVAL_P(name), "=,; \t\r\n\013\014") != NULL) {   /* man isspace for \013 and \014 */
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Cookie names cannot contain any of the following characters '=,; \\t\\r\\n\\013\\014'"
        );
        return;
    }

    if ((!url_encode || Z_BVAL_P(url_encode) == 0) 
            && Z_STRLEN_P(value) > 0 && strpbrk(Z_STRVAL_P(value), ",; \t\r\n\013\014") != NULL) { /* man isspace for \013 and \014 */
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Cookie values cannot contain any of the following characters '=,; \\t\\r\\n\\013\\014'"
        );
        return;
    }

    if (Z_STRLEN_P(value) > 0 && url_encode && Z_BVAL_P(url_encode)) {
        int encoded_value_len;
        encoded_value = php_url_encode(Z_STRVAL_P(value), Z_STRLEN_P(value), &encoded_value_len);
        len += encoded_value_len;
    } else if ( Z_STRLEN_P(value) > 0 ) {
        encoded_value = estrndup(Z_STRVAL_P(value), Z_STRLEN_P(value));
        len += Z_STRLEN_P(value);
    }
    if (path) {
        len += Z_STRLEN_P(path);
    }
    if (domain) {
        len += Z_STRLEN_P(domain);
    }

    cookie = emalloc(len + 100);

    if (Z_STRLEN_P(value) == 0) {
        /* 
         * MSIE doesn't delete a cookie when you set it to a null value
         * so in order to force cookies to be deleted, even on MSIE, we
         * pick an expiry date in the past
         */
        dt = php_format_date("D, d-M-Y H:i:s T", sizeof("D, d-M-Y H:i:s T")-1, 1, 0 TSRMLS_CC);
        snprintf(cookie, len + 100, "%s=deleted; expires=%s", Z_STRVAL_P(name), dt);
        efree(dt);
    } else {
        snprintf(cookie, len + 100, "%s=%s", Z_STRVAL_P(name), Z_STRLEN_P(value) > 0 ? encoded_value : "");
        if (expires && Z_LVAL_P(expires) > 0) {
            const char *p;
            strlcat(cookie, "; expires=", len + 100);
            dt = php_format_date("D, d-M-Y H:i:s T", sizeof("D, d-M-Y H:i:s T")-1, Z_LVAL_P(expires), 0 TSRMLS_CC);
            /* check to make sure that the year does not exceed 4 digits in length */
            p = zend_memrchr(dt, '-', strlen(dt));
            if (!p || *(p + 5) != ' ') {
                efree(dt);
                efree(cookie);
                efree(encoded_value);
                php_can_throw_exception(
                    ce_can_InvalidParametersException TSRMLS_CC,
                    "Expiry date cannot have a year greater then 9999"
                );
                return;
            }
            strlcat(cookie, dt, len + 100);
            efree(dt);
        }
    }
    
    if (encoded_value) {
        efree(encoded_value);
    }

    if (path && Z_STRLEN_P(path) > 0) {
        strlcat(cookie, "; path=", len + 100);
        strlcat(cookie, Z_STRVAL_P(path), len + 100);
    }
    if (domain && Z_STRLEN_P(domain) > 0) {
        strlcat(cookie, "; domain=", len + 100);
        strlcat(cookie, Z_STRVAL_P(domain), len + 100);
    }
    if (secure && Z_BVAL_P(secure)) {
        strlcat(cookie, "; secure", len + 100);
    }
    if (httponly && Z_BVAL_P(httponly)) {
        strlcat(cookie, "; httponly", len + 100);
    }
    
    if (evhttp_add_header(request->req->output_headers, "Set-Cookie", cookie) != 0) {
        RETVAL_FALSE;
    } else {
        RETVAL_TRUE;
    }
    efree(cookie);
}

/**
 * Get realpath of the given path
 */
static char *get_realpath(char *path, int check_is_readable TSRMLS_DC)
{
    char resolved_path_buff[MAXPATHLEN];
    if (VCWD_REALPATH(path, resolved_path_buff)) {
        if (php_check_open_basedir(resolved_path_buff TSRMLS_CC)) {
            return NULL;
        }
                
#ifdef R_OK
        if (check_is_readable == 1 && VCWD_ACCESS(resolved_path_buff, R_OK)) {
            return NULL;
        }
#endif
        return estrdup(resolved_path_buff);
    }
    return NULL;
}

/**
 * Send file
 */
static PHP_METHOD(CanServerRequest, sendFile)
{
    char *filename, *root, *mimetype;
    int filename_len, root_len = 0, *mimetype_len = 0;
    zval *download = NULL;
    long chunksize = 8192; // default chunksize 8 kB
    
#if PHP_VERSION_ID < 50399
#define TYPE_SPEC "s|sszl"
#define CAN_CHECK_NULL_PATH(p, l) (l > 0 && strlen(p) != l)
#else
#define TYPE_SPEC "p|pszl"
#define CAN_CHECK_NULL_PATH(p, l) (0)
#endif
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            TYPE_SPEC, &filename, &filename_len, &root, &root_len, 
                     &mimetype, &mimetype_len, &download, &chunksize)
        || filename_len == 0
        || CAN_CHECK_NULL_PATH(filename, filename_len)
        || CAN_CHECK_NULL_PATH(root, root_len)
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    // do not serve requests for files begins with ``/..`` or ``../``
    if (0 == php_can_strpos(filename, "/..", 0) || 0 == php_can_strpos(filename, "../", 0)) {
        php_can_throw_exception_code(
            ce_can_HTTPError TSRMLS_CC, 400, "Bogus file requested '%s'", filename
        );
        return;
    }
    
    // try to determine real path of the given root
    char *rootpath = NULL;
    if (root_len > 0) {
        rootpath = get_realpath(root, 1 TSRMLS_CC);
        if (rootpath == NULL) {
            zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
            php_can_throw_exception(
                ce_can_InvalidParametersException TSRMLS_CC,
                "%s%s%s(): Cannot determine real path of $root value '%s'",
                class_name, space, get_active_function_name(TSRMLS_C),
                root
            );
            return;
        }
    }
    
    // try to determine real path of the root+file
    char *tmppath = NULL;
    spprintf(&tmppath, 0, "%s%s%s", rootpath ? rootpath : "", (rootpath && filename[0] != '/' ? "/" : ""), filename);

    // resolve realpath of the requested file
    char *filepath = get_realpath(tmppath, 0 TSRMLS_CC);
    if (filepath == NULL) {
        php_can_throw_exception_code(
            ce_can_HTTPError TSRMLS_CC, 404, "Requested file '%s%s%s' does not exist", 
                root_len > 0 ? root : "", filename[0] != '/' ? "/" : "", filename
        );
        efree(tmppath);
        efree(rootpath);
        return;
    }
    efree(tmppath);
    
    // requested file exists, we check if file is within givet root path, 
    // otherwise we send 404 File Not Found to prevent giving informations 
    // about existence of this file on the server machine. This check prevents 
    // serving of the symlinks to outside of the root path as well.
    if (rootpath) {
        if (0 != php_can_strpos(filepath, rootpath, 0)) {
            php_can_throw_exception_code(
                ce_can_HTTPError TSRMLS_CC, 404, 
                    "Requested file '%s' is not within root path '%s'", filename, root
            );
            efree(filepath);
            efree(rootpath);
            return;
        }
        efree(rootpath);
    }
    
#ifdef R_OK
    // requested path exists and is within root path, check for read permissions
    if (VCWD_ACCESS(filepath, R_OK)) {
        php_can_throw_exception_code(
            ce_can_HTTPError TSRMLS_CC, 403, "Requested file '%s' is not readable", filename
        );
        efree(filepath);
        return;
    }
#endif
    
    int fd = -1;
    if ((fd = open(filepath, O_RDONLY)) < 0) {
        php_can_throw_exception(
            ce_can_RuntimeException TSRMLS_CC,
            "Cannot open the file '%s'", filename
        );
        efree(filepath);
        return;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        php_can_throw_exception(
            ce_can_RuntimeException TSRMLS_CC,
            "Cannot fstat the file '%s'", filename
        );
        efree(filepath);
        return;
    }
    
    // we do not serving directory listings, so if requested URI points to directory
    // we send 403 Forbidden response to the client
    if (S_ISDIR(st.st_mode)) {
        php_can_throw_exception_code(
            ce_can_HTTPError TSRMLS_CC, 403, "Requested path '%s' is a directory", filename
        );
        efree(filepath);
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    // generate and add ETag
    char *etag = NULL;
    int etag_len = spprintf(&etag, 0, "\"%x-%x-%x\"", (int)st.st_ino, (int)st.st_mtime, (int)st.st_size);
    evhttp_add_header(request->req->output_headers, "ETag", etag);
    
    // handle $mimetype
    if (mimetype_len == 0) {
        
        if (has_finfo == -1) {
            has_finfo = zend_lookup_class("\\finfo", sizeof("\\finfo") - 1, &finfo_cep TSRMLS_CC) == SUCCESS ? 
                1 : 0;
        }
        
        if (has_finfo) {
            
            if (mimetypes == NULL) {
                ALLOC_HASHTABLE(mimetypes);
                zend_hash_init(mimetypes, 100, NULL, ZVAL_PTR_DTOR, 0);
            }
            
            zval **cached;
            if (SUCCESS == zend_hash_find(mimetypes, etag, etag_len + 1, (void **)&cached)) {

                evhttp_add_header(request->req->output_headers, "Content-Type", Z_STRVAL_PP(cached));
                
            } else {

                zval *retval_ptr, *object, **params[1], *arg, *zfilepath, *retval = NULL;
                zend_fcall_info fci;
                zend_fcall_info_cache fcc;
                zend_class_entry *ce = *finfo_cep;

                ALLOC_INIT_ZVAL(object);
                object_init_ex(object, ce);

                MAKE_STD_ZVAL(arg);
                ZVAL_LONG(arg, 0x000010|0x000400); // MAGIC_MIME_TYPE|MAGIC_MIME_ENCODING
                params[0] = &arg;

                fci.size = sizeof(fci);
                fci.function_table = EG(function_table);
                fci.function_name = NULL;
                fci.symbol_table = NULL;
                fci.object_ptr = object;
                fci.retval_ptr_ptr = &retval_ptr;
                fci.param_count = 1;
                fci.params = params;
                fci.no_separation = 1;

                fcc.initialized = 1;
                fcc.function_handler = ce->constructor;
                fcc.calling_scope = EG(scope);
                fcc.called_scope = Z_OBJCE_P(object);
                fcc.object_ptr = object;

                // call constructor
                int result = zend_call_function(&fci, &fcc TSRMLS_CC);
                zval_ptr_dtor(&arg);
                if (retval_ptr) {
                    zval_ptr_dtor(&retval_ptr);
                }

                if (result == FAILURE) {
                    php_can_throw_exception(
                        ce_can_RuntimeException TSRMLS_CC,
                        "Failed to call '%s' constructor",
                        ce->name
                    );
                    zval *mtype;
                    MAKE_STD_ZVAL(mtype);
                    ZVAL_STRING(mtype, default_mimetype, 1);
                    zend_hash_add(mimetypes, etag, etag_len + 1, (void **)mtype, sizeof(zval), NULL);
                    efree(filepath);
                    efree(etag);
                    return;
                }

                // call finfo->file(filename)
                MAKE_STD_ZVAL(zfilepath);
                ZVAL_STRING(zfilepath, filepath, 1);
                zend_call_method_with_1_params(&object, Z_OBJCE_P(object), NULL, "file", &retval, zfilepath);
                zval_ptr_dtor(&zfilepath);

                if (!retval || Z_TYPE_P(retval) != IS_STRING) {
                    php_can_throw_exception(
                        ce_can_RuntimeException TSRMLS_CC,
                        "Unable determine mimetype of the '%s'",
                        filename
                    );
                    zval *mtype;
                    MAKE_STD_ZVAL(mtype);
                    ZVAL_STRING(mtype, default_mimetype, 1);
                    zend_hash_add(mimetypes, etag, etag_len + 1, (void **)mtype, sizeof(zval), NULL);
                    efree(filepath);
                    efree(etag);
                    return;
                }

                // workaround for CSS files bug in magic library. If css file beginns with C-style comments
                // magic returns text/x-c as mimetype - we rewright it to test/css if the file has .css extension
                if (0 == php_can_strpos(Z_STRVAL_P(retval), "text/x-c;", 0)) {
                    char *ext = php_can_substr(filepath, -5, 5);
                    if (ext != NULL) {
                        if (0 == strcasecmp(ext, ".css")) {
                            int mime_len = sizeof("text/x-c;") - 1;
                            char *encoding = php_can_substr(Z_STRVAL_P(retval), mime_len, Z_STRLEN_P(retval) - mime_len);
                            if (encoding != NULL) {
                                efree(Z_STRVAL_P(retval));
                                Z_STRLEN_P(retval) = spprintf(&(Z_STRVAL_P(retval)), 0, "text/css;%s", encoding);
                                efree(encoding);
                            }
                        }
                        efree(ext);
                    }
                }
                
                zend_hash_add(mimetypes, etag, etag_len + 1, &retval, sizeof(zval *), NULL);
                
                zval_add_ref(&retval);
                evhttp_add_header(request->req->output_headers, "Content-Type", Z_STRVAL_P(retval));
                zval_ptr_dtor(&retval);
                zval_ptr_dtor(&object);
                
            }
            
        } else {
            
            // finfo is not present, so just set to default mimetype
            evhttp_add_header(request->req->output_headers, "Content-Type", default_mimetype);
        }
        
    } else {
        // $mimetype was given
        evhttp_add_header(request->req->output_headers, "Content-Type", mimetype);
    }
    
    // handle $download
    if (download) { 
        char *basename = NULL;
        size_t basename_len;
        if (Z_TYPE_P(download) == IS_BOOL && Z_BVAL_P(download) == 1) { 
            // $download is true, determine basename of the file
            php_basename(filepath, strlen(filepath), NULL, 0, &basename, &basename_len TSRMLS_CC);
        } else if (Z_TYPE_P(download) == IS_STRING) {
            // $download is filename
            basename = estrndup(Z_STRVAL_P(download), Z_STRLEN_P(download));
        }
        if (basename) {
            spprintf(&basename, 0, "attachment; filename=\"%s\"", basename);
            evhttp_add_header(request->req->output_headers, "Content-Disposition", basename);
            efree(basename);
        }
    }
    
    efree(filepath);
    
    // add Accept-Ranges header to notify client that we can handle renged requests
    evhttp_add_header(request->req->output_headers, "Accept-Ranges", "bytes");
    
    // check if client gave us ETag in header
    const char *client_etag = evhttp_find_header(request->req->input_headers, "If-None-Match");
    if (client_etag != NULL && strcmp(client_etag, etag) == 0) {
        
        // ETags are the same 
        request->response_code = 304;
        evhttp_send_reply(request->req, request->response_code, NULL, NULL);
        
    } else {
        
        // ETag is not the same or unknown, so check client's modification stamp
        const char *client_lm = evhttp_find_header(request->req->input_headers, "If-Modified-Since");
        int client_ts = 0;
        if (client_lm != NULL) {
            zval retval, *strtotime, *time, *args[1];
            MAKE_STD_ZVAL(strtotime); ZVAL_STRING(strtotime, "strtotime", 1);
            MAKE_STD_ZVAL(time); ZVAL_STRING(time, client_lm, 1);
            args[0] = time; Z_ADDREF_P(args[0]);
            if (call_user_function(EG(function_table), NULL, strtotime, &retval, 1, args TSRMLS_CC) == SUCCESS) {
                if (Z_TYPE(retval) == IS_LONG) {
                    client_ts = Z_LVAL(retval);
                }
                zval_dtor(&retval);
            }
            Z_DELREF_P(args[0]);
            zval_ptr_dtor(&time);
            zval_ptr_dtor(&strtotime);
        }
        
        if (client_ts >= st.st_mtime) {
            
            // modification falg of the file is older than client's stamp
            // so send "Not Modified" response
            request->response_code = 304;
            evhttp_send_reply(request->req, request->response_code, NULL, NULL);

        } else {

            // generate and add Last-Modified header
            char *lm = NULL;
            zval retval, *gmstrftime, *format, *timestamp, *args[2];
            MAKE_STD_ZVAL(gmstrftime); ZVAL_STRING(gmstrftime, "gmstrftime", 1);
            MAKE_STD_ZVAL(format); ZVAL_STRING(format, "%a, %d %b %Y %H:%M:%S GMT", 1);
            MAKE_STD_ZVAL(timestamp); ZVAL_LONG(timestamp, st.st_mtime);
            args[0] = format; args[1] = timestamp;
            Z_ADDREF_P(args[0]); Z_ADDREF_P(args[1]);
            if (call_user_function(EG(function_table), NULL, gmstrftime, &retval, 2, args TSRMLS_CC) == SUCCESS) {
                if (Z_TYPE(retval) == IS_STRING) {
                    lm = estrndup(Z_STRVAL(retval), Z_STRLEN(retval));
                }
                zval_dtor(&retval);
            }
            Z_DELREF_P(args[0]); Z_DELREF_P(args[1]);
            zval_ptr_dtor(&format);
            zval_ptr_dtor(&timestamp);
            zval_ptr_dtor(&gmstrftime);
        
            if (lm != NULL) {
                evhttp_add_header(request->req->output_headers, "Last-Modified", lm);
                efree(lm);
            }
            
            // if request method is HEAD, just add Content-Length header
            // and send respinse without body
            if (request->req->type == EVHTTP_REQ_HEAD) {
                request->response_code = 200;
                char *size = NULL;
                spprintf(&size, 0, "%ld", (long)st.st_size);
                evhttp_add_header(request->req->output_headers, "Content-Length", size);
                evhttp_send_reply(request->req, request->response_code, NULL, NULL);
                efree(size);
            } else {
            
                // check if the client requested the ranged content
                long range_from = 0, range_to = st.st_size, range_len;
                char *range = (char *)evhttp_find_header(request->req->input_headers, "Range");
                if (range != NULL) {
                    int pos = php_can_strpos(range, "bytes=", 0);
                    if (FAILURE != pos) {
                        int part_len =  strlen(range) - 6;
                        char * part = php_can_substr(range, 6, part_len);
                        if (part != NULL) {
                            char start[part_len], end[part_len];
                            int is_end = 0, i, y = 0;
                            for(i = 0; i < part_len; i++) {
                                if (part[i] == '-') {
                                    is_end = 1;
                                    start[i] = '\0';
                                    continue;
                                }
                                if (is_end) {
                                    end[y++] = part[i];
                                } else {
                                    start[i] = part[i];
                                }
                            }
                            end[y] = '\0';
                            efree(part);

                            if (strlen(start) == 0) {
                                // bytes=-100 -> last 100 bytes
                                range_from = MAX(0, st.st_size - atol(end));
                                range_to = st.st_size;
                            } else if (strlen(end) == 0) {
                                // bytes=100- -> all but the first 99 bytes
                                range_from = atol(start);
                                range_to = st.st_size;
                            } else {
                                // bytes=100-200 -> bytes 100-200 (inclusive)
                                range_from = atol(start);
                                range_to = MIN(atol(end) + 1, st.st_size);
                            }
                        }
                    }
                }

                range_len = range_to - range_from;
                if (range_len <= 0) {

                    // requested range is not valid, so send appropriate
                    // response "Requested Range not satisfiable"
                    request->response_code = 416;
                    evhttp_send_reply(request->req, request->response_code, NULL, NULL);

                } else {

                    // set response code to 206 if partial content requested, to 200 otherwise
                    request->response_code = range_len != st.st_size ? 206 : 200;
                    
                    if (request->response_code == 206) {
                        char *range = NULL;
                        spprintf(&range, 0, "bytes %ld-%ld/%ld", range_from, range_to, (long)st.st_size);
                        evhttp_add_header(request->req->output_headers, "Content-Range", range);
                        efree(range);
                    }
                    request->response_len = range_len;
                    struct evbuffer *buffer = evbuffer_new();
                    evbuffer_add_file(buffer, fd, range_from, range_len);
                    evhttp_send_reply(request->req, request->response_code, NULL, buffer);
                    evbuffer_free(buffer);
                }
            }
        }
    }
    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_SENT;
    efree(etag);
}

/**
 * Start sending chunked response
 */
static PHP_METHOD(CanServerRequest, sendResponseStart)
{
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (request->status != PHP_CAN_SERVER_RESPONSE_STATUS_NONE) {
        php_can_throw_exception(
            ce_can_InvalidOperationException TSRMLS_CC,
            "Invalid status"
        );
        return;
    }
    
    zval *status, *reason = NULL;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z|z", &status, &reason) 
        || Z_TYPE_P(status) != IS_LONG 
        || (reason && (Z_TYPE_P(reason) != IS_STRING || Z_STRLEN_P(reason) == 0))
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(int $status[, string $reason])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    if (Z_LVAL_P(status) < 100 || Z_LVAL_P(status) > 599) {
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Unexpected HTTP status, expecting range is 100-599"
        );
        return;
    }
    
    evhttp_send_reply_start(request->req, Z_LVAL_P(status), reason != NULL ? Z_STRVAL_P(reason) : NULL);

    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_SENDING;
    request->response_len = 0;
    request->response_code = Z_LVAL_P(status);
}

/**
 * Send next response chunk
 */
static PHP_METHOD(CanServerRequest, sendResponseChunk)
{
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (request->status != PHP_CAN_SERVER_RESPONSE_STATUS_SENDING) {
        php_can_throw_exception(
            ce_can_InvalidOperationException TSRMLS_CC,
            "Invalid status"
        );
        return;
    }
    
    zval *chunk;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z", &chunk) 
        || Z_TYPE_P(chunk) != IS_STRING 
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $chunk)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    if (Z_STRLEN_P(chunk) > 0) {
        struct evbuffer *buffer = evbuffer_new();
        evbuffer_add(buffer, Z_STRVAL_P(chunk), Z_STRLEN_P(chunk));
        evhttp_send_reply_chunk(request->req, buffer);
        evbuffer_free(buffer);
        request->response_len += Z_STRLEN_P(chunk);
    }
}

/**
 * Finalize sending chunked response
 */
static PHP_METHOD(CanServerRequest, sendResponseEnd)
{
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (request->status != PHP_CAN_SERVER_RESPONSE_STATUS_SENDING) {
        php_can_throw_exception(
            ce_can_InvalidOperationException TSRMLS_CC,
            "Invalid status"
        );
        return;
    }

    evhttp_send_reply_end(request->req);

    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_SENT;
}

static zend_function_entry server_request_methods[] = {
    PHP_ME(CanServerRequest, __construct,          NULL, ZEND_ACC_FINAL | ZEND_ACC_PROTECTED)
    PHP_ME(CanServerRequest, findRequestHeader,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, findResponseHeader,   NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, getRequestBody,       NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, getResponseBody,      NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, setResponseBody,      NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, addResponseHeader,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, removeResponseHeader, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, getResponseHeaders,   NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, setResponseStatus,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, redirect,             NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, setCookie,            NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, sendFile,             NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, sendResponseStart,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, sendResponseChunk,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, sendResponseEnd,      NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_request_init(TSRMLS_D)
{
    memcpy(&server_request_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_request_obj_handlers.clone_obj = NULL;
    server_request_obj_handlers.read_property = read_property;
    server_request_obj_handlers.get_properties = get_properties;
    server_request_obj_handlers.write_property = write_property;
    
    // class \Can\Server\Request
    PHP_CAN_REGISTER_CLASS(
        &ce_can_server_request,
        ZEND_NS_NAME(PHP_CAN_SERVER_NS, "Request"),
        server_request_ctor,
        server_request_methods
    );

    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_server_request, "STATUS_NONE",
        PHP_CAN_SERVER_RESPONSE_STATUS_NONE);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_server_request, "STATUS_SENDING",
        PHP_CAN_SERVER_RESPONSE_STATUS_SENDING);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_server_request, "STATUS_SENT",
        PHP_CAN_SERVER_RESPONSE_STATUS_SENT);
    PHP_CAN_REGISTER_CLASS_CONST_LONG(ce_can_server_request, "STATUS_FORWARD",
        PHP_CAN_SERVER_RESPONSE_STATUS_FORWARD);
}

PHP_MINIT_FUNCTION(can_server_request)
{
    server_request_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(can_server_request)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(can_server_request)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(can_server_request)
{
    return SUCCESS;
}
