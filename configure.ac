dnl ##########################################################################
dnl #
dnl # BUILD CFENGINE
dnl #
dnl # Run ./autogen.sh to build configure script
dnl #
dnl ##########################################################################

AC_PREREQ(2.59)

AC_INIT
AC_CONFIG_SRCDIR([src/cfpromises.c])
AC_CANONICAL_TARGET

define([svnversion], [r]esyscmd([sh -c "svnversion --no-newline || echo exported | tr -d '\n'" 2>/dev/null]))dnl


dnl
dnl The version in the next line is the only one to set
dnl
dnl 'svnversion' need to be removed before release and added back after.
dnl

AM_INIT_AUTOMAKE(cfengine, 3.2.4)   dnl remember to set version
AM_MAINTAINER_MODE([enable])

AC_DEFINE(BUILD_YEAR, esyscmd([date +%Y | tr -d '\n']), "Software build year")

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_HEADERS([src/conf.h])

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

ACX_PTHREAD

CC="$PTHREAD_CC"
CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"

dnl ######################################################################
dnl Checks for programs.
dnl ######################################################################

AC_PROG_CC
AC_EXEEXT

# Use either new LT_INIT or old AC_DISABLE_STATIC/AC_PROG_LIBTOOL macros
m4_ifdef([LT_INIT],
  [LT_INIT([disable-static])],
  [AC_DISABLE_STATIC
   AC_PROG_LIBTOOL])

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL
AC_CONFIG_LIBOBJ_DIR(pub)
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
dnl Checks for libraries.
dnl ######################################################################

AC_ARG_WITH(diagnostics,
	AS_HELP_STRING(--with-diagnostics, [enable test suite]),[],
	with_diagnostics=no)

if test "x$with_diagnostics" != xno; then
   AC_DEFINE(BUILD_TESTSUITE, 1, [Build Test suite])
fi

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
   ])
fi

dnl Enterprise extensions

m4_indir(incstart[]incend, [nova/options.m4])
AM_CONDITIONAL([HAVE_NOVA], [test "x$with_nova" != xno && test -d ${srcdir}/nova])
m4_indir(incstart[]incend, [constellation/options.m4])
AM_CONDITIONAL([HAVE_CONSTELLATION], [test "x$with_constellation" != xno && test -d ${srcdir}/constellation])
m4_indir(incstart[]incend, [galaxy/options.m4])
AM_CONDITIONAL([HAVE_GALAXY], [test "x$with_galaxy" != xno && test -d ${srcdir}/galaxy])

dnl Tokyo Cabinet

AC_ARG_WITH([tokyocabinet],
    [AS_HELP_STRING([--with-tokyocabinet[[=PATH]]], [use Tokyo Cabinet to store runtime data])])

AS_IF([test -n "$with_tokyocabinet" && test "x$with_tokyocabinet" != "xno"], [WITH_TOKYO=1], [WITH_TOKYO=0])

