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

zend_class_entry *ce_can_server_request;
static zend_object_handlers server_request_obj_handlers;

static void server_request_dtor(void *object TSRMLS_DC);

static zend_object_value server_request_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_can_server_request *request;
    zend_object_value retval;

    request = ecalloc(1, sizeof(*request));
    zend_object_std_init(&request->std, ce TSRMLS_CC);
    request->cookies = NULL;
    request->get = NULL;
    request->post = NULL;
    request->files = NULL;
    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_NONE;
    request->uri = NULL;
    request->query = NULL;
    request->response_status = 0;
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

static zval *read_property(zval *object, zval *member, int type, const zend_literal *key TSRMLS_DC)
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

    } else if (Z_STRLEN_P(member) == (sizeof("remote_addr") - 1)
            && !memcmp(Z_STRVAL_P(member), "remote_addr", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        if (request->req->remote_host) {
            ZVAL_STRING(retval, request->req->remote_host, 1);
        } else {
            ZVAL_STRING(retval, "", 1);
        }
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("remote_port") - 1)
            && !memcmp(Z_STRVAL_P(member), "remote_port", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_LONG(retval, (int)request->req->remote_port);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("headers") - 1)
            && !memcmp(Z_STRVAL_P(member), "headers", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        array_init(retval);
        for (header = ((request->req->input_headers)->tqh_first);
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
        ZVAL_LONG(retval, (int)request->status);
        Z_SET_REFCOUNT_P(retval, 0);

    } else if (Z_STRLEN_P(member) == (sizeof("time") - 1)
            && !memcmp(Z_STRVAL_P(member), "time", Z_STRLEN_P(member))) {

        MAKE_STD_ZVAL(retval);
        ZVAL_DOUBLE(retval, request->time);
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
    zend_hash_update(props, "remote_addr", sizeof("remote_addr"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    ZVAL_LONG(zv, (int)request->req->remote_port);
    zend_hash_update(props, "remote_port", sizeof("remote_port"), &zv, sizeof(zval), NULL);

    MAKE_STD_ZVAL(zv);
    array_init(zv);
    for (header = ((request->req->input_headers)->tqh_first);
         header; 
         header = ((header)->next.tqe_next)
    ) {
        add_assoc_string(zv, header->key, header->value, 1);
    }
    zend_hash_update(props, "headers", sizeof("headers"), &zv, sizeof(zval), NULL);

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
    
    return props;
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
    char *header;
    int header_len;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "s", &header, &header_len)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    const char *value =evhttp_find_header(request->req->input_headers, (const char*)header);
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
 * Add response header
 *
 *
 */
static PHP_METHOD(CanServerRequest, addResponseHeader)
{
    char *header, *value;
    int header_len, value_len;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "ss", &header, &header_len, &value, &value_len)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header, string $value)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (evhttp_add_header(request->req->output_headers, header, value) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

/**
 * Remove response header
 *
 *
 */
static PHP_METHOD(CanServerRequest, removeResponseHeader)
{
    char *header, *value;
    int header_len = 0, value_len = 0;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "s|s", &header, &header_len, &value, &value_len) || header_len == 0) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $header[, string $value])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (value_len > 0) {
        char *existing_value = (char *)evhttp_find_header(request->req->output_headers, value);
        if (NULL != existing_value && 0 == strcmp(value, existing_value)
           && 0 == evhttp_remove_header(request->req->output_headers, header)
        ) {
            RETURN_TRUE;
        }
        RETURN_FALSE;
    } 

    if (evhttp_remove_header(request->req->output_headers, header) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

/**
 * Set response status
 *
 *
 */
static PHP_METHOD(CanServerRequest, setResponseStatus)
{
    long status;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "l", &status)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(int $status)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    if (status < 100 || status > 599) {
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Expection valid HTTP status (100-599)"
        );
        return;
    }

    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    request->response_status = status;
}

/**
 * Redirect client to new location
 */
static PHP_METHOD(CanServerRequest, redirect)
{
    char *location;
    int *location_len = 0;
    long status = 302;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "s|l", &location, &location_len, &status) || location_len == 0 || status < 300 || status > 399) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $location[, int status = 302])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (evhttp_add_header(request->req->output_headers, "Location", location) != 0) {
        RETURN_FALSE;
    }
    request->response_status = status;
    RETURN_TRUE;
}

