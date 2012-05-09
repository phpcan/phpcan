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

#ifndef CAN_CLIENT_H
#define CAN_CLIENT_H

#include "php.h"
#include "php_can.h"
#include "Exception.h"

#include <evhttp.h>

extern zend_class_entry *ce_can_client;
extern zend_class_entry *ce_can_client_response;

struct php_can_client {
    zend_object std;
    zval refhandle;
    struct event_base *base;
    struct evhttp_uri *uri;
    struct evhttp_connection *evcon;
    int status;
    long method;
    zval *handler;
    zval *headers;
};

struct php_can_client_response {
    zend_object std;
    zval refhandle;
    struct evhttp_request *req;
};

#endif /* CAN_CLIENT_H */
