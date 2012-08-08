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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"
#include "php_can.h"

#include <signal.h>

ZEND_DECLARE_MODULE_GLOBALS(can)

#ifdef COMPILE_DL_CAN
ZEND_GET_MODULE(can)
#endif
/**
 * Find the position of the first occurrence of a substring in a string
 */
int php_can_strpos(char *haystack, char *needle, int offset)
{
    if (!haystack) {
        return FAILURE;
    }

    int haystack_len = strlen(haystack) + 1;

    if (offset < 0 || offset > haystack_len) {
        return FAILURE;
    }

    char * found = php_memnstr(
        haystack + offset,
        needle,
        strlen(needle),
        haystack + haystack_len
    );

    if (!found) {
        return FAILURE;
    }
    return found - haystack;
}

/**
 * Return part of a string
 * 
 * @param str The input string. Must be one character or longer. 
 * @param f   If start is non-negative, the returned string will start at the start'th position in string, 
 *            counting from zero. For instance, in the string 'abcdef', the character at position 0 is 'a',
 *            the character at position 2 is 'c', and so forth.
 *            If start is negative, the returned string will start at the start'th character from the end of string.
 *            If string is less than or equal to start characters long, NULL will be returned.
 * @param l   If length is given and is positive, the string returned will contain at most length characters 
 *            beginning from start (depending on the length of string).
 *            If length is given and is negative, then that many characters will be omitted from the end of string 
 *            (after the start position has been calculated when a start is negative). If start denotes the position 
 *            of this truncation or beyond, false will be returned.
 *            If length is given and is 0 an empty string will be returned.
 *            If length is omitted, the substring starting from start until the end of the string will be returned. 
 * @return 
 */
char * php_can_substr(char *str, int f, int l)
{
    int str_len = strlen(str) + 1;

    if ((l < 0 && -l > str_len)) {
        return NULL;
    } else if (l > str_len) {
        l = str_len;
    }


    if (f > str_len) {
        return NULL;
    } else if (f < 0 && -f > str_len) {
        f = 0;
    }

    if (l < 0 && (l + str_len - f) < 0) {
        return NULL;
    }

    if (f < 0) {
        f = str_len + f;
        if (f < 0) {
            f = 0;
        }
    }

    if (l < 0) {
        l = (str_len - f) + l;
        if (l < 0) {
            l = 0;
        }
    }

    if (f >= str_len) {
        return NULL;
    }

    if ((f + l) > str_len) {
        l = str_len - f;
    }

    return estrndup(str + f, l);
}


char * php_can_method_name(int type)
{
    switch (type) {
        case EVHTTP_REQ_GET: return "GET"; break;
        case EVHTTP_REQ_POST: return "POST"; break;
        case EVHTTP_REQ_HEAD: return "HEAD"; break;
        case EVHTTP_REQ_PUT: return "PUT"; break;
        case EVHTTP_REQ_DELETE: return "DELETE"; break;
        case EVHTTP_REQ_OPTIONS: return "OPTIONS"; break;
        case EVHTTP_REQ_TRACE: return "TRACE"; break;
        case EVHTTP_REQ_CONNECT: return "CONNECT"; break;
        case EVHTTP_REQ_PATCH: return "PATCH"; break;
        default: return "Unknown"; break;
    }
}

zval * php_can_strtr_array(char *str, int slen, HashTable *hash)
{
    zval **entry;
    char  *string_key;
    uint   string_key_len;
    zval **trans;
    zval   ctmp;
    ulong num_key;
    int minlen = 128*1024;
    int maxlen = 0, pos, len, found;
    char *key;
    HashPosition hpos;
    smart_str result = {0};
    HashTable tmp_hash;

    zend_hash_init(&tmp_hash, zend_hash_num_elements(hash), NULL, NULL, 0);
    zend_hash_internal_pointer_reset_ex(hash, &hpos);
    while (zend_hash_get_current_data_ex(hash, (void **)&entry, &hpos) == SUCCESS) {
        switch (zend_hash_get_current_key_ex(hash, &string_key, &string_key_len, &num_key, 0, &hpos)) {
            case HASH_KEY_IS_STRING:
                len = string_key_len-1;
                if (len < 1) {
                    zend_hash_destroy(&tmp_hash);
                    return NULL;
                }
                zend_hash_add(&tmp_hash, string_key, string_key_len, entry, sizeof(zval*), NULL);
                if (len > maxlen) {
                    maxlen = len;
                }
                if (len < minlen) {
                    minlen = len;
                }
                break;

            case HASH_KEY_IS_LONG:
                Z_TYPE(ctmp) = IS_LONG;
                Z_LVAL(ctmp) = num_key;

                convert_to_string(&ctmp);
                len = Z_STRLEN(ctmp);
                zend_hash_add(&tmp_hash, Z_STRVAL(ctmp), len+1, entry, sizeof(zval*), NULL);
                zval_dtor(&ctmp);

                if (len > maxlen) {
                    maxlen = len;
                }
                if (len < minlen) {
                    minlen = len;
                }
                break;
        }
        zend_hash_move_forward_ex(hash, &hpos);
    }

    key = emalloc(maxlen+1);
    pos = 0;

    while (pos < slen) {
        if ((pos + maxlen) > slen) {
            maxlen = slen - pos;
        }

        found = 0;
        memcpy(key, str+pos, maxlen);

        for (len = maxlen; len >= minlen; len--) {
            key[len] = 0;

            if (zend_hash_find(&tmp_hash, key, len+1, (void**)&trans) == SUCCESS) {
                char *tval;
                int tlen;
                zval tmp;

                if (Z_TYPE_PP(trans) != IS_STRING) {
                    tmp = **trans;
                    zval_copy_ctor(&tmp);
                    convert_to_string(&tmp);
                    tval = Z_STRVAL(tmp);
                    tlen = Z_STRLEN(tmp);
                } else {
                    tval = Z_STRVAL_PP(trans);
                    tlen = Z_STRLEN_PP(trans);
                }

                smart_str_appendl(&result, tval, tlen);
                pos += len;
                found = 1;

                if (Z_TYPE_PP(trans) != IS_STRING) {
                    zval_dtor(&tmp);
                }
                break;
            }
        }

        if (! found) {
            smart_str_appendc(&result, str[pos++]);
        }
    }

    efree(key);
    zend_hash_destroy(&tmp_hash);
    smart_str_0(&result);

    zval *retval;
    MAKE_STD_ZVAL(retval);
    ZVAL_STRINGL(retval, result.c, result.len, 0);
    return retval;
}