if test $WITH_TOKYO = 1; then
  CF3_WITH_LIBRARY(tokyocabinet, [
    AC_CHECK_LIB(tokyocabinet, tchdbnew, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_CHECK_HEADERS(tcutil.h, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_CHECK_HEADERS(tchdb.h, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
    AC_DEFINE(TCDB, 1, [Define if Tokyo Cabinet is available.])
  ])
fi

dnl SQLite 3

AC_ARG_WITH([sqlite3],
  [AS_HELP_STRING([--with-sqlite3[[=PATH]]], [use SQLite 3 to store runtime data (EXPERIMENTAL, BROKEN)])])
AS_IF([test "x$with_sqlite3" = "xyes"], [WITH_SQLITE3=1], [WITH_SQLITE3=0])

if test $WITH_SQLITE3 = 1; then
  AC_MSG_CHECKING(SQLite 3)
  AC_CHECK_LIB(sqlite3, sqlite3_open, [], [AC_MSG_ERROR(Cannot find SQLite 3)])
  AC_CHECK_HEADERS(sqlite3.h)
  AC_DEFINE(SQLITE3, 1, [Define if SQLite 3 is available.])
fi

dnl QDBM

AC_ARG_WITH([qdbm],
    [AS_HELP_STRING([--with-qdbm[[=PATH]]], [use QDBM to store runtime data])])
AS_IF([test "x$with_qdbm" = "xyes"], [WITH_QDBM=1], [WITH_QDBM=0])

if test $WITH_QDBM = 1; then
  AC_MSG_CHECKING(for QDBM)
  AC_CHECK_LIB(qdbm,main, [], [AC_MSG_ERROR(Cannot find Quick Database Manager)])
  AC_CHECK_HEADERS(depot.h)
  AC_DEFINE(QDB, 1, [Define if QDBM is available.])
fi

dnl Berkeley DB.

if test $WITH_QDBM = 0 -a $WITH_TOKYO = 0 -a $WITH_SQLITE3 = 0; then
  AC_ARG_WITH(berkeleydb,
    [  --with-berkeleydb[[=PATH]] directory where BerkeleyDB exists],
    BERKELEYDB_DIR=$with_berkeleydb,
    BERKELEYDB_DIR=default)

  AC_MSG_CHECKING(for BerkeleyDB location in $BERKELEYDB_DIR)

  if test "x$BERKELEYDB_DIR" = "xno" ; then
    AC_MSG_RESULT( )
    AC_MSG_ERROR(This release of cfengine requires a version of BerkeleyDB 4.4 or later)
  else
    if test "x$BERKELEYDB_DIR" = "xdefault" ; then
        for v in BerkeleyDB.4.2 BerkeleyDB.4.3 BerkeleyDB.4.4 BerkeleyDB.4.5 BerkeleyDB.4.6 BerkeleyDB.4.7 BerkeleyDB.4.8; do
            for d in $prefix /opt /usr/local /usr/pkg /usr; do
                test -d "$d/$v" && BERKELEYDB_DIR="$d/$v"
            done
        done
    fi

    if test "x$BERKELEYDB_DIR" = "xdefault" ; then
        for d in $prefix /opt /usr/local /usr; do
            for v in db-4 db4 db3 db db40 db41 db42 db43 db44 db45 db46 db47 db48 db50 db51; do

                if test -f "$d/include/$v/db.h" ; then
                    echo "Found header in $d/include/$v "
                    test "x$d" != "x/usr" && BERKELEYDB_LDFLAGS="-L$d/lib/$v"
                    BERKELEYDB_CFLAGS="-I$d/include/$v"
                    late_LIBS=$LIBS
		    # In RedHat 8, for instance, we have /usr/include/db4
		    # and libdb-4.0.a.  Debian has /usr/lib/libdb-4.1.a, for
		    # instance.  Look for the appropriate library.
                    if test $v = db4 -o $v = db40; then
			save_CFLAGS="$CFLAGS"
			save_LDFLAGS="$LDFLAGS"
			CFLAGS="$CFLAGS $BERKELEYDB_CFLAGS"
			LDFLAGS="$LDFLAGS $BERKELEYDB_LDFLAGS"
			AC_SEARCH_LIBS(db_create, 
				[db-4 db4 db-4.8 db-4.7 db-4.6 db-4.5 db-4.4 db-4.3 db-4.2 db-4.1 db-4.0], 
				[BERKELEYDB_LIBS=$ac_cv_search_db_create])
			CFLAGS="$save_CFLAGS"
			LDFLAGS="$save_LDFLAGS"
		    else
                        BERKELEYDB_LIBS="-ldb"
                    fi
		    LIBS=$late_LIBS
                    AC_MSG_RESULT($d)
                    break
                fi
            done

            test "x$BERKELEYDB_LIBS" != "x" && break
            if test -f "$d/include/db.h"; then
                if test "x$d" != "x/usr"; then
                    BERKELEYDB_LDFLAGS="-L$d/lib64 -L$d/lib"
                    BERKELEYDB_CFLAGS="-I$d/include"
                fi
                BERKELEYDB_LIBS="-ldb"
                AC_MSG_RESULT($d)
                break
            fi
	done

        if test "x$BERKELEYDB_LIBS" = "x" ; then
            AC_MSG_ERROR(Cannot find BerkeleyDB)
        fi

    elif test -f "$BERKELEYDB_DIR/include/db.h";then
        case "$target_os" in
        solaris*)
            #If we are staticlly linking the BDB files, we do not want a
            #-R flag.  If .so's are present, assume we are dynamic linking
            if test -n "`ls $BERKELEYDB_DIR/lib/*.so 2>/dev/null`"
            then
                BERKELEYDB_LDFLAGS="-L$BERKELEYDB_DIR/lib -R$BERKELEYDB_DIR/lib"
            else
                BERKELEYDB_LDFLAGS="-L$BERKELEYDB_DIR/lib"
            fi;;
        *)
            BERKELEYDB_LDFLAGS="-L$BERKELEYDB_DIR/lib";;
        esac
  
        BERKELEYDB_CFLAGS="-I$BERKELEYDB_DIR/include"
        BERKELEYDB_LIBS="-ldb"
        AC_MSG_RESULT($BERKELEYDB_DIR)

    elif test -d "$BERKELEYDB_DIR"; then

        BERKELEYDB_CFLAGS="-I$BERKELEYDB_DIR/include"
        for v in . db48 db47 db46 db45 db44 db43 db42 db41 db40 db4 db33 db32 db3 db; do
            if test -f "$BERKELEYDB_DIR/include/$v/db.h"; then
                BERKELEYDB_INCLUDEDIR="$BERKELEYDB_DIR/include/$v"
                BERKELEYDB_CFLAGS="-I$BERKELEYDB_INCLUDEDIR"
		break
            fi
        done

        if test -f "$BERKELEYDB_INCLUDEDIR/db.h"; then
            BERKELEYDB_LIBS="-ldb"
            for v in db-4.8 db48 db-4.7 db4.7 db47 db-4.6 db4.6 db46 db-4.5 db4.5 db45 db-4.4 db4.4 db44; do
                if test -f "$BERKELEYDB_DIR/lib/lib$v.so"; then
                    BERKELEYDB_LIBS="-l$v"
                    break
                fi
                if test -f "$BERKELEYDB_DIR/lib64/lib$v.so"; then
                    BERKELEYDB_LIBS="-l$v"
                    break
                fi
            done

            case "$target_os" in
            solaris*)
                #If we are staticlly linking the BDB files, we do not want a
                #-R flag.  If .so's are present, assume we are dynamic linking
                if test -n "`ls $BERKELEYDB_DIR/lib/*.so 2>/dev/null`"
                then
                    BERKELEYDB_LDFLAGS="-L$BERKELEYDB_DIR/lib -R$BERKELEYDB_DIR/lib"
                else
                    BERKELEYDB_LDFLAGS="-L$BERKELEYDB_DIR/lib"
                fi;;
            *)         BERKELEYDB_LDFLAGS="-L$BERKELEYDB_DIR/lib";;

            esac
  
            AC_MSG_RESULT($BERKELEYDB_DIR)
        else
           AC_MSG_ERROR(Cannot find BerkeleyDB)
        fi

    else
        AC_MSG_ERROR(Cannot find BerkeleyDB)
    fi
    AC_DEFINE(USE_BERKELEYDB, 1, [Define if BerkeleyDB is available.])

    BERKELEYDB_SAVE_LDFLAGS=$LDFLAGS
    BERKELEYDB_SAVE_CPPFLAGS=$CPPFLAGS
    BERKELEYDB_SAVE_LIBS=$LIBS
    LDFLAGS="$LDFLAGS $BERKELEYDB_LDFLAGS"
    CPPFLAGS="$CFLAGS $BERKELEYDB_CFLAGS"
    LIBS="$LIBS $BERKELEYDB_LIBS"

    AC_MSG_CHECKING(for Berkeley DB API)

    AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#include <db.h>

