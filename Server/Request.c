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

zend_class_entry *ce_buddel_server_request;
static zend_object_handlers server_request_obj_handlers;

static void server_request_dtor(void *object TSRMLS_DC);

static zend_object_value server_request_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_buddel_server_request *r;
    zend_object_value retval;

    r = ecalloc(1, sizeof(*r));
    zend_object_std_init(&r->std, ce TSRMLS_CC);
    r->cookies = NULL;
    r->get = NULL;
    r->post = NULL;
    r->files = NULL;
    r->status = PHP_BUDDEL_SERVER_RESPONSE_STATUS_NONE;
    r->uri = NULL;
    r->query = NULL;
    r->response_status = 0;
    r->error = NULL;
    retval.handle = zend_objects_store_put(r,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_request_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_request_obj_handlers;
    return retval;
}

static void server_request_dtor(void *object TSRMLS_DC)
{
    struct php_buddel_server_request *r = (struct php_buddel_server_request*)object;

    if (r->req) {
        r->req = NULL;
    }

    if (r->cookies) {
        zval_ptr_dtor(&r->cookies);
    }

    if (r->get) {
        zval_ptr_dtor(&r->get);
    }

    if (r->post) {
        zval_ptr_dtor(&r->post);
    }

    if (r->files) {
        zval_ptr_dtor(&r->files);
    }

    if (r->uri) {
        efree(r->uri);
        r->uri = NULL;
    }

    if (r->query) {
        efree(r->query);
        r->query = NULL;
    }
    
    if (r->error) {
        efree(r->error);
        r->error = NULL;
    }

    zend_objects_store_del_ref(&r->refhandle TSRMLS_CC);
    zend_object_std_dtor(&r->std TSRMLS_CC);
    efree(r);
}

static char * typeToMethod(int type)
{
    switch (type) {
        case EVHTTP_REQ_GET:
            return "GET";
            break;
        case EVHTTP_REQ_POST:
            return "POST";
            break;
        case EVHTTP_REQ_HEAD:
            return "HEAD";
            break;
        case EVHTTP_REQ_PUT:
            return "PUT";
            break;
        case EVHTTP_REQ_DELETE:
            return "DELETE";
            break;
        case EVHTTP_REQ_OPTIONS:
            return "OPTIONS";
            break;
        case EVHTTP_REQ_TRACE:
            return "TRACE";
            break;
        case EVHTTP_REQ_CONNECT:
            return "CONNECT";
            break;
        case EVHTTP_REQ_PATCH:
            return "PATCH";
            break;
        default:
            return "Unknown";
            break;
    }
}

