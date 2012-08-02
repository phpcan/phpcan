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

zend_class_entry *ce_can_server_websocket_route;
static zend_object_handlers server_websocket_route_obj_handlers;

static void server_websocket_route_dtor(void *object TSRMLS_DC);

static zend_object_value server_websocket_route_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_can_server_route *route;
    zend_object_value retval;

    route = ecalloc(1, sizeof(*route));
    zend_object_std_init(&route->std, ce TSRMLS_CC);
    route->handler = NULL;
    route->methods = PHP_CAN_SERVER_ROUTE_METHOD_GET;
    route->regexp = NULL;
    route->route = NULL;
    route->casts = NULL;
    retval.handle = zend_objects_store_put(route,       
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            server_websocket_route_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &server_websocket_route_obj_handlers;
    return retval;
}

static void server_websocket_route_dtor(void *object TSRMLS_DC)
{
    struct php_can_server_route *route = (struct php_can_server_route*)object;

    if (route->handler) {
        zval_ptr_dtor(&route->handler);
    }
    
    if (route->regexp) {
        efree(route->regexp);
        route->regexp = NULL;
    }

    if (route->route) {
        efree(route->route);
        route->route = NULL;
    }
    
    if (route->casts) {
        zval_ptr_dtor(&route->casts);
    }

    zend_objects_store_del_ref(&route->refhandle TSRMLS_CC);
    zend_object_std_dtor(&route->std TSRMLS_CC);
    efree(route);

}


static char *gen_hash(const char *key1, const char *key2, const char *body)
{
    unsigned int spaces = 0, len;
    unsigned char buf[17];
    int i;
    const char * k;
    unsigned char *tmp;

    k = key1;
    tmp = buf;
    for(i=0; i < 2; ++i, k = key2, tmp = buf + 4) {
        unsigned long num = 0;
        unsigned int spaces = 0;
        char * end = (char*)k + strlen(k);
        for (; k != end; ++k) {
            spaces += (int)(*k == ' ');
            if (*k >= '0' && *k <= '9')
                num = num * 10 + (*k - '0');
        }
        num /= spaces;
        tmp[0] = (num & 0xff000000) >> 24;
        tmp[1] = (num & 0xff0000) >> 16;
        tmp[2] = (num & 0xff00) >> 8;
        tmp[3] = num & 0xff;
    }

    memcpy(buf + 8, body, 8);
    buf[16] = '\0';

    char md5str[33];
    PHP_MD5_CTX context;
    unsigned char digest[16];

    md5str[0] = '\0';
    PHP_MD5Init(&context);
    PHP_MD5Update(&context, buf, 16);
    PHP_MD5Final(digest, &context);
    make_digest_ex(md5str, digest, 16);
    char *retval = estrdup(md5str);
    return retval;
}


static void
websocket_read_cb(struct bufferevent *bufev, void *arg)
{
    php_printf("websocket_read_cb: length=%ld\n", EVBUFFER_LENGTH(bufev->input));
    
}

static void
websocket_write_cb(struct bufferevent *bufev, void *arg)
{
    php_printf("websocket_write_cb\n");
    bufferevent_enable(bufev, EV_READ);
}

static void
websocket_error_cb(struct bufferevent *bufev, short what, void *arg)
{
    php_printf("websocket_error_cb\n");
}

