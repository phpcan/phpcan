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

#ifndef BUDDEL_EXCEPTION_H
#define BUDDEL_EXCEPTION_H

#include "php.h"
#include "zend_exceptions.h"

extern zend_class_entry *ce_buddel_Exception,
    // runtime exceptions
    *ce_buddel_RuntimeException,
    *ce_buddel_ServerBindingException,
    // logic exceptions
    *ce_buddel_LogicException,
    *ce_buddel_InvalidParametersException,
    *ce_buddel_InvalidCallbackException,
    *ce_buddel_InvalidOperationException,
    *ce_buddel_UnknownActionException,
    *ce_buddel_InvalidActionParameterException,
    *ce_buddel_RequiredActionParameterException;

int php_buddel_throw_exception(zend_class_entry *ce TSRMLS_DC, char *format, ...);

PHP_MINIT_FUNCTION(buddel_exception);
PHP_MSHUTDOWN_FUNCTION(buddel_exception);
PHP_RINIT_FUNCTION(buddel_exception);
PHP_RSHUTDOWN_FUNCTION(buddel_exception);

#endif /* BUDDEL_EXCEPTION_H */
