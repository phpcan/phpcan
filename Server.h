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

#ifndef CAN_SERVER_H
#define CAN_SERVER_H

#include "php.h"
#include "fopen_wrappers.h"
#include "ext/date/php_date.h"
#include "ext/standard/php_array.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_rand.h"
#include "ext/standard/md5.h"
#include "zend_interfaces.h"

#ifdef HAVE_JSON
#include "ext/json/php_json.h"
#endif

#include "ext/standard/base64.h"
#include "ext/standard/url.h"
#include "ext/pcre/php_pcre.h"
#include "php_variables.h"
#include "php_can.h"
#include "Exception.h"

#define PHP_CAN_SERVER_NAME "PHP Can HTTP Server"

#define PHP_CAN_SERVER_RESPONSE_STATUS_NONE    0
#define PHP_CAN_SERVER_RESPONSE_STATUS_SENDING 1
#define PHP_CAN_SERVER_RESPONSE_STATUS_SENT    2
#define PHP_CAN_SERVER_RESPONSE_STATUS_FORWARD 3

#define PHP_CAN_SERVER_ROUTE_METHOD_GET        1
#define PHP_CAN_SERVER_ROUTE_METHOD_POST       2
#define PHP_CAN_SERVER_ROUTE_METHOD_HEAD       4
#define PHP_CAN_SERVER_ROUTE_METHOD_PUT        8
#define PHP_CAN_SERVER_ROUTE_METHOD_DELETE    16
#define PHP_CAN_SERVER_ROUTE_METHOD_OPTIONS   32
#define PHP_CAN_SERVER_ROUTE_METHOD_TRACE     64
#define PHP_CAN_SERVER_ROUTE_METHOD_CONNECT   128
#define PHP_CAN_SERVER_ROUTE_METHOD_PATCH     256
#define PHP_CAN_SERVER_ROUTE_METHOD_ALL       511

#ifndef IS_PATH
#define IS_PATH 99
#endif

#define WS_FRAME_CONTINUATION 0x0
#define WS_FRAME_STRING       0x1
#define WS_FRAME_BINARY       0x2
#define WS_FRAME_CLOSE        0x8
#define WS_FRAME_PING         0x9
#define WS_FRAME_PONG         0xA

#define WS_CLOSE_NORMAL               1000
#define WS_CLOSE_GOING_AWAY           1001
#define WS_CLOSE_PROTOCOL_ERROR       1002
#define WS_CLOSE_UNEXPECTED_DATA      1003
#define WS_CLOSE_NOT_CONSISTENT       1007
#define WS_CLOSE_GENEIC               1008
#define WS_CLOSE_MESSAGE_TOOBIG       1009
#define WS_CLOSE_MISSING_EXTENSION    1010
#define WS_CLOSE_UNEXPECTED_CONDITION 1011

extern zend_class_entry *ce_can_server;
extern zend_class_entry *ce_can_server_request;
extern zend_class_entry *ce_can_server_response;
extern zend_class_entry *ce_can_server_route;
extern zend_class_entry *ce_can_server_websocket_route;
extern zend_class_entry *ce_can_server_websocket_ctx;
extern zend_class_entry *ce_can_server_router;

struct php_can_server {
    zend_object std;
    zval refhandle;
    struct evhttp *http;
    char *addr;
    char *logformat;
    int logformat_len;
    php_stream *logfile;
    int port;
    int running;
    zval *router;
};

struct php_can_server_request {
    zend_object std;
    zval refhandle;
    struct evhttp_request *req;
    zval *cookies;
    zval *get;
    zval *post;
    zval *files;
    double time;
    int status;
    long response_len;
    long response_code;
    char *error;
    char *uri;
    char *query;
};

struct php_can_server_route {
    zend_object std;
    zval refhandle;
    char *route;
    char *regexp;
    zval *handler;
    int  methods;
    zval *casts;
};

struct php_can_server_router {
    zend_object std;
    zval refhandle;
    long pos;
    /**
     * Container for all routes, one domension array
     * where key is route index and value is a route instance
     */
    zval *routes;
    /**
     * Container where we will search for the routes
     * two dimensional array in format:
     * array(
     *   'GET' => array(
     *     '/what/ever/' => 2, // route index
     *   ),
     *   'POST' => array( ... )
     * )
     */
    zval *method_routes;
    /**
     * Container where we will search through for the routes
     * in case we cannot find the route in router->method_routes
     * Actually the search through this container will be made only 
     * to determine what kind of error response code we must send
     * back to the client: 404 Not Found or 405 Method Not Allowed
     */
    zval *route_methods;
};

struct php_can_server_logentry {
    double request_time;
    int    request_type;
    char * request_uri;
    char * uri;
    char * query;
    long   response_len;
    char * remote_host;
    long   remote_port;
    long   response_code;
    size_t mem_usage;
    char * error;
};

struct php_can_client_ctx {
    long request_id;
    zval *zrequest;
    zval *callback;
    struct php_can_server *server;
    struct evhttp_connection *evcon;
};

struct php_can_websocket_ctx {
    zend_object std;
    zval refhandle;
    long timeout;
    char *id;
    struct evhttp_request *req;
    struct evhttp_connection *evcon;
    zval *zroute;
    int rfc6455;
};

