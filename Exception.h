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

#ifndef CAN_EXCEPTION_H
#define CAN_EXCEPTION_H

#include "php.h"
#include "zend_exceptions.h"

extern zend_class_entry *ce_can_Exception,
    // runtime exceptions
    *ce_can_RuntimeException,
    *ce_can_ServerBindingException,
    // logic exceptions
    *ce_can_LogicException,
    *ce_can_InvalidParametersException,
    *ce_can_InvalidCallbackException,
    *ce_can_InvalidOperationException,
    *ce_can_HTTPForward,
    *ce_can_HTTPError
    ;

int php_can_throw_exception(zend_class_entry *ce TSRMLS_DC, char *format, ...);

PHP_MINIT_FUNCTION(can_exception);
PHP_MSHUTDOWN_FUNCTION(can_exception);
PHP_RINIT_FUNCTION(can_exception);
PHP_RSHUTDOWN_FUNCTION(can_exception);

#endif /* CAN_EXCEPTION_H */
