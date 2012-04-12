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
#include "php_buddel.h"
#include "Exception.h"

zend_class_entry *ce_buddel_Exception;
zend_class_entry *ce_buddel_RuntimeException;
zend_class_entry *ce_buddel_ServerBindingException;
zend_class_entry *ce_buddel_LogicException;
zend_class_entry *ce_buddel_InvalidParametersException;
zend_class_entry *ce_buddel_InvalidCallbackException;
zend_class_entry *ce_buddel_InvalidOperationException;

/**
 * Throw an exception
 *
 */
int php_buddel_throw_exception(zend_class_entry *ce TSRMLS_DC, char *format, ...)
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

void buddel_exceptions_init(TSRMLS_D)
{
    PHP_BUDDEL_REGISTER_SUBCLASS( &ce_buddel_Exception, zend_exception_get_default(TSRMLS_C),
        ZEND_NS_NAME(PHP_BUDDEL_NS, "Exception"), NULL, NULL);

        PHP_BUDDEL_REGISTER_SUBCLASS( &ce_buddel_RuntimeException, ce_buddel_Exception,
            ZEND_NS_NAME(PHP_BUDDEL_NS, "RuntimeException"), NULL, NULL);

            PHP_BUDDEL_REGISTER_SUBCLASS( &ce_buddel_ServerBindingException, ce_buddel_RuntimeException,
                ZEND_NS_NAME(PHP_BUDDEL_NS, "ServerBindingException"), NULL, NULL);


        PHP_BUDDEL_REGISTER_SUBCLASS( &ce_buddel_LogicException, ce_buddel_Exception,
            ZEND_NS_NAME(PHP_BUDDEL_NS, "LogicException"), NULL, NULL);

        PHP_BUDDEL_REGISTER_SUBCLASS( &ce_buddel_InvalidParametersException, ce_buddel_LogicException,
            ZEND_NS_NAME(PHP_BUDDEL_NS, "InvalidParametersException"), NULL, NULL);

        PHP_BUDDEL_REGISTER_SUBCLASS( &ce_buddel_InvalidCallbackException, ce_buddel_LogicException,
            ZEND_NS_NAME(PHP_BUDDEL_NS, "InvalidCallbackException"), NULL, NULL);

        PHP_BUDDEL_REGISTER_SUBCLASS( &ce_buddel_InvalidOperationException, ce_buddel_LogicException,
            ZEND_NS_NAME(PHP_BUDDEL_NS, "InvalidOperationException"), NULL, NULL);
}

PHP_MINIT_FUNCTION(buddel_exception)
{
    buddel_exceptions_init(TSRMLS_C);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(buddel_exception)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(buddel_exception)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(buddel_exception)
{
    return SUCCESS;
}