void server_websocket_route_handle_request(zval *zroute, zval *zrequest, zval *params TSRMLS_DC)
{
    struct php_can_server_request *request = (struct php_can_server_request *)
        zend_object_store_get_object(zrequest TSRMLS_CC);
    
    // check if it's valid WebSocket HTTP request
    if (request->req->type != EVHTTP_REQ_GET) {
        request->response_code = 405;
        spprintf(&request->error, 0, "Unsupported WebSocket request method");
        return;
    }

    const char *hdr_upgrade, *hdr_conn, *hdr_wskey, *hdr_wskey1, *hdr_wskey2, *hdr_origin, *hdr_wsver;
    char *body = NULL;
    
    if ((hdr_upgrade = evhttp_find_header(request->req->input_headers, "Upgrade")) == NULL
        || strcmp(hdr_upgrade, "websocket") != 0
    ) {
        request->response_code = 400;
        spprintf(&request->error, 0, "Invalid value of the WebSocket Upgrade request header: %s", hdr_upgrade);
        return;
    }

    if ((hdr_conn = evhttp_find_header(request->req->input_headers, "Connection")) == NULL
        || php_can_strpos((char *)hdr_conn, "Upgrade", 0) == FAILURE
    ) {
        request->response_code = 400;
        spprintf(&request->error, 0, "Missing \"Upgrade\" in the value of the WebSocket Connection request header");
        return;
    }

    if ((hdr_wsver = evhttp_find_header(request->req->input_headers, "Sec-WebSocket-Version")) == NULL
        || (strcmp(hdr_wsver, "7") != 0 && strcmp(hdr_wsver, "8") != 0 && strcmp(hdr_wsver, "13") != 0)
    ) {
        request->response_code = 400;
        spprintf(&request->error, 0, "Missing or unsupported value of the Sec-WebSocket-Version request header");
        return;
    } else {
        if (strcmp(hdr_wsver, "13") != 0) {
            hdr_origin = evhttp_find_header(request->req->input_headers, "Sec-Websocket-Origin");
        } else {
            hdr_origin = evhttp_find_header(request->req->input_headers, "Origin");
        }
        if (hdr_origin == NULL) {
            request->response_code = 400;
            spprintf(&request->error, 0, "Missing Origin request header");
            return;
        }
    }

    if ((hdr_wskey = evhttp_find_header(request->req->input_headers, "Sec-WebSocket-Key")) == NULL
        && ((hdr_wskey1 = evhttp_find_header(request->req->input_headers, "Sec-WebSocket-Key1")) == NULL
         || (hdr_wskey2 = evhttp_find_header(request->req->input_headers, "Sec-WebSocket-Key2")) == NULL)
    ) {
        request->response_code = 400;
        spprintf(&request->error, 0, "Missing Sec-WebSocket-Key request header");
        return;
    }
    
    if (Z_OBJCE_P(zroute) != ce_can_server_websocket_route) {
        
        zval *callback,  retval, *args[2];
        MAKE_STD_ZVAL(callback);
        ZVAL_STRING(callback, "onHandshake", 1);

        args[0] = zrequest;
        args[1] = params;

        Z_ADDREF_P(args[0]);
        Z_ADDREF_P(args[1]);

        if (call_user_function(EG(function_table), &zroute, callback, &retval, 2, args TSRMLS_CC) == SUCCESS) {
            zval_dtor(&retval);
        }
        Z_DELREF_P(args[0]);
        Z_DELREF_P(args[1]);

        zval_ptr_dtor(&callback);

        if(EG(exception)) {

            if (instanceof_function(Z_OBJCE_P(EG(exception)), ce_can_HTTPError TSRMLS_CC)) {

                zval *code = NULL, *error = NULL;
                code  = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "code", sizeof("code")-1, 1 TSRMLS_CC);
                error = zend_read_property(Z_OBJCE_P(EG(exception)), EG(exception), "message", sizeof("message")-1, 1 TSRMLS_CC);
                request->response_code = code ? Z_LVAL_P(code) : 500;
                spprintf(&request->error, 0, "%s", error ? Z_STRVAL_P(error) : "Unknown");

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
            return;
        }
    }
    
    
    if (hdr_wskey != NULL) {
        
        zval *zhash_func, hash_retval, *zhash_arg1, *zhash_arg2, *zhash_arg3, *hash_args[3];
        char *accept = NULL;

        MAKE_STD_ZVAL(zhash_func); ZVAL_STRING(zhash_func, "hash", 1);
        MAKE_STD_ZVAL(zhash_arg1); ZVAL_STRING(zhash_arg1, "sha1", 1);
        MAKE_STD_ZVAL(zhash_arg2);
        spprintf(&accept, 0, "%s%s", hdr_wskey, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        ZVAL_STRING(zhash_arg2, accept, 1);
        efree(accept);

        MAKE_STD_ZVAL(zhash_arg3);
        ZVAL_BOOL(zhash_arg3, 1);

        hash_args[0] = zhash_arg1;
        hash_args[1] = zhash_arg2;
        hash_args[2] = zhash_arg3;

        Z_ADDREF_P(hash_args[0]);
        Z_ADDREF_P(hash_args[1]);
        Z_ADDREF_P(hash_args[2]);

        if (call_user_function(EG(function_table), NULL, zhash_func, &hash_retval, 3, hash_args TSRMLS_CC) == SUCCESS) {
            char *base64_str = NULL;
            base64_str = (char *) php_base64_encode((unsigned char*)Z_STRVAL(hash_retval), Z_STRLEN(hash_retval), NULL);
            evhttp_add_header(request->req->output_headers, "Sec-WebSocket-Accept", base64_str);
            efree(base64_str);
            zval_dtor(&hash_retval);
        }

        Z_DELREF_P(hash_args[0]);
        Z_DELREF_P(hash_args[1]);
        Z_DELREF_P(hash_args[2]);
        
    } else if (hdr_wskey1 != NULL && hdr_wskey2 != NULL) {
        
        evhttp_add_header(request->req->output_headers, "Sec-WebSocket-Origin", hdr_origin);
        
        char *location = NULL;
        spprintf(&location, 0, "ws://%s%s", 
                evhttp_find_header(request->req->input_headers, "Host"),
                evhttp_uri_get_path(request->req->uri_elems));
        evhttp_add_header(request->req->output_headers, "Sec-WebSocket-Location", location);
        efree(location);
        
        body = gen_hash(hdr_wskey, hdr_wskey2, EVBUFFER_DATA(request->req->input_buffer)); 
        
    }
    
    request->response_code = 101;
    evhttp_add_header(request->req->output_headers, "Upgrade", "websocket");
    evhttp_add_header(request->req->output_headers, "Connection", "Upgrade");

    const char *ws_protocol = evhttp_find_header(request->req->input_headers, "Sec-WebSocket-Protocol");
    if (ws_protocol != NULL) {
        evhttp_add_header(request->req->output_headers, "Sec-WebSocket-Protocol", ws_protocol);
    }
    
    // get ownership of the request object, send response
    evhttp_request_own(request->req);

    struct evhttp_connection *evcon = evhttp_request_get_connection(request->req);
    struct bufferevent *bufev = evhttp_connection_get_bufferevent(evcon); 
    struct evbuffer *output = bufferevent_get_output(bufev);
    evbuffer_add_printf(output, "HTTP/1.1 101 Switching Protocols\r\n");

    // write headers
    struct evkeyval *header;
    for (header=((request->req->output_headers)->tqh_first); header; header=((header)->next.tqe_next)) {
        evbuffer_add_printf(output, "%s: %s\r\n", header->key, header->value);
    }
    evbuffer_add(output, "\r\n", 2);
    
    if (body != NULL) {
        evbuffer_add(output, body, strlen(body));
        efree(body);
    }

    bufferevent_enable(bufev, EV_WRITE);

    bufferevent_setcb(bufev,
        websocket_read_cb,
        NULL,
        websocket_error_cb,
        evcon
    );

    request->status = PHP_CAN_SERVER_RESPONSE_STATUS_SENT;
    
}

/**
 * Constructor
 */
static PHP_METHOD(CanServerWebSocketRoute, __construct)
{
    zval *uri = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z", &uri) 
            || Z_TYPE_P(uri) != IS_STRING
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $uri)",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }

    struct php_can_server_route *route = (struct php_can_server_route*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    MAKE_STD_ZVAL(route->casts);
    array_init(route->casts);
    
    if (FAILURE != php_can_strpos(Z_STRVAL_P(uri), "<", 0) && FAILURE != php_can_strpos(Z_STRVAL_P(uri), ">", 0)) {
        int i;
        for (i = 0; i < Z_STRLEN_P(uri); i++) {
            if (Z_STRVAL_P(uri)[i] != '<') {
                spprintf(&route->regexp, 0, "%s%c", route->regexp == NULL ? "" : route->regexp, Z_STRVAL_P(uri)[i]);
            } else {
                int y = php_can_strpos(Z_STRVAL_P(uri), ">", i);
                char *name = php_can_substr(Z_STRVAL_P(uri), i + 1, y - (i + 1));
                int pos = php_can_strpos(name, ":", 0);
                if (FAILURE != pos) {
                    char *var = php_can_substr(name, 0, pos);
                    char *filter = php_can_substr(name, pos + 1, strlen(name) - (pos + 1));
                    if (strcmp(filter, "int") == 0) {
                        spprintf(&route->regexp, 0, "%s(?<%s>%s)", route->regexp, var, "-?[0-9]+");
                        add_assoc_long(route->casts, var, IS_LONG);
                    } else if (0 == strcmp(filter, "float")) {
                        spprintf(&route->regexp, 0, "%s(?<%s>%s)", route->regexp, var, "-?[0-9.]+");
                        add_assoc_long(route->casts, var, IS_DOUBLE);
                    } else if (0 == strcmp(filter, "path")) {
                        spprintf(&route->regexp, 0, "%s(?<%s>%s)", route->regexp, var, ".+?");
                        add_assoc_long(route->casts, var, IS_PATH);
                    } else if (0 == (pos = php_can_strpos(filter, "re:", 0))) {
                        char *reg = php_can_substr(filter, pos + 3, strlen(filter) - (pos + 3));
                        spprintf(&route->regexp, 0, "%s(?<%s>%s)", route->regexp, var, reg);
                        efree(reg);
                    }
                    efree(filter);
                    efree(var);
                    
                } else {
                    spprintf(&route->regexp, 0, "%s(?<%s>[^/]+)", route->regexp, name);
                }
                efree(name);
                i = y;
            }
        }
        spprintf(&route->regexp, 0, "\1^%s$\1", route->regexp);
    }
    
    route->route = estrndup(Z_STRVAL_P(uri), Z_STRLEN_P(uri));

}

/**
 * Get URI
 */
static PHP_METHOD(CanServerWebSocketRoute, getUri)
{
    zval *as_regexp = NULL;
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "|z", &as_regexp) || (as_regexp && Z_TYPE_P(as_regexp) != IS_BOOL)) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s([bool $as_regexp])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    struct php_can_server_route *route = (struct php_can_server_route*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (as_regexp && Z_BVAL_P(as_regexp)) {
        if (route->regexp != NULL) {
            char *regexp = php_can_substr(route->regexp, 1, strlen(route->regexp) - 2);
            RETVAL_STRING(regexp, 0);
        } else {
            RETVAL_FALSE;
        }
    } else {
        RETVAL_STRING(route->route, 1);
    }
}