#if DB_VERSION_MAJOR < 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR < 1)
#Ancient Berkeley DB version
#endif
       ]])],[AC_MSG_RESULT(OK)],[AC_DEFINE(CF_OLD_DB,1,Define if old Berkeley API)
    ],[])

    AC_CHECK_LIB(db, db_create)

    LDFLAGS=$BERKELEYDB_SAVE_LDFLAGS
    CPPFLAGS=$BERKELEYDB_SAVE_CPPFLAGS
    LIBS=$BERKELEYDB_SAVE_LIBS

fi
AC_SUBST(BERKELEYDB_LDFLAGS)
AC_SUBST(BERKELEYDB_CFLAGS)
AC_SUBST(BERKELEYDB_LIBS)

fi

dnl OpenSSL

AC_ARG_WITH(openssl,
    [AS_HELP_STRING([--with-openssl[[=PATH]]],
    [Specify OpenSSL path])], [], [with_openssl=yes])

if test x"$with_openssl" = xno ; then
    AC_MSG_ERROR([This release of CFEngine requires OpenSSL >= 0.9.7])
fi

CF3_WITH_LIBRARY(openssl, [
   AC_CHECK_LIB(crypto, RSA_generate_key, [], [AC_MSG_ERROR(Cannot find OpenSSL)])
   AC_CHECK_HEADERS([openssl/opensslv.h], [], [AC_MSG_ERROR(Cannot find OpenSSL)])

   AC_MSG_CHECKING(for OpenSSL version)

   AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
   #include <openssl/opensslv.h>

   #if OPENSSL_VERSION_NUMBER < 0x0090602fL
   #This OpenSSL is too old
   #endif
   ]])],[AC_MSG_RESULT(OK)],[AC_MSG_ERROR(This release of CFEngine requires OpenSSL >= 0.9.7)])
])

