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

extern ZEND_DECLARE_MODULE_GLOBALS(can)

#ifndef HAVE_TAILQFOREACH
#include <sys/queue.h>
#endif

#include <event.h>
#include <evhttp.h>
#include <signal.h>
#include <sys/time.h>

static zend_bool request_counter_used = 0;
static long request_counter = 0;

zend_class_entry *ce_can_server;
static zend_object_handlers server_obj_handlers;

void php_can_parse_multipart(const char* content_type, struct evbuffer* buffer, zval* post, zval** files TSRMLS_DC);
void server_websocket_route_handle_request(zval *zroute, zval *zrequest, zval *params TSRMLS_DC);
static void server_dtor(void *object TSRMLS_DC);

static zend_object_value server_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_can_server *server;
    zend_object_value retval;

    server = ecalloc(1, sizeof(*server));
    server->logformat = NULL;
    server->logfile = NULL;
    server->router = NULL;
    zend_object_std_init(&server->std, ce TSRMLS_CC);
    object_properties_init(&server->std, ce);
    retval.handle = zend_objects_store_put(server,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_obj_handlers;
    return retval;
}

static void server_dtor(void *object TSRMLS_DC)
{
    struct php_can_server *server = (struct php_can_server*)object;

    if (server->http) {
        free(server->http);
        server->http = NULL;
    }

    zend_objects_store_del_ref(&server->refhandle TSRMLS_CC);
    zend_object_std_dtor(&server->std TSRMLS_CC);

    if (server->addr) {
        efree(server->addr);
        server->addr = NULL;
    }

    if (server->logformat) {
        efree(server->logformat);
        server->logformat = NULL;
    }

    if (server->logfile) {
        php_stream_close(server->logfile);
        server->logfile = NULL;
    }
    
    if (server->router) {
        zval_ptr_dtor(&server->router);
    }
    efree(server);
}

static void free_client_ctx(struct php_can_client_ctx *ctx)
{
    if (ctx->evcon) {
        evhttp_connection_free(ctx->evcon);
    }
    if (ctx->zrequest) {
        zval_ptr_dtor(&ctx->zrequest);
    }
    if (ctx->callback) {
        zval_ptr_dtor(&ctx->callback);
    }
    free(ctx);
}

/**
 * Response handler for forwarded requests
 */
static void forward_response_callback(struct evhttp_request *response, void *arg)
{
    struct php_can_client_ctx *ctx = (struct php_can_client_ctx *)arg;

    struct php_can_server_request *origin_request = (struct php_can_server_request*)
        zend_object_store_get_object(ctx->zrequest TSRMLS_CC);
    
    if (!response) {
        // missing response, send 500 error
        evhttp_send_error(origin_request->req, 500, NULL);
        free_client_ctx(ctx);
        return;
    }
    
    // copy response headers
    struct evkeyval *header;
    for (header = ((response->input_headers)->tqh_first); header; header = ((header)->next.tqe_next)) {
        evhttp_add_header(origin_request->req->output_headers, header->key, header->value);
    }
    
    // coopy response body
    evbuffer_add_buffer(origin_request->req->output_buffer, response->input_buffer);
    
    if (ctx->callback) {
        
        zval *zrequest, retval, *args[1];
        MAKE_STD_ZVAL(zrequest);
        object_init_ex(zrequest, ce_can_server_request);
        Z_SET_REFCOUNT_P(zrequest, 1);
        struct php_can_server_request *request = (struct php_can_server_request *)
                zend_object_store_get_object(zrequest TSRMLS_CC);
        request->req = origin_request->req;
        request->response_code = (long)response->response_code;
        request->response_len = EVBUFFER_LENGTH(origin_request->req->output_buffer);
        
        args[0] = zrequest;
        Z_ADDREF_P(args[0]);
        if (call_user_function(EG(function_table), NULL, ctx->callback, &retval, 1, args TSRMLS_CC) == SUCCESS) {
            if (response->response_code != request->response_code) {
                response->response_code = (int)request->response_code;
            }
            // calculate the content-length header if body length is changed
            size_t content_len = EVBUFFER_LENGTH(origin_request->req->output_buffer);
            if (content_len != EVBUFFER_LENGTH(response->input_buffer)) {
                evhttp_remove_header(origin_request->req->output_headers, "Content-Length");
                char header_val[22];
                sprintf(header_val, "%ld", content_len);
                evhttp_add_header(origin_request->req->output_headers, "Content-Length", header_val);
            }
            zval_dtor(&retval);
        }
        Z_DELREF_P(args[0]);
        
        zval_ptr_dtor(&zrequest);
        
        if (EG(exception)) {
            // TODO: exeption handler
            zend_clear_exception(TSRMLS_C);
        }
    } 

    origin_request->response_len = EVBUFFER_LENGTH(origin_request->req->output_buffer);
    evhttp_send_reply(origin_request->req, response->response_code, NULL, origin_request->req->output_buffer);
    
    if (ctx->server->logformat_len) {
        struct php_can_server_logentry *logentry;
        LOGENTRY_CTOR(logentry, origin_request);
        LOGENTRY_LOG(logentry, ctx->server, ctx->request_id);
        LOGENTRY_DTOR(logentry);
    }

    free_client_ctx(ctx);
}

