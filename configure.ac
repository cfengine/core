dnl ##########################################################################
dnl #
dnl # Build CFEngine
dnl #
dnl # Run ./autogen.sh to build configure script
dnl #
dnl ##########################################################################

AC_PREREQ(2.59)

AC_INIT
AC_CONFIG_SRCDIR([libpromises/generic_agent.c])
AC_CANONICAL_TARGET

dnl
dnl This program needs to be checked early, as MAKEINFO variable is expanded in
dnl AM_INIT_AUTOMAKE.
dnl
AC_CHECK_PROG(MAKEINFO, makeinfo, makeinfo)

define([revision], esyscmd([sh -c "(git rev-list -1 --abbrev-commit HEAD || echo unknown) | tr -d '\n'" 2>/dev/null]))dnl

dnl
dnl The version in the next line is the only one to set
dnl

_AM_SET_OPTION([tar-ustar])
AM_INIT_AUTOMAKE(cfengine, 3.5.0b2.revision)
AM_MAINTAINER_MODE([enable])

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

dnl ######################################################################
dnl Use pthreads if available
dnl ######################################################################

AC_ARG_WITH([pthreads],
    [AS_HELP_STRING([--with-pthreads[[=PATH]]],
                    [Specify path to pthreads, if not the part of operating system])])

if test "x$with_pthreads" != x && test "x$with_pthreads" != "xyes" && test "x$with_pthreads" != "xno"; then
   LIBS="$LIBS -L$with_pthreads/lib"
   CFLAGS="$CFLAGS -I$with_pthreads/include"
fi

ACX_PTHREAD([],
            [AC_MSG_ERROR(pthread-compatible library is required to build CFEngine)])

CC="$PTHREAD_CC"
CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"

dnl ######################################################################
dnl Checks for programs.
dnl ######################################################################

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_MKDIR_P
AC_EXEEXT

# Use either new LT_INIT or old AC_DISABLE_STATIC/AC_PROG_LIBTOOL macros
m4_ifdef([LT_INIT],
  [LT_INIT([disable-static])],
  [AC_DISABLE_STATIC
   AC_PROG_LIBTOOL])

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL
AC_CONFIG_LIBOBJ_DIR(libcompat)
AC_FUNC_GETLOADAVG 
AC_PATH_PROG(GETCONF, getconf, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin:/sw/bin)

AM_CONDITIONAL(CROSS_COMPILING, test "x$cross_compiling" = "xyes")

#
# Add to the default list of places in CPPFLAGS to match LDFLAGS above
# Don't add if cross-compiling (setting host), to avoid using native libs.
#
if test "x$cross_compiling" = "xno"; then
  for x in /usr/local/include /usr/local/gnu/include /opt/dce/include /sw/include /usr/pkg/include /usr/X11R7/include
  do
    if test -d "$x"; then
      y=`expr " $CPPFLAGS " : ".* -I$x "`
      if test $y -eq 0; then
        CPPFLAGS="$CPPFLAGS -I$x"
      fi
    fi
  done
fi

dnl ######################################################################
dnl Configure files layout
dnl ######################################################################

AC_ARG_ENABLE([fhs],
        [AS_HELP_STRING([--enable-fhs], [Enable FHS compliance. Defaults to custom CFEngine files layout])])

#
# pkglibdir/pkgdatadir are not overridable, so use our own invention instead.
#

AS_IF([test x"$enable_fhs" = xyes], [
  projlibdir='${libdir}/cfengine'
  projdatadir='${exec_prefix}/share/cfengine'
  projdocdir='${exec_prefix}/share/doc/cfengine'
  WORKDIR='${localstatedir}/lib/cfengine'
], [
  if test x"$prefix" = xNONE || test x"$prefix" = x/var/cfengine; then
    prefix=/var/cfengine
    case "$target_os" in
      mingw*)
        WORKDIR=$(cmd /c "echo %PROGRAMFILES%\\Cfengine" | sed 's/\\/\\\\/g');;
      *)
        WORKDIR=/var/cfengine;;
    esac
  else
    WORKDIR="${localstatedir}/cfengine"
  fi

  sbindir='${exec_prefix}/bin' # /var/cfengine/bin despite being sbin_?
  projlibdir='${exec_prefix}/lib'
  projdatadir='${exec_prefix}/share'
  projdocdir='${exec_prefix}/share/doc'
  mandir='${exec_prefix}/share/man'
])

AC_SUBST(projlibdir)
AC_SUBST(projdatadir)
AC_SUBST(projdocdir)