dnl PCRE

AC_ARG_WITH([pcre], [AS_HELP_STRING([--with-pcre[[=PATH]]], [Specify PCRE path])], [], [with_pcre=yes])

if test "x$with_pcre" = "xno"; then
   AC_MSG_ERROR([PCRE is required])
fi

CF3_WITH_LIBRARY(pcre, [
  AC_CHECK_LIB(pcre, pcre_exec, [], [AC_MSG_ERROR(Cannot find PCRE)])
  AC_CHECK_HEADERS([pcre.h pcre/pcre.h], [break], [AC_MSG_ERROR(Cannot find PCRE)])
])

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
AC_CHECK_HEADERS(sys/acl.h winsock2.h)
AC_CHECK_HEADERS(zone.h)
AC_CHECK_HEADERS(sys/uio.h)

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

dnl ######################################################################
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ######################################################################

AC_C_CONST

AC_SYS_LARGEFILE
AC_TYPE_OFF_T

#
# AC_SYS_LARGEFILE correctly figures out necessary macros for large files, but
# on AIX there is a gotcha:
#
# Code generated by flex #includes <stdio.h> at the beginning of the file, which
# picks up 32-bit wide off_t. Then it #includes <conf.h> which provides LFS
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

AC_CHECK_DECLS(clock_gettime, [], [], [[#include <time.h>]])
AC_REPLACE_FUNCS(clock_gettime)

AC_CHECK_DECLS(unsetenv)
AC_REPLACE_FUNCS(unsetenv)

AC_CHECK_DECLS(strnlen)
AC_REPLACE_FUNCS(strnlen)

AC_CHECK_DECLS(strndup)
AC_REPLACE_FUNCS(strndup)

AC_CHECK_DECLS(setlinebuf)
AC_REPLACE_FUNCS(setlinebuf)

AC_CHECK_DECLS(strlcat)
AC_REPLACE_FUNCS(strlcat)

AC_CHECK_DECLS(strlcpy)
AC_REPLACE_FUNCS(strlcpy)

AC_CHECK_DECLS(realpath)
AC_CHECK_FUNCS(realpath)

AC_CHECK_DECLS(round, [], [], [[#include <math.h>]])
AC_REPLACE_FUNCS(round)

AC_CHECK_DECLS(strdup)
AC_REPLACE_FUNCS(strdup)

AC_CHECK_DECLS(nanosleep)
AC_REPLACE_FUNCS(nanosleep)

AC_CHECK_FUNCS(getcwd getnetgrent waitpid seteuid setegid setreuid setregid)
AC_CHECK_FUNCS(uname gethostname chflags)
AC_CHECK_FUNCS(strstr strsep putenv drand48 srand48 getaddrinfo)
AC_CHECK_FUNCS(bcopy mkfifo statfs statvfs door)
AC_CHECK_FUNCS(sysinfo setsid strrchr strerror snprintf sysconf)
AC_CHECK_FUNCS(getzoneid getzonenamebyid)
AC_CHECK_FUNCS(fpathconf)

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

AC_CHECK_FUNC(door_create,door)
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))
AC_CHECK_FUNC(getaddrinfo, , AC_CHECK_LIB(socket, getaddrinfo))

AC_SEARCH_LIBS(socket, socket, ,
    [AC_CHECK_LIB(nsl, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)])

AC_CHECK_FUNC(lchown, AC_DEFINE(HAVE_LCHOWN, [], [Whether to use lchown(3) to change ownerships]))

AC_CHECK_LIB(pthread,main)

AC_CHECK_FUNC(pthread_attr_setstacksize, AC_DEFINE(HAVE_PTHREAD_ATTR_SETSTACKSIZE, [], [Whether the thread library has setstacksize]), AC_CHECK_LIB(pthread, pthread_create))
AC_CHECK_FUNC(pthread_sigmask, AC_DEFINE(HAVE_PTHREAD_SIGMASK, [], [Whether the thread library has setmask]), AC_CHECK_LIB(pthread, pthread_create))

if test "$ac_cv_lib_pthread_main" = "yes"; then
	AC_CHECK_HEADERS(pthread.h)
	AC_CHECK_HEADERS(sched.h)
fi

#
# Temporarily force C macros to enable threading in CFEngine if pthreads were
# found by ACX_PTHREAD, and errorneously disabled by another pthread-related
# checks.
#
if test "x$acx_pthread_ok" = "xyes"; then
   AC_DEFINE(HAVE_PTHREAD_H)
   AC_DEFINE(HAVE_LIBPTHREAD)
fi

dnl ######################################################################
dnl Check for sa_len in struct sockaddr
dnl ######################################################################

AC_CHECK_MEMBERS([struct sockaddr.sa_len], , , [
#include <sys/types.h>
#include <sys/socket.h>])
   
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
 AC_DEFINE(HAVE_ORTENTRY, 1)
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

CPPFLAGS="$CPPFLAGS $POSTGRESQL_CPPFLAGS $MYSQL_CPPFLAGS $TOKYOCABINET_CPPFLAGS $PCRE_CPPFLAGS $BERKELEYDB_CPPFLAGS $OPENSSL_CPPFLAGS $MONGO_CPPFLAGS $LDAP_CPPFLAGS $LIBVIRT_CPPFLAGS"
CFLAGS="$CFLAGS $POSTGRESQL_CFLAGS $MYSQL_CFLAGS $TOKYOCABINET_CFLAGS $PCRE_CFLAGS $BERKELEYDB_CFLAGS $OPENSSL_CFLAGS $MONGO_CFLAGS $LDAP_CFLAGS $LIBVIRT_CFLAGS"
LDFLAGS="$LDFLAGS $POSTGRESQL_LDFLAGS $MYSQL_LDFLAGS $TOKYOCABINET_LDFLAGS $PCRE_LDFLAGS $BERKELEYDB_LDFLAGS $OPENSSL_LDFLAGS $MONGO_LDFLAGS $LDAP_LDFLAGS $LIBVIRT_LDFLAGS"
LIBS="$LIBS $POSTGRESQL_LIBS $MYSQL_LIBS $TOKYOCABINET_LIBS $PCRE_LIBS $BERKELEYDB_LIBS $OPENSSL_LIBS $MONGO_LIBS $LDAP_LIBS $LIBVIRT_LIBS"

dnl ######################################################################
dnl OS specific stuff
dnl ######################################################################

dnl To ensure conf.h is picked up via VPATH
if test "$srcdir" != "."; then
  CPPFLAGS="$CPPFLAGS -I`pwd`/src"
fi

case "$target_os" in

   sunos3*)
	AC_DEFINE(SUN3, [], [SunOS 3.x build])
        ;;
   sunos4*)
	AC_DEFINE(SUN4, [], [SunOS 4.x build])
        ;;
   solaris2.4*)
	AC_DEFINE(SOLARIS)
        AC_DEFINE(HAVE_SYS_ACL_H,)
        CPPFLAGS="-w $CPPFLAGS"
        LIBS="$LIBS -lelf"
        ;;
   solaris2.5*)
	AC_DEFINE(SOLARIS)
        AC_DEFINE(_POSIX_C_SOURCE, [], [SVR4 header stuff])
        AC_DEFINE(__EXTENSIONS__, [], [SVR4 header stuff])
        LIBS="$LIBS -lelf -lsec"
        ;;
   solaris2*)
	AC_DEFINE(SOLARIS, [], [Solaris build])
        AC_DEFINE(__BIT_TYPES_DEFINED__, [], [Solaris 2.6-related stuff]) # avoid conflict with db.h

       AC_PREPROC_IFELSE([
                         AC_LANG_PROGRAM([[
#define _POSIX_C_SOURCE 1
#include <unistd.h>
                           ]], [])],
                           [AC_DEFINE(_POSIX_C_SOURCE)],
                           [AC_DEFINE(_POSIX_C_SOURCE, 200112loL)])
        AC_DEFINE(__EXTENSIONS__)
        LIBS="$LIBS -lelf -lsec"
        ;;
   solaris*)
        AC_DEFINE(_POSIX_PTHREAD_SEMANTICS, 1, [Use POSIX pthread semantics on Solaris])
        ;;
   ultrix*)
	AC_DEFINE(ULTRIX, [], [Ultrix build])
        ;;
   hpux*|hp-ux*) 

	AC_DEFINE(HPuUX, [], [HP/UX build])
        if test "$GCC" != "yes"; then
	  AC_DEFINE(REGEX_MALLOC, [], [Whether to use the local regex functions])
        fi

        LIBS="$LIBS -lc"
	AC_CHECK_LIB(PW, main)
        ;;
   aix*)
	AC_DEFINE(AIX, [], [AIX build])
        CPPFLAGS="$CPPFLAGS -w"
	AC_CHECK_LIB(pthreads, main)
        AC_CHECK_HEADER(pthreads.h)
        ;;
   osf*)
	AC_DEFINE(OSF, [], [OSF/1 build])
	if test "$GCC" = yes; then
	   AC_MSG_WARN([pthreads may not work with GCC under Tru64])
	   AC_MSG_WARN([If you get build errors mentioning PTHREADDEFAULTS etc.,])
	   AC_MSG_WARN([re-configure with CC=cc.])
	fi
        ;;
   irix6*)
	# rtentry is detected OK on a 6.5.19 system.
        AC_DEFINE(HAVE_ORTENTRY, 1, [The old route entry structure in newer BSDs]) # Have to hack this for 6.* owing to bug
	AC_DEFINE(IRIX, [], [IRIX build])
        CFLAGS="$CFLAGS -w"
        ;;
   irix4*)
	AC_DEFINE(IRIX)
        CFLAGS="$CFLAGS -w"
        LIBS="$LIBS -lsun"
        ;;
   irix*)
	AC_DEFINE(IRIX)
        CFLAGS="$CFLAGS -w"
        ;;
   linux*)
	AC_DEFINE(LINUX, [], [Linux build])
	AC_CHECK_LIB(nss_nis, yp_get_default_domain)
        AC_CHECK_LIB(acl,main)
        AC_CHECK_HEADERS(acl.h sys/acl.h acl/libacl.h)
        ;;
   freebsd*|dragonfly*)
	AC_DEFINE(FREEBSD, [], [FreeBSD build])
        CFLAGS="$CFLAGS -pthread -D_THREAD_SAFE -DBUILDTIN_GCC_THREAD"
        AC_CHECK_HEADERS(pthread.h)
        ;;
   netbsd*)
	AC_DEFINE(NETBSD, [], [NetBSD build])
        ;;
   newsos*)
	AC_DEFINE(NEWS_OS, [], [NewsOS build])
        ;;
   bsd/os*)
	AC_DEFINE(BSDOS, [], [BSD/OS build])
        ;;
   bsd*)
	AC_DEFINE(BSD43, [], [4.3BSD build])
        ;;
   aos*)
	AC_DEFINE(AOS, [], [AOS build])
        ;;
   nextstep*)
	AC_DEFINE(NEXTSTEP, [], [NeXTSTEP build])
        ;;

   unicos*)
	AC_DEFINE(CFCRAY, [], [Cray build])
        ;;

   cray*)
	AC_DEFINE(CFCRAY)
        ;;

   qnx*)
        AC_DEFINE(CFQNX, [], [QNX build])
        ;;

   openbsd*|obsd*)
	AC_DEFINE(OPENBSD, [], [OpenBSD build])
        ;;

   gnu*)
	AC_DEFINE(CFGNU, [], [GNU build])
        ;;

   sysv4.2MP|unix_sv*)
	AC_DEFINE(UNIXWARE, [], [Unixware build])
        AC_CHECK_LIB(thread,main)         dnl POSIX threads, weird setup
        if test "$ac_cv_lib_thread_main" = "yes"; then
	   AC_CHECK_HEADERS(thread.h)
        fi
        ;;

   cygwin*)
        AC_DEFINE(CFCYG, 1, [Cygwin NT build])
        AC_DEFINE(NT, [], [NT build])
        ;;

   mingw*)
        AC_DEFINE(MINGW, 1, [Native NT build])
        AC_DEFINE(NT)
        LIBS="$LIBS -liphlpapi -lws2_32 -lpsapi -lole32 -loleaut32 -luuid"
        ;;

   sco*)
        AC_DEFINE(SCO, [], [SCO build])
        ;;

   darwin*)
        AC_DEFINE(DARWIN, [], [Darwin build])
        LDFLAGS="-Xlinker -m $LDFLAGS"
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
		else
  		        WORKDIR=/var/cfengine
		fi

     	AC_DEFINE_UNQUOTED(WORKDIR, "${WORKDIR}", [lock and log directories])
        AC_SUBST(workdir, "${WORKDIR}")
	],
	[

        case "$target_os" in

           mingw*)
                WORKDIR=$(cmd /c "echo %PROGRAMFILES%\\Cfengine" | sed 's/\\/\\\\/g')
                ;;

           *)
                WORKDIR=/var/cfengine
                ;;
        esac

		AC_DEFINE_UNQUOTED(WORKDIR, "${WORKDIR}")
	]
)