static void forward_request(const char *url, zval *zrequest, struct php_can_server *server, zval *headers, zval *callback)
{
    struct php_can_server_request *request = (struct php_can_server_request*)
        zend_object_store_get_object(zrequest TSRMLS_CC);
    
    struct evhttp_uri *uri = evhttp_uri_parse(url);
    if (uri == NULL) {
        request->response_code = 500;
        spprintf(&request->error, 0, "%s ``%s``", "Cannot parse URL", url);
    } else {
        struct php_can_client_ctx *ctx = 0;
        ctx = calloc(1, sizeof(*ctx));
        if (!ctx) {
            request->response_code = 500;
            spprintf(&request->error, 0, "%s", "Cannot allocate client_ctx");
        } else {
            ctx->callback = NULL;
            if (callback) {
                Z_ADDREF_P(callback);
                ctx->callback = callback;
            }
            Z_ADDREF_P(zrequest);
            ctx->request_id = request_counter;
            ctx->zrequest = zrequest;
            ctx->server = server;
            ctx->evcon = evhttp_connection_base_new(CAN_G(can_event_base), NULL, 
                    evhttp_uri_get_host(uri), evhttp_uri_get_port(uri));
            if (!ctx->evcon) {
                request->response_code = 500;
                spprintf(&request->error, 0, "%s", "Cannot create new evhttp_connection\n");
                free_client_ctx(ctx);
            } else {
                struct evhttp_request *c_req = evhttp_request_new(forward_response_callback, ctx);
                if (!c_req) {
                    request->response_code = 500;
                    spprintf(&request->error, 0, "%s", "Cannot create new evhttp_request\n");
                    free_client_ctx(ctx);
                } else {

                    struct evkeyval *header;
                    for (header=((request->req->input_headers)->tqh_first); header; header=((header)->next.tqe_next)) {
                        evhttp_add_header(c_req->output_headers, header->key, header->value);
                    }
                    
                    if (headers && Z_TYPE_P(headers) == IS_ARRAY) {
                        zval **item;
                        PHP_CAN_FOREACH(headers, item) {
                            if (keytype == HASH_KEY_IS_STRING && Z_TYPE_PP(item) == IS_STRING) {
                                if (evhttp_find_header(c_req->output_headers, (const char *)strkey) != NULL) {
                                    evhttp_remove_header(c_req->output_headers, (const char *)strkey);
                                }
                                if (Z_STRLEN_PP(item) > 0) {
                                    evhttp_add_header(c_req->output_headers, (const char *)strkey, 
                                            (const char *)Z_STRVAL_PP(item));
                                }
                            }
                        }
                    }

                    char *forwardUri = NULL;
                    const char *path = evhttp_uri_get_path(uri),
                               *query = evhttp_uri_get_query(uri);
                    spprintf(&forwardUri, 0, "%s%s%s", path ? path : "/", query ? "?" : "", query ? query : "");
                    evhttp_make_request(ctx->evcon, c_req, request->req->type, forwardUri);
                    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_FORWARD;
                    efree(forwardUri);
                }
            }
        }
    }
}

