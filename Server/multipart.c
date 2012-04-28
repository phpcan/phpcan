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
#include "Exception.h"
#include <event.h>

/* The longest anonymous name */
#define MAX_SIZE_ANONNAME 33

#if PHP_VERSION_ID < 50399 && HAVE_MBSTRING && !defined(COMPILE_DL_MBSTRING)
#include "ext/mbstring/mbstring.h"
#endif

static int unlink_filename(zval **item TSRMLS_DC)
{
    if (Z_TYPE_PP(item) == IS_STRING
        && strstr(Z_STRVAL_PP(item), "/phpcan") != NULL) {
        VCWD_UNLINK(Z_STRVAL_PP(item));
    }
    zval_ptr_dtor(item);
    return 0;
}

static char * getword(char **line, char stop)
{
    char *pos = *line, quote;
    char *res;

    while (*pos && *pos != stop) {

        if ((quote = *pos) == '"' || quote == '\'') {
            ++pos;
            while (*pos && *pos != quote) {
                if (*pos == '\\' && pos[1] && pos[1] == quote) {
                    pos += 2;
                } else {
                    ++pos;
                }
            }
            if (*pos) {
                ++pos;
            }
        } else ++pos;

    }
    if (*pos == '\0') {
        res = estrdup(*line);
        *line += strlen(*line);
        return res;
    }

    res = estrndup(*line, pos - *line);

    while (*pos == stop) {
        ++pos;
    }

    *line = pos;
    return res;
}


static char * substring_conf(char *start, int len, char quote TSRMLS_DC)
{
    char *result = emalloc(len + 2);
    char *resp = result;
    int i;

    for (i = 0; i < len; ++i) {
        if (start[i] == '\\' && (start[i + 1] == '\\' || (quote && start[i + 1] == quote))) {
            *resp++ = start[++i];
        } else {
#if PHP_VERSION_ID < 50399 && HAVE_MBSTRING && !defined(COMPILE_DL_MBSTRING)
            if (php_mb_encoding_translation(TSRMLS_C)) {
                size_t j = php_mb_gpc_mbchar_bytes(start+i TSRMLS_CC);
                while (j-- > 0 && i < len) {
                    *resp++ = start[i++];
                }
                --i;
            } else {
                *resp++ = start[i];
            }
#else
            *resp++ = start[i];
#endif
        }
    }

    *resp = '\0';
    return result;
}

static char * getword_conf(char **line TSRMLS_DC)
{
    char *str = *line, *strend, *res, quote;

#if PHP_VERSION_ID < 50399 && HAVE_MBSTRING && !defined(COMPILE_DL_MBSTRING)
    if (php_mb_encoding_translation(TSRMLS_C)) {
        int len=strlen(str);
        php_mb_gpc_encoding_detector(&str, &len, 1, NULL TSRMLS_CC);
    }
#endif

    while (*str && isspace(*str)) {
        ++str;
    }

    if (!*str) {
        *line = str;
        return estrdup("");
    }

    if ((quote = *str) == '"' || quote == '\'') {
        strend = str + 1;
look_for_quote:
        while (*strend && *strend != quote) {
            if (*strend == '\\' && strend[1] && strend[1] == quote) {
                strend += 2;
            } else {
                ++strend;
            }
        }
        if (*strend && *strend == quote) {
            char p = *(strend + 1);
            if (p != '\r' && p != '\n' && p != '\0') {
                strend++;
                goto look_for_quote;
            }
        }

        res =  substring_conf(str + 1, strend - str - 1, quote TSRMLS_CC);

        if (*strend == quote) {
            ++strend;
        }

    } else {

        strend = str;
        while (*strend && !isspace(*strend)) {
            ++strend;
        }
        res =  substring_conf(str, strend - str, 0 TSRMLS_CC);
    }

    while (*strend && isspace(*strend)) {
        ++strend;
    }

    *line = strend;
    return res;
}

static const char* my_memmem( const char * s1, size_t l1, const char * s2, size_t l2 )
{
    if( !l2 ) return s1;
    while( l1 >= l2 )
    {
        l1--;
        if( !memcmp( s1, s2, l2 ) )
            return s1;
        s1++;
    }
    return NULL;
}

static int unlink_uploaded_files(char **filename TSRMLS_DC)
{
    VCWD_UNLINK(*filename);
    return 0;
}