dnl #####################################################################
dnl Fix for lex/flex
dnl #####################################################################

AC_MSG_CHECKING(for 8-bit support in Flex)

if test "$LEX" = "flex"; then
   EIGHTBIT="\200-\377"
   AC_SUBST(EIGHTBIT)
   AC_MSG_RESULT(8-bit support added)
   NOWRAP="%option noyywrap"
else
   AC_MSG_RESULT(no 8-bit support)
   NOWRAP=""
fi

AC_SUBST(NOWRAP)

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
	
dnl # -Wformat - to be considered

	GCC_CFLAG="-g -Wreturn-type -Wmissing-prototypes"
	case "$CFLAGS" in "-O"*|*" -O"*)
		GCC_CFLAG="$GCC_CFLAG -Wuninitialized"
		;;
	esac

	AC_MSG_RESULT(yes)
	AC_SUBST(GCC_CFLAG)
else 
	GCC_CFLAG=""
	AC_MSG_RESULT(no)
	AC_SUBST(GCC_CFLAG)
fi


dnl #####################################################################
dnl Hostname and Version stuff
dnl #####################################################################

AC_PATH_PROG(HOSTNAME, hostname, "", $PATH)

AC_DEFINE_UNQUOTED(AUTOCONF_HOSTNAME, "`$HOSTNAME`", [Special Cfengine symbol])
AC_DEFINE_UNQUOTED(AUTOCONF_SYSNAME, "$target_os", [Speial Cfengine symbol])


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
dnl
dnl

