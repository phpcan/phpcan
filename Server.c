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

static long request_counter = 0;

zend_class_entry *ce_buddel_server;
static zend_object_handlers server_obj_handlers;

void php_buddel_parse_multipart(const char* content_type, struct evbuffer* buffer, zval* post, zval** files TSRMLS_DC);
static void server_dtor(void *object TSRMLS_DC);

static zend_object_value server_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_buddel_server *s;
    zend_object_value retval;

    s = ecalloc(1, sizeof(*s));
    s->logformat = NULL;
    s->logfile = NULL;
    s->router = NULL;
    zend_object_std_init(&s->std, ce TSRMLS_CC);

    retval.handle = zend_objects_store_put(s,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_obj_handlers;
    return retval;
}

static void server_dtor(void *object TSRMLS_DC)
{
    struct php_buddel_server *s = (struct php_buddel_server*)object;

    if (s->http) {
        free(s->http);
        s->http = NULL;
    }

    zend_objects_store_del_ref(&s->refhandle TSRMLS_CC);
    zend_object_std_dtor(&s->std TSRMLS_CC);

    if (s->addr) {
        efree(s->addr);
        s->addr = NULL;
    }

    if (s->logformat) {
        efree(s->logformat);
        s->logformat = NULL;
    }

    if (s->logfile) {
        php_stream_close(s->logfile);
        s->logfile = NULL;
    }
    
    if (s->router) {
        zval_ptr_dtor(&s->router);
    }
    efree(s);
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

    request_counter++;

    zval *request, *args[2];
    struct php_buddel_server *s = (struct php_buddel_server*)arg;
    struct php_buddel_server_request *r;
    struct php_buddel_server_router *router;
    struct php_buddel_server_route *route = NULL;
    const char *cookie = NULL, *content_type = NULL, *content_length = NULL;
    long content_len = 0, buffer_len = 0;
    zval retval, *params;
    struct timeval tp = {0};
    ulong routeIndex = -1;
    
    struct evbuffer *buffer = evbuffer_new();
    
    // create request object
    MAKE_STD_ZVAL(request);
    object_init_ex(request, ce_buddel_server_request);
    Z_SET_REFCOUNT_P(request, 1);
    r = (struct php_buddel_server_request *)zend_object_store_get_object(request TSRMLS_CC);
    r->req = req;
    
    // set request time
    if(gettimeofday(&tp, NULL) == 0 ) {
        r->time = (double)(tp.tv_sec + tp.tv_usec / 1000000.00);
    }

    const char * uri_path = evhttp_uri_get_path(req->uri_elems);
    if (uri_path == NULL) {
        // Bad request
        r->response_status = 400;
        spprintf(&r->error, 0, "Cannot determine path of the uri");
        
    } else {

        MAKE_STD_ZVAL(params);
        array_init(params);
        
        // try to find route handler
        router = (struct php_buddel_server_router *)zend_object_store_get_object(s->router TSRMLS_CC);
        char *method = typeToMethod(req->type);
        zval **tmp;
        if (FAILURE != zend_hash_find(Z_ARRVAL_P(router->methods), method, strlen(method) + 1, (void **)&tmp)) {
            zval **item;
            if (FAILURE != zend_hash_find(Z_ARRVAL_PP(tmp), uri_path, strlen(uri_path) + 1, (void **)&item)) {
                routeIndex = Z_LVAL_PP(item);
            } else {
                PHP_BUDDEL_FOREACH(*tmp, item) {
                    if (strkey[0] == '\1') {
                        pcre_cache_entry *pce;
                        if (NULL != (pce = pcre_get_compiled_regex_cache(strkey, strlen(strkey) TSRMLS_CC))) {
                            zval *subpats = NULL;
                            zval *res = NULL;
                            ALLOC_INIT_ZVAL(subpats);
                            ALLOC_INIT_ZVAL(res);
                            php_pcre_match_impl(pce, (char *)uri_path, strlen(uri_path), res, subpats, 0, 0, 0, 0 TSRMLS_CC);
                            if(Z_LVAL_P(res) > 0) {
                                routeIndex = Z_LVAL_PP(item);
                                zval **match;
                                PHP_BUDDEL_FOREACH(subpats, match) {
                                    if (keytype == HASH_KEY_IS_STRING) {
                                        add_assoc_stringl(params, strkey, Z_STRVAL_PP(match), Z_STRLEN_PP(match), 1);
                                    }
                                }
                            }
                            zval_ptr_dtor(&subpats);
                            zval_ptr_dtor(&res);
                            if (routeIndex > -1) {
                                break;
                            }
                        }
                    }
                }
            }
        }

        zval **zroute;
        if (routeIndex == -1 || FAILURE == zend_hash_index_find(Z_ARRVAL_P(router->routes), routeIndex, (void **)&zroute)) {
            
            r->response_status = 404;
            spprintf(&r->error, 0, "Cannot determine route for the path '%s'", uri_path);
            
        } else {

            // set route
            route = (struct php_buddel_server_route *)zend_object_store_get_object(*zroute TSRMLS_CC);

            // parse cookies
            cookie = evhttp_find_header(r->req->input_headers, "Cookie");
            if (cookie != NULL) {
                MAKE_STD_ZVAL(r->cookies);
                array_init(r->cookies);
                parse_cookies(cookie, &r->cookies TSRMLS_CC);
            }

            // set query and parse GET parameters
            r->uri = estrdup(uri_path);
            const char *query = evhttp_uri_get_query(req->uri_elems);
            if (query != NULL) {
                r->query = estrdup(query);
                MAKE_STD_ZVAL(r->get);
                array_init(r->get);
                char *q = estrdup(query); // will be freed within php_default_treat_data()
                php_default_treat_data(PARSE_STRING, q, r->get TSRMLS_CC);
            }

            // parse POST parameters
            if (r->req->type == EVHTTP_REQ_POST) {

                buffer_len = EVBUFFER_LENGTH(r->req->input_buffer);
                content_length = evhttp_find_header(r->req->input_headers, "Content-Length");
                if (content_length != NULL) {
                    content_len = atol(content_length);
                }

                if (buffer_len > content_len) {
                    r->response_status = 400;
                    spprintf(&r->error, 0, "Actual POST length %ld does not match Content-Length %ld", 
                            buffer_len, content_len);
                } else {
                    content_type = evhttp_find_header(r->req->input_headers, "Content-Type");
                    if (content_type != NULL) {
                        MAKE_STD_ZVAL(r->post);
                        array_init(r->post);
                        if (NULL != strstr(content_type, "multipart/form-data")) {
                             php_buddel_parse_multipart(content_type, r->req->input_buffer, r->post, &r->files TSRMLS_CC);
                        } else if (NULL != strstr(content_type, "application/x-www-form-urlencoded")) {
                            php_default_treat_data(PARSE_STRING,
                                estrndup(EVBUFFER_DATA( r->req->input_buffer ), buffer_len),
                                r->post TSRMLS_CC
                            );
                        }
                    }
                }
            }
            
            if (r->response_status == 0) {
                
                // call handler
                args[0] = request;
                args[1] = params;

                Z_ADDREF_P(args[0]);
                Z_ADDREF_P(args[1]);

                if (call_user_function(EG(function_table), NULL, route->handler, &retval, 2, args TSRMLS_CC) == SUCCESS) {
                    if (r->status != PHP_BUDDEL_SERVER_RESPONSE_STATUS_SENT) {
                        if (r->response_status == 0) {
                            r->response_status = 200;
                        }
                        if (r->response_status >= 200 && r->response_status < 300) {
                            if (Z_TYPE(retval) == IS_STRING) {
                                if (Z_STRLEN(retval) > 0) {
                                    r->response_len = Z_STRLEN(retval);
                                    evbuffer_add(buffer, Z_STRVAL(retval), Z_STRLEN(retval));
                                }
                            } else if (Z_TYPE(retval) == IS_NULL) {
                                // empty response
                            } else {
                                // non-scalar
                                r->response_status = 500;
                                spprintf(&r->error, 0, "Request handler must return a string instead of %s", 
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
                    zval_dtor(&retval);
                }
                Z_DELREF_P(args[0]);
                Z_DELREF_P(args[1]);
            }
        }
        zval_ptr_dtor(&params);
    }
    
    if(EG(exception)) {
        if (instanceof_function(Z_OBJCE_P(EG(exception)), ce_buddel_HTTPError TSRMLS_CC)) {
            zval *code = NULL, *error = NULL;
            code  = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "code", sizeof("code")-1, 1 TSRMLS_CC);
            error = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "message", sizeof("message")-1, 1 TSRMLS_CC);
            r->response_status = code ? Z_LVAL_P(code) : 500;
            spprintf(&r->error, 0, "%s", error ? Z_STRVAL_P(error) : "Unknown");
        } else {
            zval *file = NULL, *line = NULL, *error = NULL;
            file = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "file", sizeof("file")-1, 1 TSRMLS_CC);
            line = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "line", sizeof("line")-1, 1 TSRMLS_CC);
            r->response_status = 500;
            spprintf(&r->error, 0, "Uncaught exception '%s' within request handler thrown in %s on line %d %s", 
                    Z_OBJCE_P(EG(exception))->name,
                    file ? Z_STRVAL_P(file) : NULL,
                    line ? (int)Z_LVAL_P(line) : 0,
                    error ? Z_STRVAL_P(error) : ""
            );
        }
        zend_clear_exception(TSRMLS_C);
    }
    
    if (r->status != PHP_BUDDEL_SERVER_RESPONSE_STATUS_SENT) {
        // send response
        evhttp_send_reply(r->req, r->response_status, NULL, buffer);
    }
    
    evbuffer_free(buffer);

    struct php_buddel_server_logentry *logentry;
    LOGENTRY_CTOR(logentry, r);

    zval_ptr_dtor(&request);

    if (s->logformat_len) {
        LOGENTRY_LOG(logentry, s, request_counter);
    }

    LOGENTRY_DTOR(logentry);
}