static zval *read_property(zval *object, zval *member, int type, const zend_literal *key TSRMLS_DC)
{
    struct php_buddel_server_request *r;
    zval tmp_member;
    zval *retval;
    zend_object_handlers *std_hnd;
    struct evkeyval *header;
    char * str;

    r = (struct php_buddel_server_request*)zend_object_store_get_object(object TSRMLS_CC);

    if (member->type != IS_STRING) {
        tmp_member = *member;
        zval_copy_ctor(&tmp_member);
        convert_to_string(&tmp_member);
        member = &tmp_member;
    }

    if (Z_STRLEN_P(member) == (sizeof("method") - 1)
            && !memcmp(Z_STRVAL_P(member), "method", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_STRING(retval, typeToMethod(r->req->type), 1);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("uri") - 1)
            && !memcmp(Z_STRVAL_P(member), "uri", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_STRING(retval, r->uri != NULL ? r->uri : "", 1);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("query") - 1)
            && !memcmp(Z_STRVAL_P(member), "query", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_STRING(retval, r->query != NULL ? r->query : "", 1);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("protocol") - 1)
            && !memcmp(Z_STRVAL_P(member), "protocol", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        spprintf(&str, 0, "HTTP/%d.%d", r->req->major, r->req->minor);
        ZVAL_STRING(retval, str, 1);
        Z_SET_REFCOUNT_P(retval, 0);
        efree(str);

    } else if (Z_STRLEN_P(member) == (sizeof("remote_addr") - 1)
            && !memcmp(Z_STRVAL_P(member), "remote_addr", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        if (r->req->remote_host) {
            ZVAL_STRING(retval, r->req->remote_host, 1);
        } else {
            ZVAL_STRING(retval, "", 1);
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("remote_port") - 1)
            && !memcmp(Z_STRVAL_P(member), "remote_port", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_LONG(retval, (int)r->req->remote_port);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("headers") - 1)
            && !memcmp(Z_STRVAL_P(member), "headers", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        TAILQ_FOREACH( header, r->req->input_headers, next)
        {
            add_assoc_string(retval, header->key, header->value, 1);
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("cookies") - 1)
            && !memcmp(Z_STRVAL_P(member), "cookies", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (r->cookies) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(r->cookies),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("get") - 1)
            && !memcmp(Z_STRVAL_P(member), "get", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (r->get) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(r->get),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("post") - 1)
            && !memcmp(Z_STRVAL_P(member), "post", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (r->post) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(r->post),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("files") - 1)
            && !memcmp(Z_STRVAL_P(member), "files", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        if (r->files) {
            zval *tmp;
            zend_hash_copy(
                Z_ARRVAL_P(retval), Z_ARRVAL_P(r->files),
                (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
            );
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("status") - 1)
            && !memcmp(Z_STRVAL_P(member), "status", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_LONG(retval, (int)r->status);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("time") - 1)
            && !memcmp(Z_STRVAL_P(member), "time", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_DOUBLE(retval, r->time);
        Z_SET_REFCOUNT_P(retval, 0);

    } else {
        std_hnd = zend_get_std_object_handlers();
        retval = std_hnd->read_property(object, member, type, key TSRMLS_CC);
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
    
    struct php_buddel_server_request *r = (struct php_buddel_server_request*)
        zend_objects_get_address(object TSRMLS_CC);
    
    
    props = zend_std_get_properties(object TSRMLS_CC);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, typeToMethod(r->req->type), 1);
    zend_hash_update(props, "method", sizeof("method"), &zv, sizeof(zval), NULL);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, r->uri != NULL ? r->uri : "", 1);
    zend_hash_update(props, "uri", sizeof("uri"), &zv, sizeof(zval), NULL);
    
    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, r->query != NULL ? r->query : "", 1);
    zend_hash_update(props, "query", sizeof("query"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    spprintf(&str, 0, "HTTP/%d.%d", r->req->major, r->req->minor);
    ZVAL_STRING(zv, str, 1);
    zend_hash_update(props, "protocol", sizeof("protocol"), &zv, sizeof(zval), NULL);
    efree(str);

    MAKE_STD_ZVAL(zv);
    ZVAL_STRING(zv, r->req->remote_host ? r->req->remote_host : "", 1);
    zend_hash_update(props, "remote_addr", sizeof("remote_addr"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    ZVAL_LONG(zv, (int)r->req->remote_port);
    zend_hash_update(props, "remote_port", sizeof("remote_port"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    TAILQ_FOREACH (header, r->req->input_headers, next)
    {
        add_assoc_string(zv, header->key, header->value, 1);
    }
    zend_hash_update(props, "headers", sizeof("headers"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (r->cookies) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(r->cookies),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "cookies", sizeof("cookies"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (r->get) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(r->get),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "get", sizeof("get"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (r->post) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(r->post),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "post", sizeof("post"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    if (r->files) {
        zval *tmp;
        zend_hash_copy(
            Z_ARRVAL_P(zv), Z_ARRVAL_P(r->files),
            (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *)
        );
    }
    zend_hash_update(props, "files", sizeof("files"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    ZVAL_LONG(zv, (int)r->status);
    zend_hash_update(props, "status", sizeof("status"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    ZVAL_DOUBLE(zv, r->time);
    zend_hash_update(props, "time", sizeof("time"), &zv, sizeof(zval), NULL);
    
    return props;
}

/**
 * Constructor
 */
static PHP_METHOD(BuddelServerRequest, __construct)
{
    /* final protected */
}

/**
 * Find request header
 */
static PHP_METHOD(BuddelServerRequest, findRequestHeader)
{
    char *header;
    int header_len;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "s", &header, &header_len)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_buddel_server_request *r = (struct php_buddel_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    const char *value =evhttp_find_header(r->req->input_headers, (const char*)header);
    if (value == NULL) {
        RETURN_FALSE;
    }
    RETURN_STRING(value, 1);
}

/**
 * Add response header
 *
 *
 */
static PHP_METHOD(BuddelServerRequest, addResponseHeader)
{
    char *header, *value;
    int header_len, value_len;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "ss", &header, &header_len, &value, &value_len)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header, string $value)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_buddel_server_request *r = (struct php_buddel_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (evhttp_add_header(r->req->output_headers, header, value) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

/**
 * Remove response header
 *
 *
 */
static PHP_METHOD(BuddelServerRequest, removeResponseHeader)
{
    char *header, *value;
    int header_len, value_len;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "s", &header, &header_len)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_buddel_server_request *r = (struct php_buddel_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (evhttp_remove_header(r->req->output_headers, header) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

/**
 * Set response status
 *
 *
 */
static PHP_METHOD(BuddelServerRequest, setResponseStatus)
{
    long status;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "l", &status)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(int $status)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    if (status < 100 || status > 599) {
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "Expection valid HTTP status (100-599)"
        );
        return;
    }

    struct php_buddel_server_request *r = (struct php_buddel_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    r->response_status = status;
}

/**
 * Get raw request data
 */
static PHP_METHOD(BuddelServerRequest, getRequestBody)
{
    struct php_buddel_server_request *r = (struct php_buddel_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    int buffer_len = EVBUFFER_LENGTH(r->req->input_buffer);
    if (buffer_len > 0) {
        RETURN_STRINGL(EVBUFFER_DATA(r->req->input_buffer), buffer_len, 1);
    }
    RETURN_FALSE;
}

static zend_function_entry server_request_methods[] = {
    PHP_ME(BuddelServerRequest, __construct,          NULL, ZEND_ACC_FINAL | ZEND_ACC_PROTECTED)
    PHP_ME(BuddelServerRequest, findRequestHeader,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRequest, addResponseHeader,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRequest, removeResponseHeader, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRequest, setResponseStatus,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServerRequest, getRequestBody,       NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_request_init(TSRMLS_D)
{
    memcpy(&server_request_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_request_obj_handlers.clone_obj = NULL;
    server_request_obj_handlers.read_property = read_property;
    server_request_obj_handlers.get_properties = get_properties;
    
    // class \Buddel\Server\Request
    PHP_BUDDEL_REGISTER_CLASS(
        &ce_buddel_server_request,
        ZEND_NS_NAME(PHP_BUDDEL_SERVER_NS, "Request"),
        server_request_ctor,
        server_request_methods
    );

    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_request, "STATUS_NONE",
        PHP_BUDDEL_SERVER_RESPONSE_STATUS_NONE);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_request, "STATUS_SENDING",
        PHP_BUDDEL_SERVER_RESPONSE_STATUS_SENDING);
    PHP_BUDDEL_REGISTER_CLASS_CONST_LONG(ce_buddel_server_request, "STATUS_SENT",
        PHP_BUDDEL_SERVER_RESPONSE_STATUS_SENT);
}

PHP_MINIT_FUNCTION(buddel_server_request)
{
    server_request_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(buddel_server_request)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(buddel_server_request)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(buddel_server_request)
{
    return SUCCESS;
}