dnl ######################################################################
dnl Enable debugging
dnl ######################################################################

AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug], [Enable debugging]),
              [debug=$enableval],
              [debug=no])
AM_CONDITIONAL([NDEBUG], [test x"$debug" = x"no"])

if test x"$debug" = x"yes"; then
   CFLAGS="$CFLAGS -g3 -O0"
else
   CFLAGS="$CFLAGS -O2 -DNDEBUG"
fi

dnl ######################################################################
dnl Checks for libraries.
dnl ######################################################################

dnl Now check for database connectors

AC_ARG_WITH([sql],
    [AS_HELP_STRING([--with-sql[[=PATH]]], [Enable SQL database connectors (deprecated, use --with[[out]]-postgresql and --with[[out]]-mysql instead)])],[with_postgresql=$with_sql; with_mysql=$with_sql],[])

dnl PostgreSQL

AC_ARG_WITH([postgresql],
    [AS_HELP_STRING([--with-postgresql[[=PATH]]],
                    [Enable PostgreSQL connector])], [], [with_postgresql=check])

if test "x$with_postgresql" != "xno"; then

   if test "x$with_postgresql" != xyes && test "x$with_postgresql" != xcheck; then
      PG_CONFIG=$with_postgresql/bin/pg_config
   else
      PG_CONFIG=pg_config
   fi

   # pg_config is only for native builds
   if test "x$cross_compiling" = "xno"; then
      if test x`which $PG_CONFIG` != x ; then
         CPPFLAGS="$CPPFLAGS -I`$PG_CONFIG --includedir`"
      fi
   fi

   CF3_WITH_LIBRARY(postgresql, [
     AC_CHECK_LIB(pq, PQconnectdb, [], [if test "x$with_postgresql" != xcheck; then AC_MSG_ERROR(Cannot find PostgreSQL client library); fi])
     AC_CHECK_HEADERS(libpq-fe.h, [], [if test "x$with_postgresql" != xcheck; then AC_MSG_ERROR(Cannot find PostgreSQL client library); fi])
   ])
fi

dnl MySQL

AC_ARG_WITH([mysql],
    [AS_HELP_STRING([--with-mysql[[=PATH]]],
                    [Enable MySQL connector])], [], [with_mysql=check])

