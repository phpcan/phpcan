dnl  Copyright (c) 2002-2011, Dmitri Vinogradov <dmitri.vinogradov@gmail.com>.
dnl  All rights reserved.

PHP_ARG_ENABLE(can, whether to enable Can support,
[  --enable-can           Enable Can support])

PHP_ARG_WITH(libevent, libevent install prefix,
[  --with-libevent=DIR     libevent install prefix])

if test "$PHP_CAN" != "no"; then
  SEARCH_PATH="/usr /usr/local"
  SEARCH_FOR="/include/event2/event.h"

  if test "$PHP_LIBEVENT" = "yes"; then
    AC_MSG_CHECKING([for libevent headers in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        LIBEVENT_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  else
    AC_MSG_CHECKING([for libevent headers in $PHP_LIBEVENT])
    if test -r $PHP_LIBEVENT/$SEARCH_FOR; then
      LIBEVENT_DIR=$PHP_LIBEVENT
      AC_MSG_RESULT([found])
    fi
  fi

  if test -z "$LIBEVENT_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Cannot find libevent headers (version 2.+ required)])
  fi

  PHP_ADD_INCLUDE($LIBEVENT_DIR/include)

  LIBNAME=event
  LIBSYMBOL=event_base_new

  if test "x$PHP_LIBDIR" = "x"; then
    PHP_LIBDIR=lib
  fi

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $LIBEVENT_DIR/$PHP_LIBDIR, CAN_SHARED_LIBADD)
  ],[
    AC_MSG_ERROR([wrong libevent version {2.+ is required} or lib not found])
  ],[
    -L$LIBEVENT_DIR/$PHP_LIBDIR
  ])

  PHP_ADD_EXTENSION_DEP(can, sockets, true)
  PHP_SUBST(CAN_SHARED_LIBADD)
  PHP_NEW_EXTENSION(can, \
    Can.c \
    Exception.c \
    Server.c \
    Server/Router.c \
    Server/Route.c \
    Server/WebSocketRoute.c \
    Server/Request.c \
    Server/multipart.c \
    , $ext_shared)
fi