/**
 * Constructor
 *
 *
 */
static PHP_METHOD(BuddelServer, __construct)
{
    zval *object = getThis();
    struct php_buddel_server *s;
    char *addr, *logformat = NULL;
    int addr_len, logformat_len = 0, num_args = ZEND_NUM_ARGS();
    long port;
    zval *zlogfile = NULL;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, num_args TSRMLS_CC,
            "sl|sr", &addr, &addr_len, &port, &logformat, &logformat_len, &zlogfile)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $ip, integer $port[, string $log_format[, resource $log_handler]])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    s = (struct php_buddel_server*)zend_object_store_get_object(object TSRMLS_CC);

    if (s->http) {
        /* called __construct() twice, bail out */
        return;
    }

    event_init();

    // try to bind server on given ip and port
    if ((s->http = evhttp_start(addr, port)) == NULL) {
        php_buddel_throw_exception(
            ce_buddel_ServerBindingException TSRMLS_CC,
            "Error binding server on %s port %d",
            addr, (int)port
        );
        return;
    }

    // allow all known http methods
    evhttp_set_allowed_methods(s->http,
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
    evhttp_set_timeout(s->http, 10);

    s->addr = estrndup(addr, addr_len);
    s->port = port;
    s->running = 0;
    if (logformat_len > 0) {
        s->logformat = estrndup(logformat, logformat_len);
        s->logformat_len = logformat_len;
    }
    if (zlogfile != NULL) {
        php_stream_from_zval_no_verify(s->logfile, &zlogfile);
    }
    if (logformat_len > 0) {
        double now;
        SETNOW(now);
        char *msg = NULL,
             *date = php_format_date("Y-m-d H:i:s", sizeof("Y-m-d H:i:s"), (long)now, 1 TSRMLS_CC);
        int len = spprintf(&msg, 0,
            "#Version: 1.0\n#Date: %s\n#Software: %s, version %s\n#"
            "Remark: Server binded to %s on port %d\n#"
            "Remark: W3C Extended Log File Format\n#Fields: %s",
            date, PHP_BUDDEL_SERVER_NAME, PHP_BUDDEL_SERVER_VERSION, addr, (int)port, s->logformat
        );
        WRITELOG(s, msg, len);
        efree(msg);
        efree(date);
    }
}

