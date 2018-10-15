dnl
dnl  Copyright 2018 Northern.tech AS
dnl
dnl  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
dnl
dnl  This program is free software; you can redistribute it and/or modify it
dnl  under the terms of the GNU General Public License as published by the
dnl  Free Software Foundation; version 3.
dnl
dnl  This program is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
dnl
dnl To the extent this program is licensed as part of the Enterprise
dnl versions of CFEngine, the applicable Commercial Open Source License
dnl (COSL) may apply to this file if you as a licensee so wish it. See
dnl included file COSL.txt.
dnl
dnl ##########################################################################
dnl #
dnl # Build CFEngine
dnl #
dnl # Run ./autogen.sh to build configure script
dnl #
dnl ##########################################################################

AC_PREREQ(2.63)

m4_define([cfversion_from_env],  m4_normalize(m4_esyscmd([echo $EXPLICIT_VERSION])))
m4_define([cfversion_from_file], m4_normalize(m4_esyscmd([cat CFVERSION])))
m4_ifval(cfversion_from_env, [
    m4_define([cfversion], cfversion_from_env)
], [
    m4_ifval(cfversion_from_file, [
        m4_define([cfversion], cfversion_from_file)
    ], [
        m4_fatal([Could not determine CFEngine version;
                  please set EXPLICIT_VERSION in the environment,
                  or make sure all git tags are up to date and rerun autogen.sh])
    ])
])


AC_INIT(cfengine, cfversion)
AC_CONFIG_SRCDIR([libpromises/generic_agent.c])
AC_CANONICAL_TARGET

dnl
dnl This program needs to be checked early, as MAKEINFO variable is expanded in
dnl AM_INIT_AUTOMAKE.
dnl
AC_CHECK_PROG(MAKEINFO, makeinfo, makeinfo)


dnl Parallel unit tests are causing spurious failures across several systems,
dnl particularly those doing process testing.
dnl Unfortunately the option to disable parallel tests (serial-tests) doesn't
dnl exist in automake 1.11 and earlier, so we need to do this complicated logic
dnl to determine whether we can disable it or not. If it doesn't exist as an
dnl option, then serial tests are already the default.
AC_MSG_CHECKING([automake version])
m4_define(AUTOMAKE_VERSION, m4_normalize(m4_esyscmd([automake --version 2>&1 | sed -ne '/^automake/{s/^automake.* \([^ ][^ ]*\)$/\1/; p;}'])))
m4_define(SERIAL_TESTS, m4_bmatch(AUTOMAKE_VERSION, [^1\.\([0-9]\|1[0-1]\)\(\.\|$\)], [], [serial-tests]))
AC_MSG_RESULT(AUTOMAKE_VERSION)


AM_INIT_AUTOMAKE([tar-ustar] SERIAL_TESTS)
AM_MAINTAINER_MODE([enable])

m4_divert_text([DEFAULTS], [: "${AR_FLAGS=cr}"])

cfengine_version=cfversion

m4_undefine([cfversion])
m4_undefine([cfversion_from_file])
m4_undefine([cfversion_from_env])

AC_DEFINE(BUILD_YEAR, esyscmd([date +%Y | tr -d '\n']), "Software build year")

AC_DEFINE_UNQUOTED(ABS_TOP_SRCDIR,
"`cd -- "$srcdir"; pwd`",
[Absolute path of source tree])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_HEADERS([libutils/config.h])

dnl Libtool madness

AC_CONFIG_MACRO_DIR([m4])

dnl
dnl hide [s]include macros, so old aclocal (automake < 1.10) won't find them and
dnl won't complain about something/something.m4 not found
dnl
m4_define(incstart,sinc)
m4_define(incend,lude)

dnl
dnl Save environment CFLAGS before autoconf starts messing with it.
dnl It is restored later.
dnl
ENV_CFLAGS="$CFLAGS"

dnl ######################################################################
dnl Checks for programs.
dnl ######################################################################

AC_PROG_CC
AC_PROG_MKDIR_P
AC_EXEEXT

dnl GCC specific flags
m4_include([m4/cf3_gcc_flags.m4])

# Use either new LT_INIT or old AC_DISABLE_STATIC/AC_PROG_LIBTOOL macros
m4_ifdef([LT_INIT],
  [LT_INIT([disable-static])],
  [AC_DISABLE_STATIC
   AC_PROG_LIBTOOL])

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL
AC_PATH_PROG([PERL], [perl])
AC_CONFIG_LIBOBJ_DIR(libcompat)
AC_PATH_PROG(GETCONF, getconf, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin:/sw/bin)
AM_CONDITIONAL(CROSS_COMPILING, test "x$cross_compiling" = "xyes")


dnl ######################################################################
dnl Use pthreads if available
dnl ######################################################################

AC_ARG_WITH([pthreads],
    [AS_HELP_STRING([--with-pthreads[[=PATH]]],
                    [Specify path to pthreads, if not the part of operating system])])

if test "x$with_pthreads" != x && test "x$with_pthreads" != "xyes" && test "x$with_pthreads" != "xno"; then
   LIBS="$LIBS -L$with_pthreads/lib"
   CPPFLAGS="-I$with_pthreads/include $CPPFLAGS"
fi

ACX_PTHREAD([],
            [AC_MSG_ERROR(pthread-compatible library is required to build CFEngine)])

CC="$PTHREAD_CC"
CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"

dnl ######################################################################
dnl Whether to build extensions as builtin extensions or a separate
dnl plugin. The default is plugin.
dnl --enable-builtin-extensions is the default on Windows, and can be
dnl used on other platforms for debugging purposes, but isn't otherwise
dnl supported.
dnl ######################################################################

AC_ARG_ENABLE([builtin-extensions], [AS_HELP_STRING([Build binaries with builtin extensions])],
              [],
              [
                  AS_CASE([${target_os}], [mingw*], [enable_builtin_extensions=yes], [enable_builtin_extensions=no])
              ])
AM_CONDITIONAL([BUILTIN_EXTENSIONS], [test "x$enable_builtin_extensions" = "xyes"])
AS_IF([test "x$enable_builtin_extensions" = "xyes"], [AC_DEFINE([BUILTIN_EXTENSIONS], [1], [Define if you want builtin Enterprise extensions])])
AC_SUBST([enable_builtin_extensions])

dnl ######################################################################
dnl Configure files layout
dnl ######################################################################

AC_ARG_ENABLE([fhs],
        [AS_HELP_STRING([--enable-fhs], [Enable FHS compliance. Defaults to custom CFEngine files layout])])

#
# pkglibdir/pkgdatadir are not overridable, so use our own invention instead.
#

AS_IF([test x"$enable_fhs" = xyes], [
  projlibdir='${pkglibdir}'
  WORKDIR='${localstatedir}/lib/${PACKAGE}'
  MASTERDIR='default'
  INPUTDIR='default'
  LOGDIR='${localstatedir}/log/${PACKAGE}'
  PIDDIR='${runstatedir:-${localstatedir}/run}/${PACKAGE}'
  STATEDIR='default'
], [
  if test x"$prefix" = xNONE || test x"$prefix" = x/var/cfengine; then
    prefix=/var/cfengine
    case "$target_os" in
      mingw*)
        WORKDIR=$(cmd /c "echo %PROGRAMFILES%\\Cfengine" | sed 's/\\/\\\\/g')
        MASTERDIR=default
        INPUTDIR=default
        LOGDIR=$(cmd /c "echo %PROGRAMFILES%\\Cfengine" | sed 's/\\/\\\\/g')
        PIDDIR=$(cmd /c "echo %PROGRAMFILES%\\Cfengine" | sed 's/\\/\\\\/g')
        STATEDIR=default
      ;;
      *)
        WORKDIR=/var/cfengine
        MASTERDIR=default
        INPUTDIR=default
        LOGDIR=/var/cfengine
        PIDDIR=/var/cfengine
        STATEDIR=default
      ;;
    esac
  else
    WORKDIR="${localstatedir}/${PACKAGE}"
    MASTERDIR="default"
    INPUTDIR="default"
    LOGDIR="${localstatedir}/${PACKAGE}"
    PIDDIR="${localstatedir}/${PACKAGE}"
    STATEDIR="default"
  fi
  bindir="${bindir:-${exec_prefix}/bin}"
  projlibdir='${libdir}'
])

AC_SUBST(projlibdir)

dnl ######################################################################
dnl Platform specific compiler flags.
dnl ######################################################################