/**
 * Invoked on incoming clint handshake.
 * Override this method to check incoming request data 
 * or/and to inject additional response headers
 * @param Request instance
 * @param array uri arguments
 */
static PHP_METHOD(CanServerWebSocketRoute, onHandshake)
{

}

/**
 * Handle incoming messages
 * @param string incoming message
 * @return string outgoing message
 */
static PHP_METHOD(CanServerWebSocketRoute, onMessage) 
{

}

/**
 * Invoked when the WebSocket is closed.
 */
static PHP_METHOD(CanServerWebSocketRoute, onClose) 
{

}

static zend_function_entry server_websocket_route_methods[] = {
    PHP_ME(CanServerWebSocketRoute, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerWebSocketRoute, getUri,      NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanServerWebSocketRoute, onHandshake, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(CanServerWebSocketRoute, onMessage,   NULL, ZEND_ACC_PUBLIC)
    PHP_ME(CanServerWebSocketRoute, onClose,     NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void server_websocket_route_init(TSRMLS_D)
{
    memcpy(&server_websocket_route_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    server_websocket_route_obj_handlers.clone_obj = NULL;

    // class \Can\Server\WebSocketRoute extends \Can\Server\Route
    PHP_CAN_REGISTER_SUBCLASS(
        &ce_can_server_websocket_route,
        ce_can_server_route,
        ZEND_NS_NAME(PHP_CAN_SERVER_NS, "WebSocketRoute"),
        server_websocket_route_ctor,
        server_websocket_route_methods
    );
}

PHP_MINIT_FUNCTION(can_server_websocket_route)
{
    server_websocket_route_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(can_server_websocket_route)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(can_server_websocket_route)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(can_server_websocket_route)
{
    return SUCCESS;
}