/**
 * Set cookie
 */
static PHP_METHOD(CanServerRequest, setCookie)
{
    char *name, *value, *path, *domain;
    int name_len = 0, value_len = 0, path_len = 0, domain_len = 0;
    long expires = 0;
    zend_bool secure = 0, httponly= 0, url_encode = 0; 

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "ss|slssbb", &name, &name_len, &value, &value_len, &expires, &path, &path_len, 
            &domain, &domain_len, &secure, &httponly) || name_len == 0
    ) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $name [, string $value [, int $expire = 0 [, string $path "
            "[, string $domain [, bool $secure = false [, bool $httponly = false ]]]]]])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    char *cookie, *encoded_value = NULL;
    int len = name_len;
    char *dt;
    int result;

    if (name && strpbrk(name, "=,; \t\r\n\013\014") != NULL) {   /* man isspace for \013 and \014 */
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Cookie names cannot contain any of the following '=,; \\t\\r\\n\\013\\014'"
        );
        return;
    }

    if (!url_encode && value && strpbrk(value, ",; \t\r\n\013\014") != NULL) { /* man isspace for \013 and \014 */
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Cookie values cannot contain any of the following '=,; \\t\\r\\n\\013\\014'"
        );
        return;
    }

    if (value && url_encode) {
        int encoded_value_len;
        encoded_value = php_url_encode(value, value_len, &encoded_value_len);
        len += encoded_value_len;
    } else if ( value ) {
        encoded_value = estrdup(value);
        len += value_len;
    }
    if (path) {
        len += path_len;
    }
    if (domain) {
        len += domain_len;
    }

    cookie = emalloc(len + 100);

    if (value && value_len == 0) {
        /* 
         * MSIE doesn't delete a cookie when you set it to a null value
         * so in order to force cookies to be deleted, even on MSIE, we
         * pick an expiry date in the past
         */
        dt = php_format_date("D, d-M-Y H:i:s T", sizeof("D, d-M-Y H:i:s T")-1, 1, 0 TSRMLS_CC);
        snprintf(cookie, len + 100, "%s=deleted; expires=%s", name, dt);
        efree(dt);
    } else {
        snprintf(cookie, len + 100, "%s=%s", name, value ? encoded_value : "");
        if (expires > 0) {
            const char *p;
            strlcat(cookie, "; expires=", len + 100);
            dt = php_format_date("D, d-M-Y H:i:s T", sizeof("D, d-M-Y H:i:s T")-1, expires, 0 TSRMLS_CC);
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

    if (path && path_len > 0) {
        strlcat(cookie, "; path=", len + 100);
        strlcat(cookie, path, len + 100);
    }
    if (domain && domain_len > 0) {
        strlcat(cookie, "; domain=", len + 100);
        strlcat(cookie, domain, len + 100);
    }
    if (secure) {
        strlcat(cookie, "; secure", len + 100);
    }
    if (httponly) {
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
 * Get realpath of the given filename
 */
static char *get_realpath(char *filename TSRMLS_DC)
{
    char resolved_path_buff[MAXPATHLEN];
    if (VCWD_REALPATH(filename, resolved_path_buff)) {
        if (php_check_open_basedir(resolved_path_buff TSRMLS_CC)) {
            return NULL;
        }
#ifdef ZTS
        if (VCWD_ACCESS(resolved_path_buff, F_OK)) {
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

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "p|pszl", &filename, &filename_len, &root, &root_len, 
                     &mimetype, &mimetype_len, &download, &chunksize)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $filename[, string $root[, string $mimetype[, string $download[, int $chunksize=10240]]]])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    // try to determine real path of the given root
    char *path = NULL;
    if (root_len > 0) {
        path = get_realpath(root TSRMLS_CC);
        if (path == NULL) {
            const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
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
    char *filepath = NULL;
    spprintf(&filepath, 0, "%s%s%s", path ? path : "", (path && filename[0] != '/' ? "/" : ""), filename);
    if (path != NULL) {
        efree(path);
    }
    path = get_realpath(filepath TSRMLS_CC);
    if (path == NULL) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(): Cannot determine real path of file '%s'",
            class_name, space, get_active_function_name(TSRMLS_C),
            filepath
        );
        efree(filepath);
        return;
    }
    
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    // handle $mimetype
    if (mimetype_len == 0) {
        // $mimtype was not given, so try to determine mimetype with finfo
        zend_class_entry **cep;
        if (zend_lookup_class_ex("\\finfo", sizeof("\\finfo") - 1, NULL, 0, &cep TSRMLS_CC) == SUCCESS) {

            zval *retval_ptr, *object, **params[1], *arg, *zfilepath, *retval;
            zend_fcall_info fci;
            zend_fcall_info_cache fcc;
            zend_class_entry *ce = *cep;
            
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
                
                efree(filepath);
                if (path != NULL) {
                    efree(path);
                }
                return;
            }
            
            // call finfo->file(filename)
            MAKE_STD_ZVAL(zfilepath);
            ZVAL_STRING(zfilepath, path, 1);
            zend_call_method_with_1_params(&object, Z_OBJCE_P(object), NULL, "file", &retval, zfilepath);
            zval_ptr_dtor(&zfilepath);
            if (EG(exception)) {
                efree(filepath);
                if (path != NULL) {
                    efree(path);
                }
                return;
            }

            evhttp_add_header(request->req->output_headers, "Content-Type", Z_STRVAL_P(retval));
            zval_ptr_dtor(&retval);
            zval_ptr_dtor(&object);
            
        } else {
            // finfo is not present, so just set to text/plain 
            evhttp_add_header(request->req->output_headers, "Content-Type", "text/plain");
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
    
    // open stream wrapper to the file
    php_stream *stream = php_stream_open_wrapper(path, "rb", ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
    if (!stream) {
        php_can_throw_exception(
            ce_can_RuntimeException TSRMLS_CC,
            "Cannot read content of the file '%s'", path
        );
        if (path != NULL) {
            efree(path);
        }
        return;
    }
    
    // get file stats
    php_stream_statbuf st;
    if (php_stream_stat(stream, &st) < 0) {
        php_can_throw_exception(
            ce_can_RuntimeException TSRMLS_CC,
            "Cannot stat of the file '%s'", path
        );
        if (path != NULL) {
            efree(path);
        }
        return;
    }
    
    // generate ETag
    char *etag = NULL;
    spprintf(&etag, 0, "%X-%X-%X", (int)st.sb.st_ino, (int)st.sb.st_mtime, (int)st.sb.st_size);

    // check if client gave us ETag in header
    const char *client_etag = evhttp_find_header(request->req->input_headers, "If-None-Match");
    if (client_etag != NULL && strcmp(client_etag, etag) == 0) {
        
        // ETags are the same 
        request->response_status = 304;
        evhttp_send_reply(request->req, request->response_status, NULL, NULL);
        
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
        
        if (client_ts >= st.sb.st_mtime) {
            
            // modification falg of the file is older than client's stamp
            // so send "Not Modified" response
            request->response_status = 304;
            evhttp_send_reply(request->req, request->response_status, NULL, NULL);

        } else {

            // we will send 2xx response (depending on rerquest method)
            // so we add ETag header
            evhttp_add_header(request->req->output_headers, "ETag", etag);

            // generate and add Last-Modified header
            char *lm = NULL;
            zval retval, *gmstrftime, *format, *timestamp, *args[2];
            MAKE_STD_ZVAL(gmstrftime); ZVAL_STRING(gmstrftime, "gmstrftime", 1);
            MAKE_STD_ZVAL(format); ZVAL_STRING(format, "%a, %d %b %Y %H:%M:%S GMT", 1);
            MAKE_STD_ZVAL(timestamp); ZVAL_LONG(timestamp, st.sb.st_mtime);
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
            
            // add Accept-Ranges header to notify client that we can handle
            // renged requests
            evhttp_add_header(request->req->output_headers, "Accept-Ranges", "bytes");
            
            // if request method is HEAD, just add Content-Length header
            // and send respinse without body
            if (request->req->type == EVHTTP_REQ_HEAD) {
                request->response_status = 200;
                char *size = NULL;
                spprintf(&size, 0, "%ld", (long)st.sb.st_size);
                evhttp_add_header(request->req->output_headers, "Content-Length", size);
                evhttp_send_reply(request->req, request->response_status, NULL, NULL);
                efree(size);
            } else {
            
                // check if the client requested the ranged content
                long range_from = 0, range_to = st.sb.st_size, range_len;
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
                                range_from = MAX(0, st.sb.st_size - atol(end));
                                range_to = st.sb.st_size;
                            } else if (strlen(end) == 0) {
                                // bytes=100- -> all but the first 99 bytes
                                range_from = atol(start);
                                range_to = st.sb.st_size;
                            } else {
                                // bytes=100-200 -> bytes 100-200 (inclusive)
                                range_from = atol(start);
                                range_to = MIN(atol(end) + 1, st.sb.st_size);
                            }
                        }
                    }
                }

                range_len = range_to - range_from;
                if (range_len <= 0) {

                    // requested range is not valid, so send appropriate
                    // response "Requested Range not satisfiable"
                    request->response_status = 416;
                    evhttp_send_reply(request->req, request->response_status, NULL, NULL);

                } else {

                    // set response code to 206 if partial content requested, to 200 otherwise
                    request->response_status = range_len != st.sb.st_size ? 206 : 200;
                    
                    // if requested range is smaller then chunksize,
                    // do not use chunked transfer encoding
                    if (chunksize == 0 || range_len <= chunksize) {

                        char *content;
                        php_stream_seek(stream, range_from, SEEK_SET );
                        int content_len = php_stream_copy_to_mem(stream, &content, range_len, 0);
                        if (request->response_status == 206) {
                            char *range = NULL;
                            spprintf(&range, 0, "bytes %ld-%ld/%ld", range_from, range_to, (long)st.sb.st_size);
                            evhttp_add_header(request->req->output_headers, "Content-Range", range);
                            efree(range);
                        }
                        request->response_len = content_len;
                        struct evbuffer *buffer = evbuffer_new();
                        evbuffer_add(buffer, content, content_len);
                        evhttp_send_reply(request->req, 200, NULL, buffer);
                        evbuffer_free(buffer);
                        efree(content);

                    } else {

                        // send contentas chunkd transfer encoding
                        long pos = range_from, len;
                        char *chunk;
                        while(-1 != php_stream_seek(stream, pos, SEEK_SET )) {
                            if (pos == range_from) {
                                request->status = PHP_CAN_SERVER_RESPONSE_STATUS_SENDING;
                                request->response_len = 0;
                                evhttp_send_reply_start(request->req, request->response_status, NULL);
                            }
                            int len = (request->response_len + chunksize) > range_len ? (range_len - request->response_len) : chunksize;
                            int chunk_len = php_stream_copy_to_mem(stream, &chunk, len, 0);
                            if (chunk_len == 0) {
                                efree(chunk);
                                break;
                            }
                            struct evbuffer *buffer = evbuffer_new();
                            evbuffer_add(buffer, chunk, chunk_len);
                            evhttp_send_reply_chunk(request->req, buffer);
                            evbuffer_free(buffer);
                            efree(chunk);
                            request->response_len += chunk_len;
                            pos += chunk_len;
                            if (request->response_len == range_len) {
                                break;
                            }
                        }
                        evhttp_send_reply_end(request->req);
                    }
                }
            }
        }
    }
    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_SENT;
    
    efree(etag);
    php_stream_close(stream);
    if (path != NULL) {
        efree(path);
    }
}

static zend_function_entry server_request_methods[] = {
    PHP_ME(CanServerRequest, __construct,          NULL, ZEND_ACC_FINAL | ZEND_ACC_PROTECTED)
    PHP_ME(CanServerRequest, findRequestHeader,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, getRequestBody,       NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, addResponseHeader,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, removeResponseHeader, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, setResponseStatus,    NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, redirect,             NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, setCookie,            NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerRequest, sendFile,             NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_request_init(TSRMLS_D)
{
    memcpy(&server_request_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_request_obj_handlers.clone_obj = NULL;
    server_request_obj_handlers.read_property = read_property;
    server_request_obj_handlers.get_properties = get_properties;
    
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