/**
 * Start server
 *
 *
 */
static PHP_METHOD(BuddelServer, start)
{
    zval *zrouter = NULL;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "O", &zrouter, ce_buddel_server_router)) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(Routing $router)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_buddel_server *s = (struct php_buddel_server*)
        zend_object_store_get_object(getThis() TSRMLS_CC);

    zval_add_ref(&zrouter);
    s->router = zrouter;
    s->running = 1;

    evhttp_set_gencb(s->http, request_handler, (void*)s);

    event_dispatch();
}

/**
 * Stop server
 */
static PHP_METHOD(BuddelServer, stop)
{
    struct php_buddel_server *s;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC, "")) {
        const char *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_buddel_throw_exception(
            ce_buddel_InvalidParametersException TSRMLS_CC,
            "%s%s%s(void)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    s = (struct php_buddel_server*) zend_object_store_get_object(getThis() TSRMLS_CC);

    if (s->running == 0) {
        php_buddel_throw_exception(
            ce_buddel_InvalidOperationException TSRMLS_CC,
            "Server is not running"
        );
        return;
    }

    if (event_loopbreak() == 0) {
        s->running = 0;
        RETURN_TRUE;
    } else {
        RETURN_FALSE;
    }
}

static zend_function_entry server_methods[] = {
    PHP_ME(BuddelServer, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServer, start,       NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(BuddelServer, stop,        NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_init(TSRMLS_D)
{
    memcpy(&server_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_obj_handlers.clone_obj = NULL;

    // class \Buddel\Server
    PHP_BUDDEL_REGISTER_CLASS(
        &ce_buddel_server,
        ZEND_NS_NAME(PHP_BUDDEL_NS, "Server"),
        server_ctor,
        server_methods
    );
}

PHP_MINIT_FUNCTION(buddel_server)
{
    server_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(buddel_server)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(buddel_server)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(buddel_server)
{
    return SUCCESS;
}