AS_CASE([${target_os}],
  [mingw*],
    # Disable printf format warnings, because our wrapper supports more
    # flags than vanilla Windows version, so they are false positives.
    [CFLAGS="$CFLAGS -Wno-format"])

dnl ######################################################################
dnl Enable debugging
dnl ######################################################################

AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug], [Enable debugging]),
              [debug=$enableval],
              [debug=no])
AM_CONDITIONAL([NDEBUG], [test x"$debug" = x"no"])

dnl Even though CFLAGS should contain the command-line CFLAGS
dnl as last, some macro seem to messes the order up and insert
dnl its own optimisation flags as well. So we append ENV_CFLAGS
dnl at the end manually, causing a bit of flag duplication.

if test x"$debug" = x"yes"
then
    CFLAGS="$CFLAGS -g3 -O0 $ENV_CFLAGS"
else
    CFLAGS="$CFLAGS -O2 -DNDEBUG $ENV_CFLAGS"
fi

dnl ######################################################################
dnl Checks for libraries.
dnl ######################################################################

dnl Now check for database connectors

# TODO remove
AC_ARG_WITH([sql],
    [AS_HELP_STRING([--with-sql[[=PATH]]],
        [Enable SQL database connectors (deprecated, use \
        --with[[out]]-postgresql and --with[[out]]-mysql instead)])
    ],
    [with_postgresql=$with_sql; with_mysql=$with_sql],
    [])

dnl PostgreSQL

AC_ARG_WITH([postgresql],
    [AS_HELP_STRING([--with-postgresql[[=PATH]]],
        [Enable PostgreSQL connector])
    ],
    [],
    [with_postgresql=check])

if test "x$with_postgresql" != "xno"
then
    if test "x$with_postgresql" != xyes && test "x$with_postgresql" != xcheck
    then
        PG_CONFIG=$with_postgresql/bin/pg_config
    else
        PG_CONFIG=pg_config
    fi

    # pg_config is only for native builds
    if test "x$cross_compiling" = "xno"
    then
        if test x`which $PG_CONFIG` != x
        then
            pg_include_dir=`$PG_CONFIG --includedir`
            if test -n "$pg_include_dir"
            then
                POSTGRESQL_CPPFLAGS="-I$pg_include_dir"
            fi
        fi
    fi

    CF3_WITH_LIBRARY(postgresql,
        [AC_CHECK_LIB(pq, PQconnectdb,
            [],
            [if test "x$with_postgresql" != xcheck; then
                AC_MSG_ERROR(Cannot find PostgreSQL client library); fi]
         )
         AC_CHECK_HEADERS(libpq-fe.h,
             [],
             [if test "x$with_postgresql" != xcheck; then
                 AC_MSG_ERROR(Cannot find PostgreSQL client library); fi]
         )]
    )
fi

dnl MySQL

AC_ARG_WITH([mysql],
    [AS_HELP_STRING([--with-mysql[[=PATH]]],
        [Enable MySQL connector])
    ],
    [],
    [with_mysql=check])

if test "x$with_mysql" != "xno"
then
    if test "x$with_mysql" != xyes &&
       test "x$with_mysql" != xcheck
    then
        MYSQL_CONFIG=$with_mysql/bin/mysql_config
    else
        MYSQL_CONFIG=mysql_config
    fi

    # mysql_config is only for native builds
    if test "x$cross_compiling" = "xno"
    then
        if test x`which $MYSQL_CONFIG` != x
        then
            mysql_include_dir=`$MYSQL_CONFIG --include`
            if test -n "$mysql_include_dir"
            then
                MYSQL_CPPFLAGS="$mysql_include_dir"
            fi
        fi
    fi

    CF3_WITH_LIBRARY(mysql,
        [AC_CHECK_LIB(mysqlclient, mysql_real_connect,
            [],
            [if test "x$with_mysql" != xcheck; then
                AC_MSG_ERROR(Cannot find MySQL client library); fi]
        )
        AC_CHECK_HEADERS(mysql.h,
            [],
            [if test "x$with_mysql" != xcheck; then
                AC_MSG_ERROR(Cannot find MySQL clientlibrary); fi]
        )
        AC_CHECK_LIB(mysqlclient, EVP_CIPHER_CTX_init,
            [AC_MSG_ERROR([MySQL client library exports symbols clashing \
                with OpenSSL. Get the update from distribution provider, \
                recompile MySQL library or disable MySQL connector. See \
                http://bugs.mysql.com/bug.php?id=65055 for details.])
            ]
        )]
    )
fi

dnl
dnl In-process databases
dnl

dnl QDBM

AC_ARG_WITH([qdbm],
    [AS_HELP_STRING([--with-qdbm[[=PATH]]], [use QDBM to store runtime data])])
AS_IF([test -n "$with_qdbm" && test "x$with_qdbm" != "xno"], [WITH_QDBM=1], [WITH_QDBM=0])

if test $WITH_QDBM = 1; then
  CF3_WITH_LIBRARY(qdbm, [
    AC_CHECK_LIB(qdbm, dpopen, [], [AC_MSG_ERROR(Cannot find QDBM)])
    AC_CHECK_HEADERS(depot.h, [], [AC_MSG_ERROR(Cannot find QDBM)])
    AC_DEFINE(QDB, 1, [Define if QDBM is available])
  ])
fi

dnl Tokyo Cabinet

AC_ARG_WITH([tokyocabinet],
    [AS_HELP_STRING([--with-tokyocabinet[[=PATH]]], [use Tokyo Cabinet to store runtime data])])

AS_IF([test $WITH_QDBM -eq 0 && test -n "$with_tokyocabinet" && test "x$with_tokyocabinet" != "xno"], [WITH_TOKYO=1], [WITH_TOKYO=0])

if test $WITH_TOKYO = 1; then
  CF3_WITH_LIBRARY(tokyocabinet, [
    AC_CHECK_LIB(tokyocabinet, tchdbnew, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_CHECK_HEADERS(tcutil.h, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_CHECK_HEADERS(tchdb.h, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_DEFINE(TCDB, 1, [Define if Tokyo Cabinet is available.])
  ])
fi

dnl OpenLDAP Lightning MDB

AC_ARG_WITH([lmdb],
    [AS_HELP_STRING([--with-lmdb[[=PATH]]], [use Lightning MDB to store runtime data])])

AS_IF([test $WITH_TOKYO -eq 0 && test $WITH_QDBM -eq 0 && (! test -n "$with_lmdb" || test "x$with_lmdb" != "xno")], [WITH_LMDB=1], [WITH_LMDB=0])

if test $WITH_LMDB = 1; then
  CF3_WITH_LIBRARY(lmdb, [
    AC_CHECK_LIB(lmdb, mdb_dbi_open, [], [AC_MSG_ERROR(Cannot find Lightning MDB)])
    AC_CHECK_HEADERS(lmdb.h, [], [AC_MSG_ERROR(Cannot find Lightning MDB)])
    AC_DEFINE(LMDB, 1, [Define if Lightning MDB is available])
  ])
fi

if test $WITH_QDBM -eq 0 && test $WITH_TOKYO -eq 0 && test $WITH_LMDB -eq 0; then
   AC_MSG_ERROR(Either Tokyo Cabinet, LMDB, or QDBM must be selected to compile CFEngine)
fi

dnl
dnl OpenSSL
dnl

AC_ARG_WITH(openssl,
    [AS_HELP_STRING([--with-openssl[[=PATH]]],
    [Specify OpenSSL path])], [], [with_openssl=yes])

if test x"$with_openssl" = xno ; then
    AC_MSG_ERROR([This release of CFEngine requires OpenSSL >= 0.9.7])
fi

if  test -d /usr/local/Cellar/openssl/ && \
    test -d /usr/local/opt/openssl/ && \
    test "x$with_openssl" = "xyes" ; then
    with_openssl="/usr/local/opt/openssl"
    echo "OS X Homebrew detected"
    echo "Defaulting to: --with-openssl=$with_openssl"
fi

CF3_WITH_LIBRARY(openssl, [
   AC_CHECK_LIB(crypto, RSA_generate_key_ex, [], [])
   AC_CHECK_LIB(ssl, SSL_free, [], [])
   AC_CHECK_DECLS([SSL_CTX_clear_options], [], [], [[#include <openssl/ssl.h>]])
   AC_CHECK_HEADERS([openssl/opensslv.h], [], [AC_MSG_ERROR(Cannot find OpenSSL)])

   AC_MSG_CHECKING(for OpenSSL version)

   AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
   #include <openssl/opensslv.h>

   #if OPENSSL_VERSION_NUMBER < 0x0090602fL
   #This OpenSSL is too old
   #endif
   ]])],[AC_MSG_RESULT(OK)],[AC_MSG_ERROR(This release of CFEngine requires OpenSSL >= 0.9.7)])

   if test "x$ac_cv_lib_crypto_RSA_generate_key_ex" = "xyes" ; then
      AC_DEFINE(OPENSSL_NO_DEPRECATED, 1, [Define if non deprecated API is available.])
   fi

   if test "x$ac_cv_lib_crypto_RSA_generate_key_ex" = "xno" && \
      test "x$ac_cv_lib_crypto_RSA_generate_key" = "xno" ; then
      AC_MSG_ERROR(Cannot find OpenSSL)
   fi
])

dnl PCRE

AC_ARG_WITH([pcre], [AS_HELP_STRING([--with-pcre[[=PATH]]], [Specify PCRE path])], [], [with_pcre=yes])

if test "x$with_pcre" = "xno"; then
   AC_MSG_ERROR([PCRE is required])
fi

CF3_WITH_LIBRARY(pcre, [
  AC_CHECK_LIB(pcre, pcre_exec, [], [AC_MSG_ERROR(Cannot find PCRE)])
  AC_CHECK_HEADERS([pcre.h], [],
    [AC_CHECK_HEADERS([pcre/pcre.h], [],
                      AC_MSG_ERROR(Cannot find PCRE))])
])

dnl libvirt

AC_ARG_WITH([libvirt],
    [AS_HELP_STRING([--with-libvirt[[=PATH]]], [support virtual machine management])],
    [], [with_libvirt=check])

if test "x$with_libvirt" != xno
then
   CF3_WITH_LIBRARY(libvirt, [
      AC_CHECK_LIB(virt, virConnectOpen, [], [if test "x$with_libvirt" != xcheck; then AC_MSG_ERROR(Cannot find libvirt library); fi])
      AC_CHECK_HEADERS(libvirt/libvirt.h, [], [if test "x$with_libvirt" != xcheck; then AC_MSG_ERROR(Cannot find libvirt library headers); fi])
   ])
fi

dnl  BIG FAT WARNING FOR AC_CHECK_LIB: *always* leave the
dnl  "action-if-yes" parameter empty else LIBS is not changed and C
dnl  define is not generated!  Or use AC_SEARCH_LIBS for a better
dnl  interface.

dnl libacl
dnl It is enough for any of the header files [acl.h sys/acl.h acl/libacl.h] to be present

AC_ARG_WITH([libacl],
    [AS_HELP_STRING([--with-libacl[[=PATH]]], [Specify libacl path])], [], [with_libacl=check])

if test "x$with_libacl" != xno
then
  CF3_WITH_LIBRARY(libacl, [
    AC_CHECK_LIB(acl, acl_init,
      [],
      [if test "x$with_libacl" != xcheck; then AC_MSG_ERROR(Cannot find libacl library); fi])
    AC_CHECK_HEADERS([acl.h sys/acl.h acl/libacl.h],
      [libacl_header_found=yes])
    if test "x$libacl_header_found" != "xyes" && test "x$with_libacl" != xcheck;
    then
      AC_MSG_ERROR(Cannot find libacl library headers);
    fi
  ])
fi

dnl libcurl

AC_ARG_WITH([libcurl],
    [AS_HELP_STRING([--with-libcurl[[=PATH]]], [Specify libcurl path])], [], [with_libcurl=check])

if test "x$with_libcurl" != xno
then
  CF3_WITH_LIBRARY(libcurl, [
    AC_CHECK_LIB(curl, curl_global_init,
      [],
      [  libcurl_lib_found=no;
         if test "x$with_libcurl" != xcheck;
         then
             AC_MSG_ERROR(Cannot find libcurl library);
         fi
      ])
    AC_CHECK_HEADERS([curl/curl.h],
      [libcurl_header_found=yes],
      [if test "x$with_libcurl" != xcheck; then AC_MSG_ERROR(Cannot find libcurl header files); fi])
  ])
fi

AM_CONDITIONAL(HAVE_LIBCURL,
        [test "x$libcurl_lib_found" != xno && test "x$libcurl_header_found" = xyes])

dnl libyaml

AC_ARG_WITH([libyaml],
    [AS_HELP_STRING([--with-libyaml[[=PATH]]], [Specify libyaml path])], [], [with_libyaml=check])

if test "x$with_libyaml" != xno
then
  CF3_WITH_LIBRARY(libyaml, [
    AC_CHECK_LIB(yaml, yaml_parser_initialize,
      [],
      [if test "x$with_libyaml" != xcheck; then AC_MSG_ERROR(Cannot find libyaml library); fi])
    AC_CHECK_HEADERS(yaml.h,
      [libyaml_header_found=yes],
      [if test "x$with_libyaml" != xcheck; then AC_MSG_ERROR(Cannot find libyaml header files); fi])
  ])
fi

dnl libxml2

AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--with-libxml2[[=PATH]]],
        [Specify libxml2 path])
    ],
    [],
    [with_libxml2=check])