if test "x$with_mysql" != "xno"; then

   if test "x$with_mysql" != xyes && test "x$with_mysql" != xcheck; then
      MYSQL_CONFIG=$with_mysql/bin/mysql_config
   else
      MYSQL_CONFIG=mysql_config
   fi

   # mysql_config is only for native builds
   if test "x$cross_compiling" = "xno"; then
      if test x`which $MYSQL_CONFIG` != x ; then
         CPPFLAGS="$CPPFLAGS `$MYSQL_CONFIG --include`"
      fi
   fi

   CF3_WITH_LIBRARY(mysql, [
     AC_CHECK_LIB(mysqlclient, mysql_real_connect, [], [if test "x$with_mysql" != xcheck; then AC_MSG_ERROR(Cannot find MySQL client library); fi])
     AC_CHECK_HEADERS(mysql.h, [], [if test "x$with_mysql" != xcheck; then AC_MSG_ERROR(Cannot find MySQL client library); fi])

     AC_CHECK_LIB(mysqlclient, EVP_CIPHER_CTX_init,
                               [AC_MSG_ERROR([MySQL client library exports symbols clashing with OpenSSL. Get the update from distribution provider, recompile MySQL library or disable MySQL connector. See http://bugs.mysql.com/bug.php?id=65055 for details.])])
   ])
fi

dnl Enterprise extensions

dnl Mongo is disabled unless overriden in subproject
AM_CONDITIONAL(MONGO, false)

m4_indir(incstart[]incend, [nova/options.m4])
AM_CONDITIONAL([HAVE_NOVA], [test "x$with_nova" != xno && test -d ${srcdir}/nova])

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

AS_IF([test $WITH_QDBM -eq 0 && (! test -n "$with_tokyocabinet" || test "x$with_tokyocabinet" != "xno")], [WITH_TOKYO=1], [WITH_TOKYO=0])

if test $WITH_TOKYO = 1; then
  CF3_WITH_LIBRARY(tokyocabinet, [
    AC_CHECK_LIB(tokyocabinet, tchdbnew, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_CHECK_HEADERS(tcutil.h, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_CHECK_HEADERS(tchdb.h, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_DEFINE(TCDB, 1, [Define if Tokyo Cabinet is available.])
  ])
fi

if test $WITH_QDBM -eq 0 && test $WITH_TOKYO -eq 0; then
   AC_MSG_ERROR(Either Tokyo Cabinet or QDBM must be selected to compile CFEngine)
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

CF3_WITH_LIBRARY(openssl, [
   AC_CHECK_LIB(crypto, RSA_generate_key_ex, [], [])
   AC_CHECK_LIB(crypto, RSA_generate_key, [], [])
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

if test "x$with_libvirt" != xno; then
   CF3_WITH_LIBRARY(libvirt, [
      AC_CHECK_LIB(virt, virConnectOpen, [], [if test "x$with_libvirt" != xcheck; then AC_MSG_ERROR(Cannot find libvirt library); fi])
      AC_CHECK_HEADERS(libvirt/libvirt.h, [], [if test "x$with_libvirt" != xcheck; then AC_MSG_ERROR(Cannot find libvirt library headers); fi])
   ])
fi

dnl libacl

AC_ARG_WITH([libacl],
    [AS_HELP_STRING([--with-libacl[[=PATH]]], [Specify libacl path])], [], [with_libacl=check])

if test "x$with_libacl" != xno; then
  CF3_WITH_LIBRARY(libacl, [
    AC_CHECK_LIB(acl, acl_init, [], [if test "x$with_libacl" != xcheck; then AC_MSG_ERROR(Cannot find libacl library); fi])
    AC_CHECK_HEADERS([acl.h sys/acl.h acl/libacl.h], [], [if test "x$with_libacl" != xcheck; then AC_MSG_ERROR(Cannot find libacl library headers); fi])
  ])
 fi

dnl libxml2

AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--with-libxml2[[=PATH]]],
                    [Specify libxml2 path])], [], [with_libxml2=check])

if test "x$with_libxml2" != xno; then

   if test "x$with_libxml2" != xyes && test "x$with_libxml2" != xcheck; then
      XML2_CONFIG=$with_libxml2/bin/xml2-config
   else
      XML2_CONFIG=xml2-config
   fi

   # xml2-config is only for native builds
   if test "x$cross_compiling" = "xno"; then
      if test x`which $XML2_CONFIG` != x ; then
         CPPFLAGS="$CPPFLAGS `$XML2_CONFIG --cflags`"
      fi
   fi

    CF3_WITH_LIBRARY(libxml2, [
      AC_CHECK_LIB(xml2, xmlFirstElementChild, [], [if test "x$with_libxml2" != xcheck; then AC_MSG_ERROR(Cannot find libxml2); fi])
      AC_CHECK_HEADERS([libxml/xmlwriter.h], [break], [if test "x$with_libxml2" != xcheck; then AC_MSG_ERROR(Cannot find libxml2); fi])
    ])
fi

AM_CONDITIONAL([HAVE_LIBXML2], [test "x$with_libxml2" != "xno" && test "x$ac_cv_lib_xml2_xmlFirstElementChild" = xyes])

dnl avahi

AC_CHECK_HEADERS(avahi-client/client.h, AM_CONDITIONAL(HAVE_AVAHI_CLIENT, true), AM_CONDITIONAL(HAVE_AVAHI_CLIENT, false), [])
AC_CHECK_HEADERS(avahi-common/address.h, AM_CONDITIONAL(HAVE_AVAHI_COMMON, true), AM_CONDITIONAL(HAVE_AVAHI_COMMON, false),[])

dnl

dnl ######################################################################
dnl Checks for header files.
dnl ######################################################################

AC_CHECK_HEADERS(unistd.h stdlib.h sys/loadavg.h)
AC_CHECK_HEADERS(sys/param.h)

# sys/param.h is required for sys/mount.h on OpenBSD
AC_CHECK_HEADERS(sys/mount.h, [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

# Required on BSD to get struct sockaddr_dl (for retrieving MAC addresses from getifaddrs())
AC_CHECK_HEADERS(net/if_dl.h)

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
AC_CHECK_HEADERS(winsock2.h)
AC_CHECK_HEADERS(ws2tcpip.h)
AC_CHECK_HEADERS(zone.h)
AC_CHECK_HEADERS(sys/uio.h)
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(sys/mpctl.h) dnl For HP-UX $(sys.cpus) - Mantis #1069
AC_CHECK_HEADERS(sys/jail.h, [], [], [AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

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
AC_CHECK_TYPES(clockid_t)
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
  CPPFLAGS="$CPPFLAGS -D_LARGE_FILES=1"
fi

dnl ######################################################################
dnl Check for libraries
dnl ######################################################################

AC_CHECK_LIB(m, sqrt)
AC_CHECK_LIB(rt, clock_gettime)

dnl ######################################################################
dnl Check for special functions
dnl ######################################################################

AC_CHECK_DECLS(uname)
AC_REPLACE_FUNCS(uname)

AC_CHECK_DECLS(getuid)
AC_REPLACE_FUNCS(getuid)

AC_CHECK_DECLS(getgid)
AC_REPLACE_FUNCS(getgid)

AC_CHECK_DECLS(clock_gettime, [], [], [[#include <time.h>]])
AC_REPLACE_FUNCS(clock_gettime)

AC_CHECK_DECLS(unsetenv)
AC_REPLACE_FUNCS(unsetenv)

AC_CHECK_DECLS(strnlen)
AC_REPLACE_FUNCS(strnlen)

AC_CHECK_DECLS(strndup)
AC_REPLACE_FUNCS(strndup, [], [], [#define _GNU_SOURCE 1
AC_INCLUDES_DEFAULT])

AC_CHECK_DECLS(setlinebuf)
AC_REPLACE_FUNCS(setlinebuf)

AC_CHECK_DECLS(strlcat)
AC_REPLACE_FUNCS(strlcat)

AC_CHECK_DECLS(strlcpy)
AC_REPLACE_FUNCS(strlcpy)

AC_CHECK_DECLS(realpath)
AC_CHECK_FUNCS(realpath)

AC_CHECK_DECLS(lstat)
AC_REPLACE_FUNCS(lstat)

AC_CHECK_DECLS(strdup)
AC_REPLACE_FUNCS(strdup)

AC_CHECK_DECLS(round, [], [], [[#include <math.h>]])
AC_REPLACE_FUNCS(round)

AC_CHECK_FUNCS(sleep)
AC_REPLACE_FUNCS(sleep)

AC_CHECK_DECLS(nanosleep)
AC_REPLACE_FUNCS(nanosleep)

AC_CHECK_DECLS(memdup)
AC_REPLACE_FUNCS(memdup)

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

AC_CHECK_DECLS(localtime_r, [], [], [[#include <time.h>]])
AC_REPLACE_FUNCS(localtime_r)

AC_CHECK_DECLS(alarm)
AC_REPLACE_FUNCS(alarm)

AC_CHECK_DECLS(chmod)
AC_REPLACE_FUNCS(chmod)

AC_CHECK_FUNCS(getpwent setpwent endpwent)

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

AC_CHECK_FUNCS(sendto)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
                                     #include <sys/socket.h>]], 
                                   [[extern ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)]])],
    [AC_DEFINE([SENDTO_RETURNS_SSIZE_T], 1, [Whether sendto returns ssize_t])],
    [AC_DEFINE([SENDTO_RETURNS_SSIZE_T], 0, [Whether sendto does not returns ssize_t])])

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

CF3_CHECK_PROPER_FUNC([ctime],
  [],
  [[#error ctime(3) may produce different results on different OSes. Let's have our POSIX-compliant implementation all the time]],
  [#define ctime rpl_ctime])
 
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
   AC_MSG_RESULT(Unable to find out how to print intmax_t/uintmax_t types)])

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
AC_CHECK_DECLS(getaddrinfo, [], [], [[
#if HAVE_WINSOCK2_H
    #include <winsock2.h>
#endif
#if HAVE_WS2TCPIP_H
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netdb.h>
#endif]])
AC_REPLACE_FUNCS(getaddrinfo)

AC_CHECK_DECLS([[inet_ntop], [inet_pton]], [], [], [[#include <arpa/inet.h>]])
AC_REPLACE_FUNCS(inet_ntop inet_pton)

AC_CHECK_FUNCS(getifaddrs)

AC_CHECK_FUNC(lchown, AC_DEFINE(HAVE_LCHOWN, 1, [Whether to use lchown(3) to change ownerships]))

AC_CHECK_DECLS(chown)
AC_REPLACE_FUNCS(chown)

AC_CHECK_FUNCS(fchmod)
AC_REPLACE_FUNCS(fchmod)

AC_CHECK_DECLS(pthread_attr_setstacksize, [], [], [[#include <pthread.h>]])
AC_REPLACE_FUNCS(pthread_attr_setstacksize)

AC_CHECK_DECLS(pthread_sigmask, [], [], [[#include <signal.h>]])
AC_REPLACE_FUNCS(pthread_sigmask)

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

dnl ######################################################################
dnl Give the chance to enable SELINUX
dnl ######################################################################

AC_ARG_ENABLE([selinux], [AS_HELP_STRING([--enable-selinux], [Enable SELinux support])])
AS_IF([test "x$enable_selinux" = "xyes"], [
    AC_DEFINE(WITH_SELINUX, 1, [Define if you want to use SELinux])
    LIB_SELINUX="-lselinux"
    LIBS="$LIBS $LIB_SELINUX"
    AC_SUBST(LIB_SELINUX)
])

dnl ######################################################################
dnl Collect all the options
dnl ######################################################################

CPPFLAGS="$CPPFLAGS $TOKYOCABINET_CPPFLAGS $QDBM_CPPFLAGS $PCRE_CPPFLAGS $OPENSSL_CPPFLAGS $MONGO_CPPFLAGS $SQLITE3_CPPFLAGS"
CFLAGS="$CFLAGS $TOKYOCABINET_CFLAGS $QDBM_CFLAGS $PCRE_CFLAGS $OPENSSL_CFLAGS $MONGO_CFLAGS $SQLITE3_CFLAGS"
LDFLAGS="$LDFLAGS $TOKYOCABINET_LDFLAGS $QDBM_LDFLAGS $PCRE_LDFLAGS $OPENSSL_LDFLAGS $MONGO_LDFLAGS $SQLITE3_LDFLAGS"
LIBS="$LIBS $TOKYOCABINET_LIBS $QDBM_LIBS $PCRE_LIBS $OPENSSL_LIBS $MONGO_LIBS $SQLITE3_LIBS"

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
        ;;
   aix*)
        CPPFLAGS="$CPPFLAGS -w"
        ;;
   linux*)
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
   gnu*)
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

#
# OS kernels conditionals. Don't use those unless it is really needed (if code
# depends on the *kernel* feature, and even then -- some kernel features are
# shared by different kernels).
#
# Good example: use LINUX to select code which uses inotify and netlink sockets.
# Bad example: use LINUX to select code which parses output of coreutils' ps(1).
#
AM_CONDITIONAL([LINUX], [test -n "`echo ${target_os} | grep linux`"])
AM_CONDITIONAL([SOLARIS], [test -n "`(echo ${target_os} | egrep 'solaris|sunos')`"])
AM_CONDITIONAL([NT], [test -n "`(echo ${target_os} | egrep 'mingw|cygwin')`"])
AM_CONDITIONAL([CYGWIN], [test -n "`(echo ${target_os} | egrep 'cygwin')`"])
AM_CONDITIONAL([AIX], [test -n "`(echo ${target_os} | grep aix)`"])
AM_CONDITIONAL([HPUX], [test -n "`(echo ${target_os} | egrep 'hpux|hp-ux')`"])
AM_CONDITIONAL([FREEBSD], [test -n "`(echo ${target_os} | grep freebsd)`"])
AM_CONDITIONAL([NETBSD], [test -n "`(echo ${target_os} | grep netbsd)`"])
AM_CONDITIONAL([XNU], [test -n "`(echo ${target_os} | grep darwin)`"])

dnl #####################################################################
dnl Configure LOCKDIR and LOGDIR
dnl #####################################################################

AC_ARG_WITH(workdir,
    [  --with-workdir=WORKDIR  default internal for trusted cache ],
    [
        if test x$withval != x ; then
            WORKDIR=$withval
        fi
    ],
)

dnl Expand ${prefix} and whatnot in WORKDIR

adl_RECURSIVE_EVAL("${WORKDIR}", WORKDIR)

AC_DEFINE_UNQUOTED(WORKDIR, "${WORKDIR}", [Workdir location])
AC_SUBST(workdir, "${WORKDIR}")


AC_ARG_WITH(shell, [AS_HELP_STRING([--with-shell=PATH],
                   [Specify path to POSIX-compatible shell (if not /bin/sh)])],
                   [],
                   [with_shell=/bin/sh])

if test "x$with_shell" = "xno"; then
   AC_MSG_ERROR([Please specify full path to POSIX-compatible shell])
fi
AC_DEFINE_UNQUOTED(SHELL_PATH, "$with_shell", [Path to the POSIX-compatible shell])

dnl ####################################################################
dnl Set GCC CFLAGS only if using GCC.
dnl ####################################################################

AC_MSG_CHECKING(for HP-UX aC)
AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#if defined __HP_cc
#This is HP-UX ANSI C
#endif
]])], [AC_MSG_RESULT(no)],[AC_MSG_RESULT(yes)
CFLAGS="$CFLAGS -Agcc"
CPPFLAGS="$CPPFLAGS -Agcc"
HP_UX_AC=yes])

AC_MSG_CHECKING(for GCC specific compile flags)
if test x"$GCC" = "xyes" && test x"$HP_UX_AC" != x"yes"; then
    CFLAGS="$CFLAGS -g -Wall"
    CPPFLAGS="$CPPFLAGS -std=gnu99"
    AC_MSG_RESULT(yes)

    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Wno-pointer-sign"
    AC_MSG_CHECKING(for -Wno-pointer-sign)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)],
     [AC_MSG_RESULT(no)
     CFLAGS="$save_CFLAGS"])

    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Werror=implicit-function-declaration"
    AC_MSG_CHECKING(for -Werror=implicit-function-declaration)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)],
     [AC_MSG_RESULT(no)
     CFLAGS="$save_CFLAGS"])

    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Wunused-parameter"
    AC_MSG_CHECKING(for -Wunused-parameter")
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)],
     [AC_MSG_RESULT(no)
     CFLAGS="$save_CFLAGS"])
else 
    AC_MSG_RESULT(no)
fi

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
        i[[3456]]86*|x86_64*)
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
  CFLAGS="$CFLAGS -O0 -fprofile-arcs -ftest-coverage"
  LDFLAGS="$LDFLAGS -lgcov"
fi

dnl
dnl Documentation generation
dnl

AC_ARG_ENABLE(docs, AC_HELP_STRING([--enable-docs=html|pdf|all],
                                   [Enable generation of documentation in specified formats]),
                    [use_docs=$enableval], [use_docs=no])

if test "x$use_docs" != "xno"; then
   if test "x$use_docs" = "xhtml" || test "x$use_docs" = "xall"; then
     if test "$MAKEINFO" = "${am_missing_run}makeinfo"; then
      AC_MSG_ERROR([makeinfo tool is required for HTML documentation generation])
     fi
   fi

   if test "x$use_docs" = "xpdf" || test "x$use_docs" = "xall"; then
      AC_CHECK_PROG(TEXI2PDF, texi2pdf, texi2pdf)

      if test -z "$TEXI2PDF"; then
        AC_MSG_ERROR([texi2pdf tool is required for PDF documentation generation])
      fi
   fi
fi

AM_CONDITIONAL(HTML_DOCS, test "x$use_docs" = "xhtml" || test "x$use_docs" = "xall")
AM_CONDITIONAL(PDF_DOCS, test "x$use_docs" = "xpdf" || test "x$use_docs" = "xall")

#
# Populate contents of config.post.h
#
AC_SUBST(post_macros)
AM_SUBST_NOTMAKE(post_macros)

dnl ######################################################################
dnl Summarize
dnl ######################################################################

AC_MSG_RESULT( )
AC_MSG_RESULT(    Summary of options...)

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

if test "x$ac_cv_lib_xml2_xmlFirstElementChild" = xyes; then
  AC_MSG_RESULT([-> libxml2: $LIBXML2_PATH])
else
  AC_MSG_RESULT([-> libxml2: disabled])
fi

m4_indir(incstart[]incend, [nova/config.m4])

AC_MSG_RESULT([-> Workdir: $WORKDIR])

AC_MSG_RESULT( )



dnl ######################################################################
dnl Now make the Makefiles
dnl ######################################################################

AC_CONFIG_FILES([Makefile
    libcompat/Makefile
    libutils/Makefile
    libutils/config.post.h
    libcfnet/Makefile
    libpromises/Makefile
    cf-agent/Makefile
    cf-promises/Makefile
    cf-execd/Makefile
    cf-gendoc/Makefile
    cf-key/Makefile
    cf-monitord/Makefile
    cf-runagent/Makefile
    cf-serverd/Makefile
    ext/Makefile
    docs/Makefile
    docs/guides/Makefile
    docs/manpages/Makefile
    docs/reference/Makefile
    docs/tools/Makefile
    docs/tex-include/Makefile
    examples/Makefile
    masterfiles/Makefile
    tests/Makefile
    tests/acceptance/Makefile
    tests/unit/Makefile
    tests/load/Makefile])

m4_indir(incstart[]incend, [nova/output.m4])

AC_OUTPUT

AC_MSG_RESULT(DONE: Configuration done. Run make/gmake to build CFEngine.)