dnl ######################################################################
dnl Summarize
dnl ######################################################################

AC_MSG_RESULT( )
AC_MSG_RESULT(    Summary of options...)

AC_MSG_RESULT([-> PCRE: $PCRE_PATH])

if test "x$ac_cv_lib_mysqlclient_mysql_real_connect" != "xno"; then
  AC_MSG_RESULT([-> MySQL connector: $MYSQL_PATH])
else
  AC_MSG_RESULT([-> MySQL connector: disabled])
fi

if test "x$ac_cv_lib_pq_PQconnectdb" = "xyes"; then
  AC_MSG_RESULT([-> PostgreSQL connector: $POSTGRESQL_PATH])
else
  AC_MSG_RESULT([-> PostgreSQL connector: disabled])
fi

if test $WITH_TOKYO = 1; then
  AC_MSG_RESULT([-> DB: Tokyo Cabinet: $TOKYOCABINET_PATH])
elif test $WITH_QDBM = 1; then
  AC_MSG_RESULT([-> DB: QDBM])
elif test $WITH_SQLITE3 = 1; then
  AC_MSG_RESULT([-> DB: SQLite 3 (EXPERIMENTAL, BROKEN)])
else
  AC_MSG_RESULT([-> DB: Berkeley DB: $BERKELEYDB_DIR])
fi

m4_indir(incstart[]incend, [nova/config.m4])
m4_indir(incstart[]incend, [constellation/config.m4])
m4_indir(incstart[]incend, [galaxy/config.m4])

AC_MSG_RESULT( )



dnl ######################################################################
dnl Now make the Makefiles
dnl ######################################################################

AC_CONFIG_FILES([Makefile pub/Makefile src/Makefile src/cf3lex.l docs/Makefile examples/Makefile examples/example_config/Makefile masterfiles/Makefile])

m4_indir(incstart[]incend, [nova/output.m4])
m4_indir(incstart[]incend, [constellation/output.m4])
m4_indir(incstart[]incend, [galaxy/output.m4])

AC_OUTPUT

AC_MSG_RESULT(DONE: Configuration done. Run make/gmake to build cfengine.)