if test "x$with_libxml2" != xno
then
    if test "x$with_libxml2" != xyes &&
       test "x$with_libxml2" != xcheck
    then
        XML2_CONFIG=$with_libxml2/bin/xml2-config
    else
        XML2_CONFIG=xml2-config
    fi

    # xml2-config is only for native builds
    if test "x$cross_compiling" = "xno" && test x`which $XML2_CONFIG` != x
    then
        xml2_include_dir=`$XML2_CONFIG --cflags`
        if test -n "$xml2_include_dir"
        then
            LIBXML2_CPPFLAGS="$xml2_include_dir"
        fi
    else                # xml2-config not found
        # if a path, e.g. /var/cfengine was given, then we
        # must take into account that libxml2 includes are in
        # /var/cfengine/include/libxml2
        LIBXML2_CPPFLAGS=-I$with_libxml2/include/libxml2
    fi

    CF3_WITH_LIBRARY(libxml2,
        [AC_CHECK_LIB(xml2, xmlFirstElementChild,
            [],
            [if test "x$with_libxml2" != xcheck; then
                AC_MSG_ERROR(Cannotfind libxml2); fi]
        )
        AC_CHECK_HEADERS([libxml/xmlwriter.h], [break],
            [if test "x$with_libxml2" != xcheck; then
                AC_MSG_ERROR(Cannot find libxml2); fi]
        )]
    )
fi

AM_CONDITIONAL([HAVE_LIBXML2],
    [test "x$with_libxml2" != xno &&
     test "x$ac_cv_lib_xml2_xmlFirstElementChild" = xyes])

dnl avahi

AC_CHECK_HEADERS(avahi-client/client.h,
    AM_CONDITIONAL(HAVE_AVAHI_CLIENT, true),
    AM_CONDITIONAL(HAVE_AVAHI_CLIENT, false),
    [])
AC_CHECK_HEADERS(avahi-common/address.h,
    AM_CONDITIONAL(HAVE_AVAHI_COMMON, true),
    AM_CONDITIONAL(HAVE_AVAHI_COMMON, false),
    [])

dnl

dnl ######################################################################
dnl Checks for header files.
dnl ######################################################################

AC_CHECK_HEADERS(unistd.h stdlib.h sys/loadavg.h)
AC_CHECK_HEADERS(sys/param.h sys/resource.h)