/**
 * Remove null byte from any string value
 */
static int cleanUp(zval **item TSRMLS_DC)
{
    /* TODO: do we need stripslashes in PHP 5.4+ ?
    if (Z_TYPE_PP(item) == IS_STRING) {
        char *str = estrndup(Z_STRVAL_PP(item), Z_STRLEN_PP(item));
        int str_len = Z_STRLEN_PP(item);
        php_stripslashes(str, &str_len TSRMLS_CC);
        efree(Z_STRVAL_PP(item));
        Z_STRVAL_PP(item) = estrndup(str, str_len);
        Z_STRLEN_PP(item) = str_len;
        efree(str);
    }
    */
    if (Z_TYPE_PP(item) == IS_STRING) {
        int new_value_len, count = 0;
        char *new_value = php_str_to_str_ex(Z_STRVAL_PP(item), Z_STRLEN_PP(item), 
                "\0", 1, "", 0, &new_value_len, 0, &count);
        if (count > 0) {
            efree(Z_STRVAL_PP(item));
            Z_STRVAL_PP(item) = estrndup(new_value, new_value_len);
            Z_STRLEN_PP(item) = new_value_len;
        }
        efree(new_value);
    }

    return ZEND_HASH_APPLY_KEEP;
}

static void parse_cookies(const char *cookie, zval **array_ptr TSRMLS_DC)
{
    char *str, *var, *val, *strtok_buf = NULL;
    str = (char *) estrdup(cookie);
    var = php_strtok_r(str, ";\0", &strtok_buf);
    while (var) {

        val = strchr(var, '=');

        /* Remove leading spaces from cookie names,
           needed for multi-cookie header where ; can be
           followed by a space */
        while (isspace(*var)) {
            var++;
        }

        if (var == val || *var == '\0') {
            goto next_cookie;
        }

        if (val) { /* have a value */
            int val_len;
            unsigned int new_val_len;

            *val++ = '\0';
            php_url_decode(var, strlen(var));
            val_len = php_url_decode(val, strlen(val));
            val = estrndup(val, val_len);
            add_assoc_stringl(*array_ptr, var, val, val_len, 1);
            efree(val);
        } else {
            int val_len;
            unsigned int new_val_len;

            php_url_decode(var, strlen(var));
            val_len = 0;
            val = STR_EMPTY_ALLOC();
            add_assoc_stringl(*array_ptr, var, val, val_len, 1);
            efree(val);
        }
next_cookie:
        var = php_strtok_r(NULL, ";\0", &strtok_buf);
    }

    efree(str);
}