/**
 * ignore SIGPIPE (or else it will bring our program down if the client closes its socket).
 * NB: if running under gdb, you might need to issue this gdb command:
 * handle SIGPIPE nostop noprint pass because, by default, gdb will stop our program execution 
 * (which we might not want).
 * @return 
 */
static int php_can_ignore_sigpipe(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigemptyset(&sa.sa_mask) < 0 || sigaction(SIGPIPE, &sa, 0) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

/**
 * Register standard class
 */
void php_can_register_std_class(
    zend_class_entry ** ppce,
    char * class_name,
    void * obj_ctor,
    const zend_function_entry * function_list TSRMLS_DC)
{
    zend_class_entry ce;

    INIT_CLASS_ENTRY_EX(ce, class_name, strlen(class_name), function_list);
    *ppce = zend_register_internal_class(&ce TSRMLS_CC);

    /* entries changed by initialize */
    if (obj_ctor) {
        (*ppce)->create_object = obj_ctor;
    }
}

/**
 * Register subclass
 */
void php_can_register_sub_class(
    zend_class_entry ** ppce,
    zend_class_entry * parent_ce,
    char * class_name,
    void *obj_ctor,
    const zend_function_entry * function_list TSRMLS_DC)
{
    zend_class_entry ce;

    INIT_CLASS_ENTRY_EX(ce, class_name, strlen(class_name), function_list);
    *ppce = zend_register_internal_class_ex(&ce, parent_ce, NULL TSRMLS_CC);

    /* entries changed by initialize */
    if (obj_ctor) {
        (*ppce)->create_object = obj_ctor;
    } else {
        (*ppce)->create_object = parent_ce->create_object;
    }
}

static zend_function_entry can_functions[] = {
    {NULL, NULL, NULL}
};

static zend_module_dep can_deps[] = {
    ZEND_MOD_REQUIRED("pcre")
    ZEND_MOD_REQUIRED("hash")
    {NULL, NULL, NULL}
};

zend_module_entry can_module_entry = {
    STANDARD_MODULE_HEADER_EX, NULL,
    can_deps,
    "can",
    can_functions,
    PHP_MINIT(can),
    PHP_MSHUTDOWN(can),
    PHP_RINIT(can),
    PHP_RSHUTDOWN(can),
    PHP_MINFO(can),
    PHP_CAN_VERSION,
    STANDARD_MODULE_PROPERTIES
};

PHP_MINIT_FUNCTION(can)
{
    CAN_G(can_event_base) = NULL;
    
    php_can_ignore_sigpipe();

    return PHP_MINIT(can_exception)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MINIT(can_server)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MINIT(can_server_router)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MINIT(can_server_route)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MINIT(can_server_websocket)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MINIT(can_server_request)(INIT_FUNC_ARGS_PASSTHRU)
    ;
}
PHP_MSHUTDOWN_FUNCTION(can)
{
    return PHP_MSHUTDOWN(can_exception)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MSHUTDOWN(can_server)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MSHUTDOWN(can_server_router)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MSHUTDOWN(can_server_route)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MSHUTDOWN(can_server_websocket)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_MSHUTDOWN(can_server_request)(INIT_FUNC_ARGS_PASSTHRU)
    ;
}

PHP_RINIT_FUNCTION(can)
{
    return PHP_RINIT(can_exception)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RINIT(can_server)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RINIT(can_server_router)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RINIT(can_server_route)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RINIT(can_server_websocket)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RINIT(can_server_request)(INIT_FUNC_ARGS_PASSTHRU)
    ;
}
PHP_RSHUTDOWN_FUNCTION(can)
{
    return PHP_RSHUTDOWN(can_exception)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RSHUTDOWN(can_server)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RSHUTDOWN(can_server_router)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RSHUTDOWN(can_server_route)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RSHUTDOWN(can_server_websocket)(INIT_FUNC_ARGS_PASSTHRU)
        & PHP_RSHUTDOWN(can_server_request)(INIT_FUNC_ARGS_PASSTHRU)
    ;
}

PHP_MINFO_FUNCTION(can)
{
    #include <event.h>
    char buf[64];
    snprintf(buf, sizeof(buf) - 1, "%s", event_get_version());

    php_info_print_table_start();
    php_info_print_table_row(2, "PHP Can Web Framework", "enabled");
    php_info_print_table_row(2, "Version", PHP_CAN_VERSION);
    php_info_print_table_row(2, "Build Date", __DATE__ " " __TIME__);
    php_info_print_table_row(2, "libevent version", buf);
    php_info_print_table_end();
}