# sys/param.h is required for sys/mount.h on OpenBSD
AC_CHECK_HEADERS(sys/mount.h, [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

# Required on BSD to get struct sockaddr_dl (for retrieving MAC addresses from getifaddrs())
AC_CHECK_HEADERS(net/if_dl.h)

# Required on Solaris to get struct arpreq (for retrieving MAC addresses)
AC_CHECK_HEADERS(net/if_arp.h, , , [AC_INCLUDES_DEFAULT
#include <sys/socket.h>
])

AC_CHECK_HEADERS(utime.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(malloc.h sys/malloc.h)
AC_CHECK_HEADERS(vfs.h)
AC_CHECK_HEADERS(sys/vfs.h)
AC_CHECK_HEADERS(sys/sockio.h)
AC_CHECK_HEADERS(sys/statvfs.h)
AC_CHECK_HEADERS(sys/statfs.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(sys/filesys.h)
AC_CHECK_HEADERS(dustat.h)
AC_CHECK_HEADERS(sys/systeminfo.h)
AC_CHECK_HEADERS(ieeefp.h)
AC_CHECK_HEADERS(winsock2.h)
AC_CHECK_HEADERS(ws2tcpip.h)
AC_CHECK_HEADERS(zone.h)
AC_CHECK_HEADERS(sys/uio.h)
AC_CHECK_HEADERS_ONCE([sys/sysmacros.h]) dnl glibc deprecated inclusion in sys/type.h
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(sys/mpctl.h) dnl For HP-UX $(sys.cpus) - Mantis #1069
AC_CHECK_HEADERS(shadow.h)
AC_CHECK_HEADERS(sys/jail.h, [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])
AC_CHECK_HEADERS(net/route.h netinet/in.h netinet/ip.h)


AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_HEADER_STDBOOL

dnl ######################################################################
dnl Checks for data types
dnl ######################################################################

AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_CHECK_TYPES(clockid_t, [], [], [[
#ifdef HAVE_TIME_H
# include <time.h>
#endif
]])

AC_CHECK_TYPES(socklen_t, [], [], [[
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <sys/socket.h>
]])

dnl ######################################################################
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ######################################################################

AC_C_CONST

AC_FUNC_FSEEKO
AC_SYS_LARGEFILE
AC_TYPE_OFF_T

#
# AC_SYS_LARGEFILE correctly figures out necessary macros for large files, but
# on AIX there is a gotcha:
#
# Code generated by flex #includes <stdio.h> at the beginning of the file, which
# picks up 32-bit wide off_t. Then it #includes <config.h> which provides LFS
# macros, and finally it includes another system header, now with 64-bit wide
# off_t, which causes a conflict.
#
if test "x$ac_cv_sys_large_files" = x1; then
  CPPFLAGS="-D_LARGE_FILES=1 $CPPFLAGS"
fi

dnl ######################################################################
dnl Check for libraries
dnl ######################################################################

AC_CHECK_LIB(m, sqrt)
AC_CHECK_LIB(rt, clock_gettime)
AC_CHECK_LIB(dl, dlopen)

dnl ######################################################################
dnl Check for special functions
dnl ######################################################################

AC_CHECK_DECLS(clock_gettime, [], [], [[#include <time.h>]])
AC_REPLACE_FUNCS(clock_gettime)

AC_CHECK_DECLS(unsetenv)
AC_REPLACE_FUNCS(unsetenv)

AC_CHECK_DECLS(strnlen)
AC_REPLACE_FUNCS(strnlen)

cf3_FUNC_STRNDUP

AC_CHECK_DECLS(setlinebuf)
AC_REPLACE_FUNCS(setlinebuf)

AC_CHECK_DECLS(strlcat)
AC_REPLACE_FUNCS(strlcat)

AC_CHECK_DECLS(strlcpy)
AC_REPLACE_FUNCS(strlcpy)

AC_CHECK_DECLS(realpath)

AC_CHECK_DECLS(strdup)
AC_REPLACE_FUNCS(strdup)

AC_CHECK_DECLS(memrchr)
AC_REPLACE_FUNCS(memrchr)

AC_CHECK_DECLS(round, [], [], [[#include <math.h>]])
AC_REPLACE_FUNCS(round)

AC_CHECK_DECLS(nanosleep)
AC_REPLACE_FUNCS(nanosleep)

AC_CHECK_DECLS(memdup)
AC_REPLACE_FUNCS(memdup)

AC_CHECK_DECLS(memmem)
AC_REPLACE_FUNCS(memmem)

AC_CHECK_DECLS(srand48)
AC_REPLACE_FUNCS(srand48)

AC_CHECK_DECLS(drand48)
AC_REPLACE_FUNCS(drand48)

AC_CHECK_DECLS(strerror)
AC_REPLACE_FUNCS(strerror)

AC_CHECK_DECLS(strstr)
AC_REPLACE_FUNCS(strstr)

AC_CHECK_DECLS(strcasestr)
AC_REPLACE_FUNCS(strcasestr)

AC_CHECK_DECLS(strcasecmp)
AC_REPLACE_FUNCS(strcasecmp)

AC_CHECK_DECLS(strncasecmp)
AC_REPLACE_FUNCS(strncasecmp)

AC_CHECK_DECLS(strsep)
AC_REPLACE_FUNCS(strsep)

AC_CHECK_DECLS(strsignal)
AC_REPLACE_FUNCS(strsignal)

AC_CHECK_DECLS(gmtime_r, [], [], [[#include <time.h>]])
AC_REPLACE_FUNCS(gmtime_r)

AC_CHECK_DECLS(getline, [], [], [#define _GNU_SOURCE 1
AC_INCLUDES_DEFAULT])
AC_REPLACE_FUNCS(getline)

AC_CHECK_DECLS(strchrnul, [], [], [#define _GNU_SOURCE 1
AC_INCLUDES_DEFAULT])
AC_REPLACE_FUNCS(strchrnul)

AC_CHECK_DECLS(localtime_r, [], [], [[#include <time.h>]])
AC_REPLACE_FUNCS(localtime_r)

AC_CHECK_DECLS(fgetgrent, [], [], [[#include <grp.h>]])

AC_CHECK_DECLS(isfinite, [], [], [[#include <math.h>]])

AC_CHECK_FUNCS(getpwent setpwent endpwent)

AC_CHECK_FUNCS(fgetspent lckpwdf ulckpwdf)
AC_CHECK_LIB([sec], [fgetspent], [
    AC_DEFINE([HAVE_LIBSEC], 1, [Define if -lsec is available])
    AC_DEFINE([HAVE_FGETSPENT], 1, [Define if fgetspent is available])
    LIBS="-lsec $LIBS"
])

AC_CHECK_DECLS(getloadavg)
AC_FUNC_GETLOADAVG

AC_C_BIGENDIAN
AC_CHECK_HEADERS([endian.h])
AC_CHECK_DECLS(le32toh, [], [], [[#include <endian.h>]])

AC_CHECK_DECLS(closefrom, [], [], [[#include <unistd.h>
                                    #include <stdlib.h>]])
AC_REPLACE_FUNCS(closefrom)

AC_CHECK_HEADERS([sys/pstat.h])
AC_CHECK_FUNCS(pstat_getfile2)

CF3_PATH_ROOT_PROG([CHPASSWD], [chpasswd], [], [/sbin:/usr/sbin:/bin:/usr/bin:$PATH])
AS_IF([test "x$CHPASSWD" != "x"],
      [AC_DEFINE(HAVE_CHPASSWD, 1, [Define if chpasswd tool is present])]
      [AC_DEFINE_UNQUOTED(CHPASSWD, ["$CHPASSWD"], [Path to chpasswd tool])]
     )

dnl AIX has this.
CF3_PATH_ROOT_PROG([PWDADM], [pwdadm], [], [/sbin:/usr/sbin:/bin:/usr/bin:$PATH])
AS_IF([test "x$PWDADM" != "x"],
      [AC_DEFINE(HAVE_PWDADM, 1, [Define if pwdadm tool is present])]
      [AC_DEFINE_UNQUOTED(PWDADM, ["$PWDADM"], [Path to pwdadm tool])]
     )

CF3_PATH_ROOT_PROG([USERADD], [useradd], [], [/sbin:/usr/sbin:/bin:/usr/bin:$PATH])
AS_IF([test "x$USERADD" != "x"],
      [AC_DEFINE(HAVE_USERADD, 1, [Define if useradd tool is present])]
      [AC_DEFINE_UNQUOTED(USERADD, ["$USERADD"], [Path to useradd tool])]
     )

CF3_PATH_ROOT_PROG([USERMOD], [usermod], [], [/sbin:/usr/sbin:/bin:/usr/bin:$PATH])
AS_IF([test "x$USERMOD" != "x"],
      [AC_DEFINE(HAVE_USERMOD, 1, [Define if usermod tool is present])]
      [AC_DEFINE_UNQUOTED(USERMOD, ["$USERMOD"], [Path to usermod tool])]
     )

CF3_PATH_ROOT_PROG([USERDEL], [userdel], [], [/sbin:/usr/sbin:/bin:/usr/bin:$PATH])
AS_IF([test "x$USERDEL" != "x"],
      [AC_DEFINE(HAVE_USERDEL, 1, [Define if userdel tool is present])]
      [AC_DEFINE_UNQUOTED(USERDEL, ["$USERDEL"], [Path to userdel tool])]
     )

AS_IF([test "x$USERADD" != x && \
       test "x$USERMOD" != x && test "x$USERDEL" != x],
      [have_userprogs=yes],
      [have_userprogs=no]
)


AC_ARG_WITH([pam], AS_HELP_STRING([--with-pam], [Compile with PAM support]))
AS_IF([test x$with_pam != xno],
[
  CF3_WITH_LIBRARY(pam, [
    AC_CHECK_LIB(pam, pam_start)
    AC_CHECK_HEADERS([security/pam_appl.h])
  ])
  dnl These platforms must have pam, others can have it, but not required.
  AS_CASE([$target_os],
          [*gnu*|*solaris*|*aix*|*hpux*|*hp-ux*], [
              AS_IF([test "x$ac_cv_lib_pam_pam_start" != "xyes"],
                    [AC_MSG_ERROR(Cannot find PAM library)]
              )
              AS_IF([test "x$ac_cv_header_security_pam_appl_h" != "xyes"],
                    [AC_MSG_ERROR(Cannot find PAM headers)]
          )]
  )
  AS_IF([test "x$ac_cv_lib_pam_pam_start" = "xyes" && \
         test "x$ac_cv_header_security_pam_appl_h" = "xyes"],
        [have_pam=yes],
        [have_pam=no]
  )
  AC_CHECK_FUNCS(fgetpwent fgetgrent)
  AS_IF([test "x$have_pam" = "xyes" && \
      test "x$have_userprogs" = "xyes" && \
      test "x$ac_cv_func_fgetpwent" = "xyes" && \
      test "x$ac_cv_func_fgetgrent" = "xyes"],
  [
      users_promises_ok=yes
  ],[
      users_promises_ok=no
  ])
], [
  users_promises_ok=no
])
AM_CONDITIONAL(HAVE_USERS_PROMISE_DEPS, [test "x$users_promises_ok" = "xyes"])

AC_CHECK_DECLS(getnetgrent, [], [], [[#include <netdb.h>]])
AC_CHECK_FUNCS(getnetgrent)

AC_CHECK_DECLS(setnetgrent, [], [], [[#include <netdb.h>]])
AC_CHECK_FUNCS(setnetgrent)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]],
                                   [[extern int setnetgrent(const char *)]])],
  [AC_DEFINE([SETNETGRENT_RETURNS_INT], 1, [Whether setnetgrent returns int])],
  [AC_DEFINE([SETNETGRENT_RETURNS_INT], 0, [Whether setnetgrent returns int])])


AC_CHECK_DECLS(endnetgrent, [], [], [[#include <netdb.h>]])
AC_CHECK_FUNCS(endnetgrent)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]],
                                   [[extern int endnetgrent(const char *)]])],
  [AC_DEFINE([ENDNETGRENT_RETURNS_INT], 1, [Whether endnetgrent returns int])],
  [AC_DEFINE([ENDNETGRENT_RETURNS_INT], 0, [Whether endnetgrent returns int])])


AC_CHECK_FUNCS(sendto)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
                                     #include <sys/socket.h>]],
                                   [[extern ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)]])],
    [AC_DEFINE([SENDTO_RETURNS_SSIZE_T], 1, [Whether sendto returns ssize_t])],
    [AC_DEFINE([SENDTO_RETURNS_SSIZE_T], 0, [Whether sendto does not returns ssize_t])])

CF3_CHECK_PROPER_FUNC([ctime],
  [],
  [[#error ctime(3) may produce different results on different OSes. Let's have our POSIX-compliant implementation all the time]],
  [#define ctime rpl_ctime])
CF3_REPLACE_PROPER_FUNC([ctime])

dnl Check whether mkdir accepts two parameters
CF3_CHECK_PROPER_FUNC([mkdir],
  [[#include <sys/stat.h>
    #include <sys/types.h>]],
  [[int mkdir(const char *pathname, mode_t mode);]],
  [#define mkdir rpl_mkdir])

dnl Check for Win32 stat. We don't know to detect improper stat during
dnl compilation, so we resort to OS type detection.

CF3_CHECK_PROPER_FUNC([stat],
  [[#include <sys/types.h>
    #include <sys/stat.h>]],
  [[#if defined(__MINGW32__)
    #error stat in Windows CRT ill-behaves
    #endif]],
  [])

dnl Check for Win32 rename. We don't know how to detect improper rename (not
dnl removing target file if it exists) during cross-compilation, so we resort to
dnl OS type detection.

CF3_CHECK_PROPER_FUNC([rename],
  [],
  [[#if defined(__MINGW32__)
    #error rename in Windows CRT ill-behaves
    #endif]],
  [#define rename rpl_rename])

AC_CHECK_DECLS(mkdtemp)
AC_REPLACE_FUNCS(mkdtemp)

AC_CHECK_DECLS(strrstr)
AC_REPLACE_FUNCS(strrstr)

AC_CHECK_FUNCS(seteuid setegid setreuid setregid)
AC_CHECK_FUNCS(uname gethostname chflags)
AC_CHECK_FUNCS(mkfifo statfs statvfs door)
AC_CHECK_FUNCS(sysinfo setsid sysconf)
AC_CHECK_FUNCS(getzoneid getzonenamebyid)
AC_CHECK_FUNCS(fpathconf)

AC_CHECK_MEMBERS([struct stat.st_mtim, struct stat.st_mtimespec])
AC_CHECK_MEMBERS([struct stat.st_blocks])

AC_MSG_CHECKING([for PRIuMAX/PRIdMAX macros])
AC_EGREP_CPP([primacros_found],
  AC_INCLUDES_DEFAULT
  [#include <inttypes.h>
  #if defined(PRIuMAX) && defined(PRIdMAX)
    primacros_found
  #endif
  ],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)
   AC_MSG_RESULT(Unable to find out how to scan intmax_t/uintmax_t types)])

HW_FUNC_VSNPRINTF
HW_FUNC_SNPRINTF
HW_FUNC_VASPRINTF
HW_FUNC_ASPRINTF

dnl dirfd might be a function or a macro

AC_CHECK_DECLS(dirfd, [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
])

AC_CHECK_FUNCS(dirfd, [], [
  AC_MSG_CHECKING([for dirfd macro])
  AC_EGREP_CPP([dirfd_found],
    AC_INCLUDES_DEFAULT
    [#ifdef HAVE_DIRENT_H
    # include <dirent.h>
    #endif
    #ifdef dirfd
      dirfd_found
    #endif
    ],
  [AC_MSG_RESULT(yes)
  DIRFD_MACRO_FOUND=1],
  [AC_MSG_RESULT(no)])
  if test x$DIRFD_MACRO_FOUND = x; then
    AC_LIBOBJ([dirfd])
  fi])

AC_CHECK_FUNCS(jail_get)

dnl
dnl Various functions
dnl

AC_SEARCH_LIBS(setsockopt, socket)
AC_SEARCH_LIBS(gethostent, nsl)

AC_CHECK_FUNCS(socket)
AC_CHECK_FUNCS(setsockopt)
AC_CHECK_FUNCS(gethostent)

AC_CHECK_TYPES(struct sockaddr_storage, [], [], [[
#if HAVE_WINSOCK2_H
    #include <winsock2.h>
#endif
#if HAVE_WS2TCPIP_H
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netdb.h>
#endif]])

AC_CHECK_DECLS(getaddrinfo, [], [AC_LIBOBJ(getaddrinfo)], [[
#if HAVE_WINSOCK2_H
    #include <winsock2.h>
#endif
#if HAVE_WS2TCPIP_H
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netdb.h>
#endif
]])

AC_CHECK_DECLS([[inet_ntop], [inet_pton]], [], [], [[#include <arpa/inet.h>]])
AC_REPLACE_FUNCS(inet_ntop inet_pton)

AC_CHECK_FUNCS(getifaddrs)

AC_CHECK_FUNCS(getprocs64)

AC_CHECK_FUNC(lchown, AC_DEFINE(HAVE_LCHOWN, 1, [Whether to use lchown(3) to change ownerships]))

AC_CHECK_DECLS(pthread_attr_setstacksize, [], [], [[#include <pthread.h>]])
AC_REPLACE_FUNCS(pthread_attr_setstacksize)

AC_CHECK_DECLS(pthread_sigmask, [], [], [[#include <signal.h>]])
AC_REPLACE_FUNCS(pthread_sigmask)

AC_CHECK_DECLS(sched_yield, [], [], [[#include <sched.h>]])
AC_CHECK_FUNCS(sched_yield)

AC_CHECK_DECLS([openat], [], [], [[#define _GNU_SOURCE 1
                                   #include <fcntl.h>]])
AC_CHECK_DECLS([fstatat], [], [], [[#define _GNU_SOURCE 1
                                    #include <sys/stat.h>]])
AC_CHECK_DECLS([fchownat], [], [], [[#define _GNU_SOURCE 1
                                     #include <unistd.h>]])
AC_CHECK_DECLS([fchmodat], [], [], [[#define _GNU_SOURCE 1
                                    #include <sys/stat.h>]])
AC_CHECK_DECLS([readlinkat], [], [], [[#define _GNU_SOURCE 1
                                       #include <unistd.h>]])
AC_REPLACE_FUNCS(openat fstatat fchownat fchmodat readlinkat)

AC_CHECK_DECLS([log2], [], [], [[#include <math.h>]])
AC_REPLACE_FUNCS(log2)

dnl ######################################################################
dnl Required by cf-upgrade. It cannot be implemented in libcompat because
dnl cf-upgrade does not link to any libraries except libutils and only
dnl statically.
dnl ######################################################################
AC_CHECK_FUNC(fexecve, AC_DEFINE(HAVE_FEXECVE, 1, [Whether to use fexecve(3) to execute a new process]))

dnl ######################################################################
dnl These need declarations here, but will be defined in the
dnl Enterprise Windows code.
dnl ######################################################################

AC_CHECK_DECLS(alarm)
AC_CHECK_DECLS(chmod)
AC_CHECK_DECLS(chown)
AC_CHECK_DECLS(fchmod)
AC_CHECK_FUNCS(fchmod)
AC_CHECK_DECLS(uname)
AC_CHECK_DECLS(getuid)
AC_CHECK_DECLS(getgid)
AC_CHECK_DECLS(lstat)
AC_CHECK_FUNCS(sleep)
AC_CHECK_DECLS(socketpair, [], [], [[#include <sys/socket.h>]])
AC_CHECK_DECLS(fsync)
AC_CHECK_DECLS(glob, [], [], [[#include <glob.h>]])

dnl ######################################################################
dnl Check for sa_len in struct sockaddr
dnl ######################################################################

AC_CHECK_MEMBERS([struct sockaddr.sa_len], , , [
#include <sys/types.h>
#include <sys/socket.h>])

AC_CHECK_MEMBERS([struct ifreq.ifr_hwaddr],,, [
#include <sys/ioctl.h>
#include <net/if.h>])

dnl BSD uses sys/sysctl.h for CPU counting
AC_CHECK_HEADERS(sys/sysctl.h, [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

dnl ######################################################################
dnl Requirements for GetUptimeMinutes() to set $(sys.uptime)
dnl ######################################################################
dnl Linux sysinfo() call
AC_CHECK_MEMBERS([struct sysinfo.uptime], , , [#include <sys/sysinfo.h>])

dnl BSD uses sys/sysctl.h to get time-of-boot
AC_CHECK_HEADERS(sys/sysctl.h, [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

dnl Solaris uses kstat.h to get time-of-boot
AC_CHECK_HEADERS(kstat.h)
dnl SystemV way to get uptime, highly portable
AC_CHECK_HEADERS(utmp.h)
dnl POSIX way to get uptime
AC_CHECK_HEADERS(utmpx.h)

dnl #######################################################################
dnl Newer BSD systems don't have a compatible rtentry - use ortentry
dnl #######################################################################

rtry=none
AC_MSG_CHECKING(for either struct rtentry or struct ortentry)
AC_EGREP_HEADER(rtentry, net/route.h, rtry=rtentry)
if test "$rtry" = rtentry; then
 AC_DEFINE(HAVE_RTENTRY, 1, [Do we have any route entry structure?])
fi
AC_EGREP_HEADER(ortentry, net/route.h, rtry=ortentry)
if test "$rtry" = ortentry; then
 AC_DEFINE(HAVE_ORTENTRY, 1, [The old route entry structure in newer BSDs])
fi
AC_MSG_RESULT([$rtry])

dnl #######################################################################
dnl Enable extended attributes. Used for SELinux and ACLs
dnl #######################################################################

AC_CHECK_FUNCS(llistxattr, [AC_DEFINE(WITH_XATTR, 1, [Define if you have a libc that supports extended attributes])])
AC_CHECK_HEADERS([attr/xattr.h sys/xattr.h])

AC_MSG_CHECKING([whether xattr functions have extra arguments])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <sys/types.h>
                                    #include <sys/xattr.h>],
                                    [(void)llistxattr("", 0, 0, 0);
                                    (void)lgetxattr("", "", 0, 0, 0, 0);
                                    (void)lsetxattr("", "", "", 0, 0, 0);
                                    (void)lremovexattr("", "", 0);])],
                  [AC_DEFINE(WITH_XATTR_EXTRA_ARGS, 1, [Define if your xattr implementation has extra arguments])]
                  [AC_MSG_RESULT([yes])],
                  [AC_MSG_RESULT([no])])

dnl ######################################################################
dnl Give the chance to enable SELINUX
dnl ######################################################################

AC_ARG_ENABLE([selinux], [AS_HELP_STRING([--enable-selinux], [Deprecated. SELinux support is always enabled])])

dnl ######################################################################
dnl Collect all the options
dnl ######################################################################

CORE_CPPFLAGS="$LMDB_CPPFLAGS $TOKYOCABINET_CPPFLAGS $QDBM_CPPFLAGS $PCRE_CPPFLAGS $OPENSSL_CPPFLAGS $SQLITE3_CPPFLAGS $LIBACL_CPPFLAGS $LIBCURL_CPPFLAGS $LIBYAML_CPPFLAGS $POSTGRESQL_CPPFLAGS $MYSQL_CPPFLAGS $LIBXML2_CPPFLAGS $CPPFLAGS"
CORE_CFLAGS="$LMDB_CFLAGS $TOKYOCABINET_CFLAGS $QDBM_CFLAGS $PCRE_CFLAGS $OPENSSL_CFLAGS $SQLITE3_CFLAGS $LIBACL_CFLAGS $LIBCURL_CFLAGS $LIBYAML_CFLAGS $POSTGRESQL_CFLAGS $MYSQL_CFLAGS $LIBXML2_CFLAGS $CFLAGS"
CORE_LDFLAGS="$LMDB_LDFLAGS $TOKYOCABINET_LDFLAGS $QDBM_LDFLAGS $PCRE_LDFLAGS $OPENSSL_LDFLAGS $SQLITE3_LDFLAGS $LIBACL_LDFLAGS $LIBCURL_LDFLAGS $LIBYAML_LDFLAGS $POSTGRESQL_LDFLAGS $MYSQL_LDFLAGS $LIBXML2_LDFLAGS $LDFLAGS"
CORE_LIBS="$LMDB_LIBS $TOKYOCABINET_LIBS $QDBM_LIBS $PCRE_LIBS $OPENSSL_LIBS $SQLITE3_LIBS $LIBACL_LIBS $LIBCURL_LIBS $LIBYAML_LIBS $POSTGRESQL_LIBS $MYSQL_LIBS $LIBXML2_LIBS $LIBS"

dnl ######################################################################
dnl Make them available to subprojects.
dnl ######################################################################

AC_SUBST([CORE_CPPFLAGS])
AC_SUBST([CORE_CFLAGS])
AC_SUBST([CORE_LDFLAGS])
AC_SUBST([CORE_LIBS])
AC_CONFIG_FILES([configure_flags.env])

dnl ######################################################################
dnl OS specific stuff
dnl ######################################################################

case "$target_os" in

   solaris2.8|solaris2.9)
        AC_DEFINE(_XOPEN_SOURCE, 500, [UNIX 98])
        AC_DEFINE(__EXTENSIONS__, 1, [Extended UNIX 98 interfaces])
        ;;
   solaris2.10|solaris2.11)
        AC_DEFINE(_XOPEN_SOURCE, 600, [SUSv3])
        AC_DEFINE(__EXTENSIONS__, 1, [Extended UNIX 98 interfaces])
        ;;
   hpux*|hp-ux*)
        dnl pstat* functions may fail with EOVERFLOW without this.
        AC_DEFINE(_PSTAT64, 1, [Enable wide data structures everywhere])
        ;;
   aix*)
        CPPFLAGS="$CPPFLAGS -w"
        ;;
   linux*|*bsd*|*gnu*)
        AC_CHECK_LIB(nss_nis, yp_get_default_domain)
        ;;
   freebsd*|dragonfly*)
        ;;
   netbsd*)
        ;;
   unicos*)
        ;;
   cray*)
        ;;
   qnx*)
        ;;
   openbsd*|obsd*)
        ;;
   sysv4.2MP|unix_sv*)
        ;;
   cygwin*)
        ;;
   mingw*)
        ;;
   sco*)
        ;;
   darwin*)
        ;;
   *)
        AC_MSG_ERROR(Unknown system type $target_os)
        ;;
esac

m4_include([m4/cf3_platforms.m4])

dnl #####################################################################
dnl Configure directories
dnl #####################################################################

AC_ARG_WITH(workdir,
    [  --with-workdir=WORKDIR  default for internal (trusted) working directory ],
    [
        if test "x$withval" != x ; then
            WORKDIR="$withval"
            LOGDIR="$withval"
            PIDDIR="$withval"
        fi
    ],
)

dnl Expand ${prefix} and whatnot in WORKDIR

adl_RECURSIVE_EVAL("${WORKDIR}", WORKDIR)

AC_DEFINE_UNQUOTED(WORKDIR, "${WORKDIR}", [Workdir location])
AC_SUBST(workdir, "${WORKDIR}")

AC_ARG_WITH(masterdir,
    [  --with-masterdir=MASTERDIR  default for internal masterfiles directory ],
    [
        if test "x$withval" != x ; then
            MASTERDIR="$withval"
        fi
    ],
)

dnl Expand ${prefix} and whatnot in MASTERDIR

adl_RECURSIVE_EVAL("${MASTERDIR}", MASTERDIR)

AC_DEFINE_UNQUOTED(MASTERDIR, "${MASTERDIR}", [Masterfiles directory location])
AC_SUBST(masterdir, "${MASTERDIR}")

AC_ARG_WITH(inputdir,
    [  --with-inputdir=INPUTDIR  default for internal inputs directory ],
    [
        if test "x$withval" != x ; then
            INPUTDIR="$withval"
        fi
    ],
)

dnl Expand ${prefix} and whatnot in INPUTDIR

adl_RECURSIVE_EVAL("${INPUTDIR}", INPUTDIR)

AC_DEFINE_UNQUOTED(INPUTDIR, "${INPUTDIR}", [Inputs directory location])
AC_SUBST(inputdir, "${INPUTDIR}")

AC_ARG_WITH(logdir,
    [  --with-logdir=LOGDIR  default for internal log directory ],
    [
        if test "x$withval" != x ; then
            LOGDIR="$withval"
        fi
    ],
)

dnl Expand ${prefix} and whatnot in LOGDIR

adl_RECURSIVE_EVAL("${LOGDIR}", LOGDIR)

AC_DEFINE_UNQUOTED(LOGDIR, "${LOGDIR}", [Logdir location])
AC_SUBST(logdir, "${LOGDIR}")

AC_ARG_WITH(piddir,
    [  --with-piddir=LOGDIR  default for internal pid directory ],
    [
        if test "x$withval" != x ; then
            PIDDIR="$withval"
        fi
    ],
)

dnl Expand ${prefix} and whatnot in PIDDIR

adl_RECURSIVE_EVAL("${PIDDIR}", PIDDIR)

AC_DEFINE_UNQUOTED(PIDDIR, "${PIDDIR}", [piddir location])
AC_SUBST(piddir, "${PIDDIR}")

AC_ARG_WITH(statedir,
    [  --with-statedir=STATEDIR  default for internal state directory ],
    [
        if test "x$withval" != x ; then
            STATEDIR="$withval"
        fi
    ],
)

dnl Expand ${prefix} and whatnot in STATEDIR

adl_RECURSIVE_EVAL("${STATEDIR}", STATEDIR)

AC_DEFINE_UNQUOTED(STATEDIR, "${STATEDIR}", [State directory location])
AC_SUBST(statedir, "${STATEDIR}")

AC_ARG_WITH(shell, [AS_HELP_STRING([--with-shell=PATH],
                   [Specify path to POSIX-compatible shell (if not /bin/sh)])],
                   [],
                   [with_shell=/bin/sh])

dnl Expand ${prefix} and whatnot in bindir
adl_RECURSIVE_EVAL("${bindir}", bindir)

AC_DEFINE_UNQUOTED(BINDIR, "${bindir}", [binaries location])
AC_SUBST(bindir, "${bindir}")


if test "x$with_shell" = "xno"; then
   AC_MSG_ERROR([Please specify full path to POSIX-compatible shell])
fi
AC_DEFINE_UNQUOTED(SHELL_PATH, "$with_shell", [Path to the POSIX-compatible shell])

dnl ######################################################################
dnl init.d script installation
dnl ######################################################################

AC_ARG_WITH(init-script, AS_HELP_STRING([--with-init-script=PATH],
    [Install init.d script in given path. The default is no, but if specified, the default path is platform specific.]),
[
    if test -n "$withval" && test "x$withval" != "xno"; then
        if test "x$withval" = "xyes"; then
            AS_CASE(["$target_os"],
            [aix*],
            [
                INIT_D_PATH=/etc/rc.d/init.d
            ],
            [hpux*],
            [
                INIT_D_PATH=/sbin/init.d
            ],
            [
                INIT_D_PATH=/etc/init.d
            ])
        else
            INIT_D_PATH="$withval"
        fi
        AC_SUBST([INIT_D_PATH])
        AC_CONFIG_FILES([misc/init.d/cfengine3], [chmod +x misc/init.d/cfengine3])
    fi
])
AM_CONDITIONAL([WITH_INIT_D_SCRIPT], [test -n "$INIT_D_PATH"])

dnl ######################################################################
dnl systemd unit file installation
dnl ######################################################################

AC_ARG_WITH(systemd-service, AS_HELP_STRING([--with-systemd-service=PATH],
    [Install systemd service file in given path. The default is no, but if specified, the default path is /usr/lib/systemd/system.]),
[
    if test -n "$withval" && test "x$withval" != "xno"; then
        if test "x$withval" = "xyes"; then
            SYSTEMD_SERVICE_PATH=/usr/lib/systemd/system
        else
            SYSTEMD_SERVICE_PATH="$withval"
        fi
        AC_SUBST([SYSTEMD_SERVICE_PATH])
        AC_CONFIG_FILES([misc/systemd/cfengine3.service])
        AC_CONFIG_FILES([misc/systemd/cf-apache.service])
        AC_CONFIG_FILES([misc/systemd/cf-execd.service])
        AC_CONFIG_FILES([misc/systemd/cf-hub.service])
        AC_CONFIG_FILES([misc/systemd/cf-monitord.service])
        AC_CONFIG_FILES([misc/systemd/cf-postgres.service])
        AC_CONFIG_FILES([misc/systemd/cf-runalerts.service])
        AC_CONFIG_FILES([misc/systemd/cf-serverd.service])
    fi
])
AM_CONDITIONAL([WITH_SYSTEMD_SERVICE], [test -n "$SYSTEMD_SERVICE_PATH"])

AC_ARG_WITH(environment-path, AS_HELP_STRING([--with-environment-path=PATH],
    [Specifies the location of the environment files for the platform. Currently used only by systemd.]))
OS_ENVIRONMENT_PATH=
if test -z "$with_environment_path" || test "$with_environment_path" = "yes"; then
    if test -n "$SYSTEMD_SERVICE_PATH"; then
        for i in /etc/sysconfig /etc/default; do
            if test -d $i; then
                OS_ENVIRONMENT_PATH=$i
                break
            fi
        done
        if test -z "$OS_ENVIRONMENT_PATH"; then
            AC_MSG_ERROR([Unable to detect location of environment files on the platform (e.g. '/etc/sysconfig'). Please specify it using --with-environment-path, or turn off systemd support.])
        fi
    fi
else
    if test "$with_environment_path" = "no"; then
        if test -n "$SYSTEMD_SERVICE_PATH"; then
            AC_MSG_ERROR([It is not possible to both specify systemd support and not provide an environment path with --without-environment-path.])
        fi
    else
        OS_ENVIRONMENT_PATH="$with_environment_path"
    fi
fi
AC_SUBST([OS_ENVIRONMENT_PATH])


dnl #####################################################################
dnl Hostname and Version stuff
dnl #####################################################################

AC_PATH_PROG(HOSTNAME, hostname, "", $PATH)

AC_DEFINE_UNQUOTED(AUTOCONF_HOSTNAME, "`$HOSTNAME`", [Special CFEngine symbol])
AC_DEFINE_UNQUOTED(AUTOCONF_SYSNAME, "$target_os", [Speial CFEngine symbol])


dnl #####################################################################
dnl xen cpuid-based hvm detection
dnl #####################################################################

AC_MSG_CHECKING(for Xen cpuid-based HVM detection)
if test x"$GCC" = "xyes"; then
    case $host_cpu in
        i[[3456]]86*|x86_64*|amd64)
            AC_DEFINE(XEN_CPUID_SUPPORT, 1, [Define if XEN cpuid-based HVM detection is available.])
            AC_MSG_RESULT(yes)
            ;;
        *)
            AC_MSG_RESULT(no)
            ;;
    esac
else
    AC_MSG_RESULT(no)
fi

dnl
dnl Code coverage
dnl

AC_ARG_ENABLE(coverage, AS_HELP_STRING([--enable-coverage], [Enable code coverage]),
              [use_coverage=$enableval], [use_coverage=no])

if test "x$use_coverage" = "xyes"; then
  if test "$GCC" != "yes"; then
    AC_MSG_ERROR([GCC is required for --enable-coverage])
  fi

  if test "$debug" != "yes"; then
    AC_MSG_ERROR([--enable-debug is required for --enable-coverage])
  fi

  AC_CHECK_PROG(LCOV, lcov, lcov)
  AC_CHECK_PROG(LCOV_GENHTML, genhtml, genhtml)

  if test -z "$LCOV"; then
    AC_MSG_ERROR([Cannot find lcov from the LTP package])
  fi

  if test -z "$LCOV_GENHTML"; then
    AC_MSG_ERROR([Could not find genhtml from the LTP package])
  fi

  dnl Remove all optimization flags from CFLAGS
  changequote({,})
  CFLAGS=`echo "$CFLAGS" | $SED -e 's/-O[0-9]*//g'`
  changequote([,])

  dnl Add the special gcc flags
  CFLAGS="$CFLAGS -fprofile-arcs -ftest-coverage"
  LDFLAGS="$LDFLAGS -lgcov"
  AM_CONDITIONAL([ENABLE_COVERAGE], true)
else
  AM_CONDITIONAL([ENABLE_COVERAGE], false)
fi

#
# Populate contents of config.post.h
#
AC_SUBST(post_macros)
AM_SUBST_NOTMAKE(post_macros)

dnl ######################################################################
dnl Check how file descriptor transfers are supported between proceses.
dnl ######################################################################
AC_CHECK_MEMBER([struct msghdr.msg_control],
                [AC_DEFINE([HAVE_MSGHDR_MSG_CONTROL], [1], [Define to 1 if SCM_RIGHTS supported])],
                [AC_DEFINE([HAVE_NO_MSGHDR_MSG_CONTROL], [1], [Define to 1 if SCM_RIGHTS support])],
                [[#include <sys/types.h>
                  #include <sys/socket.h>]])

AC_CHECK_MEMBER([struct msghdr.msg_accrights],
                [AC_DEFINE([HAVE_MSGHDR_ACCRIGHTS], [1], [Define to 1 if BSD .msg_accrights supported])],
                [AC_DEFINE([HAVE_NO_MSGHDR_ACCRIGHTS], [1], [Define to 1 if no BSD .msg_accrights support])],
                [[#include <sys/types.h>
                  #include <sys/socket.h>]])

dnl ######################################################################
dnl Summarize
dnl ######################################################################

AC_MSG_RESULT( )

AC_MSG_RESULT(Summary:)
AC_MSG_RESULT(> Version: $cfengine_version)

AC_MSG_RESULT([> Required libraries])

AC_MSG_RESULT([-> OpenSSL: $OPENSSL_PATH])

AC_MSG_RESULT([-> PCRE: $PCRE_PATH])

if test $WITH_TOKYO = 1; then
  AC_MSG_RESULT([-> DB: Tokyo Cabinet: $TOKYOCABINET_PATH])
elif test $WITH_QDBM = 1; then
  AC_MSG_RESULT([-> DB: QDBM: $QDBM_PATH])
fi

AC_MSG_RESULT([> Optional libraries])

if test "x$ac_cv_lib_mysqlclient_mysql_real_connect" = "xyes"; then
  AC_MSG_RESULT([-> MySQL connector: $MYSQL_PATH])
else
  AC_MSG_RESULT([-> MySQL connector: disabled])
fi

if test "x$ac_cv_lib_pq_PQconnectdb" = "xyes"; then
  AC_MSG_RESULT([-> PostgreSQL connector: $POSTGRESQL_PATH])
else
  AC_MSG_RESULT([-> PostgreSQL connector: disabled])
fi

if test $WITH_LMDB = 1; then
  AC_MSG_RESULT([-> DB: Lightning MDB: $LMDB_PATH])
elif test $WITH_TOKYO = 1; then
  AC_MSG_RESULT([-> DB: Tokyo Cabinet: $TOKYOCABINET_PATH])
elif test $WITH_QDBM = 1; then
  AC_MSG_RESULT([-> DB: QDBM: $QDBM_PATH])
fi

if test "x$ac_cv_lib_virt_virConnectOpen" = xyes; then
  AC_MSG_RESULT([-> libvirt: $LIBVIRT_PATH])
else
  AC_MSG_RESULT([-> libvirt: disabled])
fi

if test "x$ac_cv_lib_acl_acl_init" = xyes; then
  AC_MSG_RESULT([-> libacl: $LIBACL_PATH])
else
  AC_MSG_RESULT([-> libacl: disabled])
fi

if test "x$ac_cv_lib_curl_curl_global_init" = xyes; then
  AC_MSG_RESULT([-> libcurl: $LIBCURL_PATH])
else
  AC_MSG_RESULT([-> libcurl: disabled])
fi

if test "x$ac_cv_lib_yaml_yaml_parser_initialize" = xyes; then
  AC_MSG_RESULT([-> libyaml: $LIBYAML_PATH])
else
  AC_MSG_RESULT([-> libyaml: disabled])
fi

if test "x$ac_cv_lib_xml2_xmlFirstElementChild" = xyes; then
  AC_MSG_RESULT([-> libxml2: $LIBXML2_PATH])
else
  AC_MSG_RESULT([-> libxml2: disabled])
fi

if test "x$users_promises_ok" = "xyes"; then
  AC_MSG_RESULT([-> User promises: PAM/user* tools])
else
  AC_MSG_RESULT([-> User promises: disabled])
fi

if test "x$enable_builtin_extensions" = "xyes"; then
  AC_MSG_RESULT([-> Enterprise extensions: Built in])
else
  AC_MSG_RESULT([-> Enterprise extensions: Plugin or not included])
fi

if test -n "$INIT_D_PATH"; then
  AC_MSG_RESULT([-> init.d script: $INIT_D_PATH])
else
  AC_MSG_RESULT([-> init.d script: disabled])
fi

if test -n "$SYSTEMD_SERVICE_PATH"; then
  AC_MSG_RESULT([-> Systemd service: $SYSTEMD_SERVICE_PATH])
else
  AC_MSG_RESULT([-> Systemd service: disabled])
fi

if test -n "$OS_ENVIRONMENT_PATH"; then
  AC_MSG_RESULT([-> Path of platform environment files: $OS_ENVIRONMENT_PATH])
fi

m4_indir(incstart[]incend, [nova/config.m4])

AC_MSG_RESULT([-> Workdir: $WORKDIR])
AC_MSG_RESULT([-> Masterdir: $MASTERDIR])
AC_MSG_RESULT([-> Inputdir: $INPUTDIR])
AC_MSG_RESULT([-> Logdir: $LOGDIR])
AC_MSG_RESULT([-> Piddir: $PIDDIR])
AC_MSG_RESULT([-> Statedir: $STATEDIR])
AC_MSG_RESULT([-> bindir: $bindir])

AC_MSG_RESULT( )

dnl ######################################################################
dnl Now make the Makefiles
dnl ######################################################################

AC_CONFIG_FILES([Makefile
    libcompat/Makefile
    libutils/Makefile
    libutils/config.post.h
    libcfnet/Makefile
    libenv/Makefile
    libpromises/Makefile
    cf-agent/Makefile
    cf-promises/Makefile
    cf-execd/Makefile
    cf-key/Makefile
    cf-monitord/Makefile
    cf-upgrade/Makefile
    cf-runagent/Makefile
    cf-serverd/Makefile
    cf-testd/Makefile
    cf-net/Makefile
    misc/Makefile
    ext/Makefile
    examples/Makefile
    tests/Makefile
    tests/acceptance/Makefile
    tests/acceptance/25_cf-execd/Makefile
    tests/unit/Makefile
    tests/load/Makefile])

AC_OUTPUT

AC_MSG_RESULT(DONE: Configuration done. Run make/gmake to build CFEngine Community.)