static void request_handler(struct evhttp_request *req, void *arg)
{
    TSRMLS_FETCH();

    if (request_counter_used) {
        if (request_counter == (LONG_MAX - 1)) {
            request_counter = 0;
        }
        request_counter++;
    }

    zval *zrequest, *args[2];
    struct php_can_server *server = (struct php_can_server*)arg;
    struct php_can_server_request *request;
    struct php_can_server_router *router;
    struct php_can_server_route *route = NULL;
    const char *cookie = NULL, *content_type = NULL, *content_length = NULL;
    long content_len = 0, buffer_len = 0;
    zval retval, *params;
    struct timeval tp = {0};
    long routeIndex = -1;
    
    struct evbuffer *buffer = evbuffer_new();
    
    // create request object
    MAKE_STD_ZVAL(zrequest);
    object_init_ex(zrequest, ce_can_server_request);
    Z_SET_REFCOUNT_P(zrequest, 1);
    request = (struct php_can_server_request *)zend_object_store_get_object(zrequest TSRMLS_CC);
    request->req = req;
    
    // set request time
    if(gettimeofday(&tp, NULL) == 0 ) {
        request->time = (double)(tp.tv_sec + tp.tv_usec / 1000000.00);
    }

    const char * uri_path = evhttp_uri_get_path(req->uri_elems);
    if (uri_path == NULL) {
        // Bad request
        request->response_code = 400;
        spprintf(&request->error, 0, "Cannot determine path of the uri");
        
    } else {

        MAKE_STD_ZVAL(params);
        array_init(params);
        
        // try to find route handler
        router = (struct php_can_server_router *)zend_object_store_get_object(server->router TSRMLS_CC);
        char *method = php_can_method_name(req->type);
        zval **method_routes;
        if (FAILURE != zend_hash_find(Z_ARRVAL_P(router->method_routes), method, strlen(method) + 1, (void **)&method_routes)) {
            zval **item;
            if (FAILURE != zend_hash_find(Z_ARRVAL_PP(method_routes), uri_path, strlen(uri_path) + 1, (void **)&item)) {
                // static route
                routeIndex = Z_LVAL_PP(item);
            } else {
                // dynamic routes, apply regexp to the URI
                PHP_CAN_FOREACH(*method_routes, item) {
                    if (strkey[0] == '\1') {
                        pcre_cache_entry *pce;
                        // TODO: cache compiled pce
                        if (NULL != (pce = pcre_get_compiled_regex_cache(strkey, strlen(strkey) TSRMLS_CC))) {
                            zval *subpats = NULL;
                            zval *res = NULL;
                            ALLOC_INIT_ZVAL(subpats);
                            ALLOC_INIT_ZVAL(res);
                            php_pcre_match_impl(pce, (char *)uri_path, strlen(uri_path), res, subpats, 0, 0, 0, 0 TSRMLS_CC);
                            if(Z_LVAL_P(res) > 0) {
                                routeIndex = Z_LVAL_PP(item);
                                zval **match;
                                PHP_CAN_FOREACH(subpats, match) {
                                    if (keytype == HASH_KEY_IS_STRING) {
                                        char * param = estrndup(Z_STRVAL_PP(match), Z_STRLEN_PP(match));
                                        int param_len = php_url_decode(param, Z_STRLEN_PP(match));
                                        add_assoc_stringl(params, strkey, param, param_len, 0);
                                    }
                                }
                            }
                            zval_ptr_dtor(&subpats);
                            zval_ptr_dtor(&res);
                            if (routeIndex >= 0) {
                                break;
                            }
                        }
                    }
                }
            }
        }

        zval **zroute;
        if (routeIndex == -1 || FAILURE == zend_hash_index_find(Z_ARRVAL_P(router->routes), routeIndex, (void **)&zroute)) {
            // there is definitely no such route for requested HTTP method
            // we search through route_methods to determine what HTTP response we send back
            zval **item;
            zend_bool found = 0;
            if (FAILURE != zend_hash_find(Z_ARRVAL_P(router->route_methods), uri_path, strlen(uri_path) + 1, (void **)&item)) {
                found = 1;
            } else {
                PHP_CAN_FOREACH(router->route_methods, item) {
                    if (strkey[0] == '\1') {
                        pcre_cache_entry *pce;
                        // TODO: cache compiled pce
                        if (NULL != (pce = pcre_get_compiled_regex_cache(strkey, strlen(strkey) TSRMLS_CC))) {
                            zval *subpats = NULL;
                            zval *res = NULL;
                            ALLOC_INIT_ZVAL(subpats);
                            ALLOC_INIT_ZVAL(res);
                            php_pcre_match_impl(pce, (char *)uri_path, strlen(uri_path), res, subpats, 0, 0, 0, 0 TSRMLS_CC);
                            if(Z_LVAL_P(res) > 0) {
                                // route exists, so we send 405
                                found = 1;
                            }
                            zval_ptr_dtor(&subpats);
                            zval_ptr_dtor(&res);
                            if (found) {
                                break;
                            }
                        }
                    }
                }
            }
            request->response_code = found ? 405 : 404;
            spprintf(&request->error, 0, "Cannot determine route for the path '%s'", uri_path);
            
        } else {
            
            if (instanceof_function(Z_OBJCE_PP(zroute), ce_can_server_websocket_route TSRMLS_CC)) {
                
                server_websocket_route_handle_request(*zroute, zrequest, params TSRMLS_CC);
                
            } else {
            
                // set route
                route = (struct php_can_server_route *)zend_object_store_get_object(*zroute TSRMLS_CC);

                // check if we must cast params
                if (zend_hash_num_elements(Z_ARRVAL_P(route->casts))) {
                    zval **item, **param;
                    PHP_CAN_FOREACH(route->casts, item) {
                        if (FAILURE != zend_hash_find(Z_ARRVAL_P(params), strkey, strlen(strkey) + 1, (void **)&param)) {
                            if (Z_LVAL_PP(item) == IS_LONG) {
                                convert_to_long_ex(param);
                            } else if (Z_LVAL_PP(item) == IS_DOUBLE) {
                                convert_to_double_ex(param);
                            } else if (Z_LVAL_PP(item) == IS_PATH) {
                                if (CHECK_ZVAL_NULL_PATH(*param)) {
                                    request->response_code = 400;
                                    spprintf(&request->error, 0, "Detected invalid characters in the URI.");
                                    break;
                                }
                            }
                        }
                    }    
                }

                // parse cookies
                cookie = evhttp_find_header(request->req->input_headers, "Cookie");
                if (cookie != NULL) {
                    MAKE_STD_ZVAL(request->cookies);
                    array_init(request->cookies);
                    parse_cookies(cookie, &request->cookies TSRMLS_CC);
                    // remove null bytes from cookies
                    zend_hash_apply(Z_ARRVAL_P(request->cookies), (apply_func_t) cleanUp TSRMLS_CC);
                }

                // set query and parse GET parameters
                request->uri = estrdup(uri_path);
                const char *query = evhttp_uri_get_query(req->uri_elems);
                if (query != NULL) {
                    request->query = estrdup(query);
                    MAKE_STD_ZVAL(request->get);
                    array_init(request->get);
                    char *q = estrdup(query); // will be freed within php_default_treat_data()
                    php_default_treat_data(PARSE_STRING, q, request->get TSRMLS_CC);
                    // remove null bytes from get params
                    zend_hash_apply(Z_ARRVAL_P(request->get), (apply_func_t) cleanUp TSRMLS_CC);
                }

                // parse POST parameters
                if (request->req->type == EVHTTP_REQ_POST) {

                    buffer_len = EVBUFFER_LENGTH(request->req->input_buffer);
                    content_length = evhttp_find_header(request->req->input_headers, "Content-Length");
                    if (content_length != NULL) {
                        content_len = atol(content_length);
                    }

                    if (buffer_len > content_len) {
                        request->response_code = 400;
                        spprintf(&request->error, 0, "Actual POST length %ld does not match Content-Length %ld", 
                                buffer_len, content_len);
                    } else {
                        content_type = evhttp_find_header(request->req->input_headers, "Content-Type");
                        if (content_type != NULL) {
                            MAKE_STD_ZVAL(request->post);
                            array_init(request->post);
                            if (NULL != strstr(content_type, "multipart/form-data")) {
                                php_can_parse_multipart(content_type, request->req->input_buffer, request->post, &request->files TSRMLS_CC);
                            } else if (NULL != strstr(content_type, "application/x-www-form-urlencoded")) {
                                php_default_treat_data(PARSE_STRING,
                                    estrndup(EVBUFFER_DATA( request->req->input_buffer ), buffer_len),
                                    request->post TSRMLS_CC
                                );
                            }
                            // remove null bytes from post params
                            zend_hash_apply(Z_ARRVAL_P(request->post), (apply_func_t) cleanUp TSRMLS_CC);
                        }
                    }
                }
            
                if (request->response_code == 0) {

                    // call handler
                    args[0] = zrequest;
                    args[1] = params;

                    Z_ADDREF_P(args[0]);
                    Z_ADDREF_P(args[1]);

                    if (call_user_function(EG(function_table), NULL, route->handler, &retval, 2, args TSRMLS_CC) == SUCCESS) {
                        if (request->status == PHP_CAN_SERVER_RESPONSE_STATUS_NONE) {
                            if (request->response_code == 0) {
                                request->response_code = 200;
                            }
                            if (request->response_code >= 200 && request->response_code < 300) {
                                if (Z_TYPE(retval) == IS_STRING) {
                                    if (Z_STRLEN(retval) > 0) {
                                        request->response_len = Z_STRLEN(retval);
                                        evbuffer_add(buffer, Z_STRVAL(retval), Z_STRLEN(retval));
                                    }
                                } else if (Z_TYPE(retval) == IS_NULL) {
                                    // empty response
                                } else {

                                    if (Z_TYPE(retval) == IS_OBJECT &&  instanceof_function(Z_OBJCE(retval), ce_can_HTTPForward TSRMLS_CC)) {

                                        zval *url = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), 
                                                "url", sizeof("url")-1, 1 TSRMLS_CC);
                                        zval *headers = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), 
                                                "headers", sizeof("headers")-1, 1 TSRMLS_CC);
                                        zval *callback = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), 
                                                "callback", sizeof("callback")-1, 1 TSRMLS_CC);
                                        forward_request((const char *)Z_STRVAL_P(url), zrequest, server, headers, callback);

                                    } else {

    #ifdef HAVE_JSON
                                        // check for existance of Content-Type response header and it's value
                                        // if the value is ``application/json`` we will try to JSON encode it
                                        int foundHeader = 0;
                                        const char *contentType =evhttp_find_header(req->output_headers, "Content-Type");
                                        if (contentType && strcmp(contentType, "application/json") == 0) {
                                            foundHeader = 1;
                                        }

                                        zend_class_entry **cep;
                                        if (Z_TYPE(retval) == IS_OBJECT 
                                                && zend_lookup_class("\\JsonSerializable", sizeof("\\JsonSerializable") - 1, &cep TSRMLS_CC) == SUCCESS
                                                && instanceof_function(Z_OBJCE(retval), *cep TSRMLS_CC)
                                        ) {
                                            // implements JsonSerializable, so just json_encode it
                                            smart_str encoded = {0};
                                            php_json_encode(&encoded, &retval, 0 TSRMLS_CC);
                                            if (foundHeader || -1 != evhttp_add_header(request->req->output_headers, "Content-Type", 
                                                    "application/json")) {
                                                request->response_len = encoded.len;
                                                evbuffer_add(buffer, encoded.c, encoded.len);
                                            }
                                            smart_str_free(&encoded);

                                        } else if (foundHeader) {
                                            smart_str encoded = {0};
                                            php_json_encode(&encoded, &retval, 0 TSRMLS_CC);
                                            request->response_len = encoded.len;
                                            evbuffer_add(buffer, encoded.c, encoded.len);
                                            smart_str_free(&encoded);
                                        }
    #endif

                                        if (request->response_len == 0) {
                                            request->response_code = 500;
                                            spprintf(&request->error, 0, "Request handler must return a string instead of %s", 
                                                Z_TYPE(retval) == IS_ARRAY ? "array" : 
                                                    Z_TYPE(retval) == IS_OBJECT ? "object" :
                                                        Z_TYPE(retval) == IS_LONG ? "integer" :
                                                            Z_TYPE(retval) == IS_DOUBLE ? "double" :
                                                                Z_TYPE(retval) == IS_BOOL ? "boolean‚" :
                                                                    Z_TYPE(retval) == IS_RESOURCE ? "resource" : "unknown"
                                            );
                                        }
                                    }
                                }
                            }
                        }
                        zval_dtor(&retval);
                    }
                    Z_DELREF_P(args[0]);
                    Z_DELREF_P(args[1]);
                }
            }
        }
        zval_ptr_dtor(&params);
    }
    
    if(EG(exception)) {
        if (instanceof_function(Z_OBJCE_P(EG(exception)), ce_can_HTTPError TSRMLS_CC)) {
            
            zval *code = NULL, *error = NULL;
            code  = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "code", sizeof("code")-1, 1 TSRMLS_CC);
            error = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "message", sizeof("message")-1, 1 TSRMLS_CC);
            request->response_code = code ? Z_LVAL_P(code) : 500;
            spprintf(&request->error, 0, "%s", error ? Z_STRVAL_P(error) : "Unknown");
            
        } else if (instanceof_function(Z_OBJCE_P(EG(exception)), ce_can_HTTPForward TSRMLS_CC)) {
            
            zval *url = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), 
                    "url", sizeof("url")-1, 1 TSRMLS_CC);
            zval *headers = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), 
                "headers", sizeof("headers")-1, 1 TSRMLS_CC);
            zval *callback = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), 
                "callback", sizeof("callback")-1, 1 TSRMLS_CC);
            forward_request((const char *)Z_STRVAL_P(url), zrequest, server, headers, callback);
            
        } else {
            zval *file = NULL, *line = NULL, *error = NULL;
            file = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "file", sizeof("file")-1, 1 TSRMLS_CC);
            line = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "line", sizeof("line")-1, 1 TSRMLS_CC);
            error = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "message", sizeof("message")-1, 1 TSRMLS_CC);
            request->response_code = 500;
            spprintf(&request->error, 0, "Uncaught exception '%s' within request handler thrown in %s on line %d \"%s\"", 
                    Z_OBJCE_P(EG(exception))->name,
                    file ? Z_STRVAL_P(file) : NULL,
                    line ? (int)Z_LVAL_P(line) : 0,
                    error ? Z_STRVAL_P(error) : ""
            );
        }
        zend_clear_exception(TSRMLS_C);
    }
    
    int write_log = 0;
    if (request->status == PHP_CAN_SERVER_RESPONSE_STATUS_NONE) {
        // send response
        evhttp_send_reply(request->req, request->response_code, NULL, buffer);
        write_log = 1;
    } else if (request->status == PHP_CAN_SERVER_RESPONSE_STATUS_SENDING) {
        // stop sending unfinished chunk response
        evhttp_send_reply_end(request->req);
        write_log = 1;
    } else if (request->status == PHP_CAN_SERVER_RESPONSE_STATUS_SENT) {
        write_log = 1;
    }
    
    evbuffer_free(buffer);

    if (server->logformat_len && write_log) {
        struct php_can_server_logentry *logentry;
        LOGENTRY_CTOR(logentry, request);
        LOGENTRY_LOG(logentry, server, request_counter);
        LOGENTRY_DTOR(logentry);
    }

    zval_ptr_dtor(&zrequest);
}