#define SETNOW(double_now) \
    double_now = 0.0;  struct timeval __tpnow = {0}; \
    if(gettimeofday(&__tpnow, NULL) == 0 ) \
        double_now = (double)(__tpnow.tv_sec + __tpnow. tv_usec / 1000000.00);

#define WRITELOG(server, msg, len) \
    if (server->logformat_len) { \
        if (server->logfile == NULL) { \
            write(STDOUT_FILENO, msg, len); \
        } else { \
            php_stream_write(server->logfile, msg, len); \
        } \
    }

#define LOGENTRY_CTOR(logentry, request) \
    logentry = (struct php_can_server_logentry *) ecalloc(1, sizeof(*logentry)); \
    logentry->request_time = request->time; \
    logentry->request_type = request->req->type; \
    logentry->request_uri = (char *)evhttp_request_uri(request->req); \
    logentry->uri = estrdup(request->uri ? request->uri : "-"); \
    logentry->query = estrdup(request->query ? request->query : "-"); \
    logentry->response_len = request->response_len; \
    logentry->remote_host = request->req->remote_host; \
    logentry->remote_port = request->req->remote_port; \
    logentry->response_code = request->response_code; \
    logentry->mem_usage = 0; \
    logentry->error = estrdup(request->error ? request->error : "-");

#define LOGENTRY_LOG(logentry, server, count) \
    double now; SETNOW(now); \
    zval *map; MAKE_STD_ZVAL(map); array_init(map); \
    if (php_can_strpos(server->logformat, "cs-uri", 0) != FAILURE) \
        add_assoc_string(map, "cs-uri", logentry->uri, 1); \
    if (php_can_strpos(server->logformat, "cs-query", 0) != FAILURE) \
        add_assoc_string(map, "cs-query", logentry->query, 1); \
    if (php_can_strpos(server->logformat, "c-ip", 0) != FAILURE) \
        add_assoc_string(map, "c-ip", logentry->remote_host, 1); \
    if (php_can_strpos(server->logformat, "c-port", 0) != FAILURE) \
        add_assoc_long(map, "c-port", logentry->remote_port); \
    if (php_can_strpos(server->logformat, "cs-method", 0) != FAILURE) \
        add_assoc_string(map, "cs-method", php_can_method_name(logentry->request_type), 1); \
    if (php_can_strpos(server->logformat, "cs-uri", 0) != FAILURE) \
        add_assoc_string(map, "cs-uri", logentry->request_uri, 1); \
    if (php_can_strpos(server->logformat, "sc-status", 0) != FAILURE) \
        add_assoc_long(map, "sc-status", logentry->response_code); \
    if (php_can_strpos(server->logformat, "sc-bytes", 0) != FAILURE) { \
        if (logentry->response_len > 0) add_assoc_long(map, "sc-bytes", logentry->response_len); \
        else add_assoc_stringl(map, "sc-bytes", "-", 1, 1); \
    } \
    if (php_can_strpos(server->logformat, "x-reqnum", 0) != FAILURE) \
        add_assoc_long(map, "x-reqnum", count); \
    if (php_can_strpos(server->logformat, "x-memusage", 0) != FAILURE) \
        add_assoc_long(map, "x-memusage", zend_memory_usage(0 TSRMLS_CC)); \
    if (php_can_strpos(server->logformat, "time", 0) != FAILURE) { \
        char *str_time = php_format_date("H:i:s", sizeof("H:i:s"), (long)now, 1 TSRMLS_CC); \
        add_assoc_string(map, "time", str_time, 0); } \
    if (php_can_strpos(server->logformat, "date", 0) != FAILURE) { \
        char *str_time = php_format_date("Y-m-d", sizeof("Y-m-d"), (long)now, 1 TSRMLS_CC); \
        add_assoc_string(map, "date", str_time, 0); } \
    if (php_can_strpos(server->logformat, "time-taken", 0) != FAILURE) \
        add_assoc_long(map, "time-taken", (now - logentry->request_time) * 1000); \
    if (php_can_strpos(server->logformat, "bytes", 0) != FAILURE) { \
        if (logentry->response_len > 0) add_assoc_long(map, "bytes", logentry->response_len); \
        else add_assoc_stringl(map, "bytes", "-", 1, 1); \
    } \
    if (php_can_strpos(server->logformat, "x-error", 0) != FAILURE) \
        add_assoc_string(map, "x-error", logentry->error, 1); \
    zval *msg = php_can_strtr_array(server->logformat, server->logformat_len, Z_ARRVAL_P(map)); \
    WRITELOG(server, Z_STRVAL_P(msg), Z_STRLEN_P(msg)); \
    zval_ptr_dtor(&msg); \
    zval_ptr_dtor(&map);

#define LOGENTRY_DTOR(logentry) \
    efree(logentry->uri); \
    efree(logentry->query); \
    efree(logentry->error); \
    efree(logentry); 

PHP_MINIT_FUNCTION(can_server);
PHP_MSHUTDOWN_FUNCTION(can_server);
PHP_RINIT_FUNCTION(can_server);
PHP_RSHUTDOWN_FUNCTION(can_server);

#endif /* CAN_SERVER_H */
