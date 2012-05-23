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

#include "php.h"
#include "php_can.h"
#include "Exception.h"

zend_class_entry *ce_can_Exception;
zend_class_entry *ce_can_RuntimeException;
zend_class_entry *ce_can_ServerBindingException;
zend_class_entry *ce_can_LogicException;
zend_class_entry *ce_can_InvalidParametersException;
zend_class_entry *ce_can_InvalidCallbackException;
zend_class_entry *ce_can_InvalidOperationException;
zend_class_entry *ce_can_HTTPForward;
zend_class_entry *ce_can_HTTPError;

/**
 * Throw an exception
 *
 */
int php_can_throw_exception(zend_class_entry *ce TSRMLS_DC, char *format, ...)
{
    va_list arg;
    char *message;
    zval *ex;

    va_start(arg, format);
    vspprintf(&message, 0, format, arg);
    va_end(arg);

    MAKE_STD_ZVAL(ex);
    object_init_ex(ex, ce);

    zend_update_property_string(ce, ex, "message", sizeof("message")-1, message TSRMLS_CC);
    zend_throw_exception_object(ex TSRMLS_CC);

    efree(message);
    return 0;
}

/**
 * Throw an exception with code and message
 *
 */
int php_can_throw_exception_code(zend_class_entry *ce TSRMLS_DC, long code, char *format, ...)
{
    va_list arg;
    char *message;
    zval *ex;

    va_start(arg, format);
    vspprintf(&message, 0, format, arg);
    va_end(arg);

    MAKE_STD_ZVAL(ex);
    object_init_ex(ex, ce);

    zend_update_property_long(ce, ex, "code", sizeof("code")-1, code TSRMLS_CC);
    zend_update_property_string(ce, ex, "message", sizeof("message")-1, message TSRMLS_CC);
    zend_throw_exception_object(ex TSRMLS_CC);

    efree(message);
    return 0;
}

/**
 * HTTPForward Constructor
 */
static PHP_METHOD(CanHttpForward, __construct)
{
    zval *url, *headers = NULL, *callback = NULL;
    
    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "z|zz", &url, &headers, &callback)
        || Z_TYPE_P(url) != IS_STRING
        || Z_STRLEN_P(url) == 0
        || (headers && Z_TYPE_P(headers) != IS_ARRAY)
    ) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(string $url[, array $headers])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    if (callback) {
        char *func_name;
        zend_bool is_callable = zend_is_callable(callback, 0, &func_name TSRMLS_CC);
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
        zend_update_property(ce_can_HTTPForward, getThis(), "callback", sizeof("callback")-1, callback TSRMLS_CC);
    }
    
    zend_update_property_string(ce_can_HTTPForward, getThis(), "url", sizeof("url")-1, Z_STRVAL_P(url) TSRMLS_CC);
    if (headers) {
        zend_update_property(ce_can_HTTPForward, getThis(), "headers", sizeof("headers")-1, headers TSRMLS_CC);
    }
}

/**
 * HTTPError Constructor
 */
static PHP_METHOD(CanHttpError, __construct)
{
    char *message;
    int message_len = 0;
    long code;

    if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC,
            "l|s", &code, &message, &message_len)) {
        zchar *space, *class_name = get_active_class_name(&space TSRMLS_CC);
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "%s%s%s(int $statuscode[, string $message])",
            class_name, space, get_active_function_name(TSRMLS_C)
        );
        return;
    }
    
    if (code < 400 || code > 599) {
        php_can_throw_exception(
            ce_can_InvalidParametersException TSRMLS_CC,
            "Invalid HTTP error statuscode, expecting 400-599"
        );
        return;
    }
    
    zend_update_property_long(ce_can_HTTPError, getThis(), "code", sizeof("code")-1, code TSRMLS_CC);
    zend_update_property_string(ce_can_HTTPError, getThis(), "message", sizeof("message")-1, 
            message_len > 0 ? message : "" TSRMLS_CC);
}

static zend_function_entry http_forward_methods[] = {
    PHP_ME(CanHttpForward, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

static zend_function_entry http_error_methods[] = {
    PHP_ME(CanHttpError, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void can_exceptions_init(TSRMLS_D)
{
    // class \Can\Exception
    PHP_CAN_REGISTER_SUBCLASS( &ce_can_Exception, zend_exception_get_default(TSRMLS_C),
        ZEND_NS_NAME(PHP_CAN_NS, "Exception"), NULL, NULL);

        // class \Can\RuntimeException extends \Can\Exception
        PHP_CAN_REGISTER_SUBCLASS( &ce_can_RuntimeException, ce_can_Exception,
            ZEND_NS_NAME(PHP_CAN_NS, "RuntimeException"), NULL, NULL);

            // class \Can\ServerBindingException extends \Can\RuntimeException
            PHP_CAN_REGISTER_SUBCLASS( &ce_can_ServerBindingException, ce_can_RuntimeException,
                ZEND_NS_NAME(PHP_CAN_NS, "ServerBindingException"), NULL, NULL);

        // class \Can\LogicException extends \Can\Exception
        PHP_CAN_REGISTER_SUBCLASS( &ce_can_LogicException, ce_can_Exception,
            ZEND_NS_NAME(PHP_CAN_NS, "LogicException"), NULL, NULL);

        // class \Can\InvalidParametersException extends \Can\LogicException
        PHP_CAN_REGISTER_SUBCLASS( &ce_can_InvalidParametersException, ce_can_LogicException,
            ZEND_NS_NAME(PHP_CAN_NS, "InvalidParametersException"), NULL, NULL);

        // class \Can\InvalidCallbackException extends \Can\LogicException
        PHP_CAN_REGISTER_SUBCLASS( &ce_can_InvalidCallbackException, ce_can_LogicException,
            ZEND_NS_NAME(PHP_CAN_NS, "InvalidCallbackException"), NULL, NULL);

        // class \Can\InvalidOperationException extends \Can\LogicException
        PHP_CAN_REGISTER_SUBCLASS( &ce_can_InvalidOperationException, ce_can_LogicException,
            ZEND_NS_NAME(PHP_CAN_NS, "InvalidOperationException"), NULL, NULL);
        
        // class \Can\HTTPForward extends \Can\Exception
        PHP_CAN_REGISTER_SUBCLASS(
            &ce_can_HTTPForward, 
            ce_can_Exception,
            ZEND_NS_NAME(PHP_CAN_NS, "HTTPForward"), 
            NULL, 
            http_forward_methods
        );
        PHP_CAN_REGISTER_PROPERTY(ce_can_HTTPForward, "url", ZEND_ACC_PROTECTED);
        PHP_CAN_REGISTER_PROPERTY(ce_can_HTTPForward, "headers", ZEND_ACC_PROTECTED);
        PHP_CAN_REGISTER_PROPERTY(ce_can_HTTPForward, "callback", ZEND_ACC_PROTECTED);
        
        
        // class \Can\HTTPError extends \Can\LogicException
        PHP_CAN_REGISTER_SUBCLASS(
            &ce_can_HTTPError, 
            ce_can_LogicException,
            ZEND_NS_NAME(PHP_CAN_NS, "HTTPError"), 
            NULL, 
            http_error_methods
        );
}

PHP_MINIT_FUNCTION(can_exception)
{
    can_exceptions_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(can_exception)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(can_exception)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(can_exception)
{
    return SUCCESS;
}