/**
 * Constructor
 *
 *
 */
static PHP_METHOD(CanServer, __construct)
{
    zval *addr, *port, *logformat = NULL, *zlogfile = NULL;
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "zz|zz", &addr, &port, &logformat, &zlogfile)
        || Z_TYPE_P(addr) != IS_STRING
        || Z_STRLEN_P(addr) == 0
        || Z_TYPE_P(port) != IS_LONG
        || Z_LVAL_P(port) < 1
        || (logformat && (Z_TYPE_P(logformat) != IS_STRING || Z_STRLEN_P(logformat) == 0))
        || (zlogfile && Z_TYPE_P(zlogfile) != IS_RESOURCE)
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $ip, integer $port[, string $log_format[, string $log_handler]])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server *server = (struct php_can_server*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    if (server->http) {
        /* called __construct() twice, bail out */
        return;
    }

    CAN_G(can_event_base) = event_init();

    // try to bind server on given ip and port
    if ((server->http = evhttp_new(CAN_G(can_event_base))) == NULL
           || (evhttp_bind_socket(server->http, Z_STRVAL_P(addr), Z_LVAL_P(port))) < 0
    ) {
        php_can_throw_exception(
            ce_can_ServerBindingException TSRMLS_CC,
            "Error binding server on %s port %ld",
            Z_STRVAL_P(addr), Z_LVAL_P(port)
        );
        event_base_free(CAN_G(can_event_base));
        CAN_G(can_event_base) = NULL;
        return;
    }

    // allow all supported http methods
    evhttp_set_allowed_methods(server->http,
        EVHTTP_REQ_GET|
        EVHTTP_REQ_POST|
        EVHTTP_REQ_HEAD|
        EVHTTP_REQ_PUT|
        EVHTTP_REQ_DELETE|
        EVHTTP_REQ_OPTIONS|
        EVHTTP_REQ_TRACE|
        EVHTTP_REQ_CONNECT|
        EVHTTP_REQ_PATCH
    );

    // set timeout to a reasonably short value for performance
    evhttp_set_timeout(server->http, 10);

    server->addr = estrndup(Z_STRVAL_P(addr), Z_STRLEN_P(addr));
    server->port = Z_LVAL_P(port);
    server->running = 0;
    if (logformat != NULL)  {
        server->logformat = estrndup(Z_STRVAL_P(logformat), Z_STRLEN_P(logformat));
        server->logformat_len = Z_STRLEN_P(logformat);
    }
    if (zlogfile != NULL) {
        if (Z_REFCOUNT_P(zlogfile) == 1) {
            zval_add_ref(&zlogfile);
        }
        php_stream_from_zval_no_verify(server->logfile, &zlogfile);
    }
    if (logformat != NULL) {

        if (FAILURE != php_can_strpos(server->logformat, "x-reqnum", 0)) {
            request_counter_used = 1;
        }
        
        double now;
        SETNOW(now);
        char *msg = NULL,
             *date = php_format_date("Y-m-d H:i:s", sizeof("Y-m-d H:i:s"), (long)now, 1 TSRMLS_CC);
        int len = spprintf(&msg, 0,
            "#Version: 1.0\n#Date: %s\n#Software: %s, version %s\n#"
            "Remark: Server binded to %s on port %d\n#"
            "Remark: W3C Extended Log File Format\n#Fields: %s",
            date, PHP_CAN_SERVER_NAME, PHP_CAN_VERSION, server->addr, server->port, server->logformat
        );
        WRITELOG(server, msg, len);
        efree(msg);
        efree(date);
    }
}