void  php_can_parse_multipart(const char* content_type, struct evbuffer* buffer, zval* post, zval** files TSRMLS_DC)
{
    char *boundary      = NULL,
         *boundary_end  = NULL,
         *boundary_val  = NULL,
         *temp_filename = NULL,
         *max_uploads   = INI_STR("max_file_uploads");

    int max_file_size = 0,
        skip_upload   = 0,
        upload_cnt    = 0,
        fd            = -1;

    size_t boundary_len     = 0,
           boundary_val_len = 0,
           anonindex        = 0,
           is_anonymous     = 0;

    const char * in     = EVBUFFER_DATA( buffer );
    size_t       inlen  = EVBUFFER_LENGTH( buffer );

    if (max_uploads && *max_uploads) {
        upload_cnt = atoi(max_uploads);
    }

    // Get the boundary
    boundary = strstr(content_type, "boundary");
    if (!boundary || !(boundary = strchr(boundary, '='))) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Missing boundary in multipart/form-data POST data");
        return;
    }

    boundary++;
    boundary_len = strlen(boundary);

    if (boundary[0] == '"') {
        boundary++;
        boundary_end = strchr(boundary, '"');
        if (!boundary_end) {
            php_can_throw_exception(
                ce_can_LogicException TSRMLS_CC,
                "Invalid boundary in multipart/form-data POST data"
            );
            return;
        }
    } else {
        // search for the end of the boundary
        boundary_end = strchr(boundary, ',');
    }
    if (boundary_end) {
        boundary_end[0] = '\0';
        boundary_len = boundary_end - boundary;
    }

    // set boundary value
    spprintf(&boundary_val, 0, "--%s", boundary);
    boundary_val_len = strlen(boundary_val);

    const char * delim = my_memmem(in, inlen, (const char *) boundary_val, boundary_val_len );

    MAKE_STD_ZVAL(*files);
    array_init(*files);

    while( delim ) {

        size_t part_len;
        const char * part = delim + boundary_val_len;
        inlen -= ( part - in );
        in = part;

        delim = my_memmem(in, inlen, (const char *) boundary_val, boundary_val_len );
        part_len = delim ? (size_t)( delim - part ) : inlen;

        if( part_len ) {

            char *cd   = NULL,
                 *pair = NULL,
                 *text = estrndup(part, part_len);

            if ( (cd = strstr(text, "Content-Disposition:")) != NULL) {

                char *param    = NULL,
                     *value    = NULL,
                     *filename = NULL;

                cd = strchr(text, ':');
                cd++;
                while (isspace(*cd)) ++cd;

                while (*cd && (pair = getword(&cd, ';'))) {

                    char *key  = NULL,
                         *word = pair;

                    while (isspace(*cd)) ++cd;

                    if (strchr(pair, '=')) {
                        key = getword(&pair, '=');
                        if (!strcasecmp(key, "name")) {
                            if (param) {
                                efree(param);
                            }
                            param = getword_conf(&pair TSRMLS_CC);
                        } else if (!strcasecmp(key, "filename")) {
                            if (filename) {
                                efree(filename);
                            }
                            filename = getword_conf(&pair TSRMLS_CC);
                        }
                    }

                    if (key) {
                        efree(key);
                    }
                    efree(word);
                }

                // If file_uploads=off, skip the file part
                if (!PG(file_uploads)) {
                    skip_upload = 1;
                } else if (upload_cnt <= 0) {
                    skip_upload = 1;
                    php_error_docref(NULL TSRMLS_CC, E_WARNING,
                        "Maximum number of allowable file uploads has been exceeded");
                }

                if (skip_upload) {
                    if (param) efree(param);
                    if (filename) efree(filename);
                    efree(text);
                    continue;
                }

                // no name="" and no filename="" found
                if (!param && !filename) {
                    php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                        "File Upload Mime headers garbled");
                    efree(text);
                    efree(boundary_val);
                    return;
                }

                if (!filename && param) {

                    if ((value = strstr( text, "\r\n\r\n" )) != NULL) {
                        value += 4; // skip CRLFs at the beginning
                        int value_len = part_len - ( value - text ) - 2;
                        add_assoc_stringl(post, param, value, value_len, 1);
                    }
                    efree(param);
                    param = NULL;
                }

                if (filename) {

                    char *s, *tmp=NULL;
                    s = strrchr(filename, '\\');
                    if ((tmp = strrchr(filename, '/')) > s) {
                        s = tmp;
                    }

                    if (!param) {
                        is_anonymous = 1;
                        param = emalloc(MAX_SIZE_ANONNAME);
                        snprintf(param, MAX_SIZE_ANONNAME, "%u", anonindex++);
                    } else {
                        is_anonymous = 0;
                    }


                    if ((value = strstr( text, "\r\n\r\n" )) != NULL) {

                        value += 4; // skip CRLFs at the beginning
                        int value_len = part_len - ( value - text ) - 2;

                        if (value_len > 0) {

                            fd = php_open_temporary_fd_ex(PG(upload_tmp_dir), "phpcan", &temp_filename, 1 TSRMLS_CC);
                            if (fd == -1) { // create temporary file failed
                                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                    "File upload error - unable to create a temporary file");
                            } else {

                                size_t wlen = write(fd, value, value_len);
                                if (wlen == -1) { // write failed
                                    php_error_docref(NULL TSRMLS_CC, E_WARNING,
                                        "File upload error - unable to write to a temporary file");
                                } else {

                                    zval *file;
                                    MAKE_STD_ZVAL(file);
                                    ALLOC_HASHTABLE(Z_ARRVAL_P(file));
                                    zend_hash_init(Z_ARRVAL_P(file), 4, NULL, (dtor_func_t)  unlink_filename, 0);
                                    Z_TYPE_P(file) = IS_ARRAY;

                                    add_assoc_string(file, "name", param, 1);
                                    if (s && s > filename) {
                                        add_assoc_string(file, "filename", s+1, 1);
                                    } else {
                                        add_assoc_string(file, "filename", filename, 1);
                                    }
                                    add_assoc_long(  file, "filesize", wlen);
                                    add_assoc_string(file, "tmp_name", temp_filename, 1);

                                    add_next_index_zval(*files, file);

                                }

                                efree(temp_filename);
                                close(fd);
                            }
                        }
                    }

                    efree(filename);
                    filename = NULL;

                    efree(param);
                    param = NULL;
                }

            }
            efree(text);
        }
    }
    efree(boundary_val);
}
