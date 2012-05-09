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

zend_class_entry *ce_can_client_response;
static zend_object_handlers client_response_obj_handlers;

static void client_response_dtor(void *object TSRMLS_DC);

static zend_object_value client_response_ctor(zend_class_entry *ce TSRMLS_DC)
{
    struct php_can_client_response *response;
    zend_object_value retval;

    response = ecalloc(1, sizeof(*response));
    response->req = NULL;
    zend_object_std_init(&response->std, ce TSRMLS_CC);

    retval.handle = zend_objects_store_put(response,
            (zend_objects_store_dtor_t)zend_objects_destroy_object,
            client_response_dtor,
            NULL TSRMLS_CC);
    retval.handlers = &client_response_obj_handlers;
    return retval;
}

static void client_response_dtor(void *object TSRMLS_DC)
{
    struct php_can_client_response *response = (struct php_can_client_response*)object;
    
    if (response->req) {
        response->req = NULL;
    }
    
    zend_objects_store_del_ref(&response->refhandle TSRMLS_CC);
    zend_object_std_dtor(&response->std TSRMLS_CC);
    efree(response);
}

/**
 * 
 * Constructor.
 */
static PHP_METHOD(CanClientResponse, __construct)
{
    /* protected. */
}

/**
 * 
 * Get response code
 */
static PHP_METHOD(CanClientResponse, getCode)
{
    struct php_can_client_response *response = (struct php_can_client_response*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (response->req) {
        RETURN_LONG(response->req->response_code);
    }
    RETURN_FALSE;
    
}

/**
 * Get headers
 */
static PHP_METHOD(CanClientResponse, getHeaders)
{
    struct evkeyval *header;
    struct php_can_client_response *response = (struct php_can_client_response*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (!response->req) {
        RETURN_FALSE;
    }
    
    array_init(return_value);
    for (header = ((response->req->output_headers)->tqh_first);
         header; 
         header = ((header)->next.tqe_next)
    ) {
        add_assoc_string(return_value, header->key, header->value, 1);
    }
}

/**
 * 
 * Get response body
 */
static PHP_METHOD(CanClientResponse, getBody)
{
    struct php_can_client_response *response = (struct php_can_client_response*)
        zend_object_store_get_object(getThis() TSRMLS_CC);
    
    if (!response->req) {
        RETURN_FALSE;
    }
    
    int buffer_len = EVBUFFER_LENGTH(response->req->input_buffer);
    if (buffer_len > 0) {
        RETURN_STRINGL(EVBUFFER_DATA(response->req->input_buffer), buffer_len, 1);
    }
    RETURN_FALSE;
}

static zend_function_entry client_response_methods[] = {
    PHP_ME(CanClientResponse, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PROTECTED)
    PHP_ME(CanClientResponse, getCode,     NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanClientResponse, getHeaders,  NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    PHP_ME(CanClientResponse, getBody,     NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static void client_response_init(TSRMLS_D)
{
    memcpy(&client_response_obj_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    client_response_obj_handlers.clone_obj = NULL;

    // class \Can\Client\Response
    PHP_CAN_REGISTER_CLASS(
        &ce_can_client_response,
        ZEND_NS_NAME(PHP_CAN_CLIENT_NS, "Response"),
        client_response_ctor,
        client_response_methods
    );
}

PHP_MINIT_FUNCTION(can_client_response)
{
    client_response_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(can_client_response)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(can_client_response)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(can_client_response)
{
    return SUCCESS;
}