/**
 * Start server
 *
 *
 */
static PHP_METHOD(CanServer, start)
{
    zval *zrouter = NULL;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "O", &zrouter, ce_can_server_router)) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(Router $router)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server *server = (struct php_can_server*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    zval_add_ref(&zrouter);
    server->router = zrouter;
    server->running = 1;

    evhttp_set_gencb(server->http, request_handler, (void*)server);

    event_base_dispatch(CAN_G(can_event_base));
}

/**
 * Stop server
 */
static PHP_METHOD(CanServer, stop)
{
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC, "")) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(void)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server *server = (struct php_can_server*) zend_object_store_get_object(getThis() TSRMLS_CC);

    if (server->running == 0) {
        php_can_throw_exception(
            ce_can_InvalidOperationException TSRMLS_CC,
            "Server is not running"
        );
        return;
    }

    if (event_base_loopbreak(CAN_G(can_event_base)) == 0) {
        event_base_free(CAN_G(can_event_base));
        CAN_G(can_event_base) = NULL;
        server->running = 0;
        RETURN_TRUE;
    } else {
        RETURN_FALSE;
    }
}

static zend_function_entry server_methods[] = {
    PHP_ME(CanServer, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServer, start,       NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServer, stop,        NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_init(TSRMLS_D)
{
    memcpy(&server_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_obj_handlers.clone_obj = NULL;

    // class \Can\Server
    PHP_CAN_REGISTER_CLASS(
        &ce_can_server,
        ZEND_NS_NAME(PHP_CAN_NS, "Server"),
        server_ctor,
        server_methods
    );
}

PHP_MINIT_FUNCTION(can_server)
{
    server_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(can_server)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(can_server)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(can_server)
{
    return SUCCESS;
}
