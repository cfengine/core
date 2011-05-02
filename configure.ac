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

define([svnversion], [r]esyscmd([sh -c "svnversion --no-newline"]))dnl

dnl
dnl The version in the next line is the only one to set
dnl
dnl 'svnversion' need to be removed before release and added back after.
dnl

AM_INIT_AUTOMAKE(cfengine, 3.1.5)   dnl remember to set version
AM_MAINTAINER_MODE([enable])

AC_DEFINE(NOVA_VERSION, "2.0.3a1", "Nova version")
AC_DEFINE(CONSTELLATION_VERSION, "1.0.0a1", "Constellation version")

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_HEADERS([src/conf.h])

dnl Libtool madness

AC_CONFIG_MACRO_DIR([m4])

#
# Add to the default list of places in LDFLAGS to compensate for
# ... the configure default value of LIBS on some systems
# Don't add if cross-compiling (setting host), to avoid using native libs.
#
if test "x$host" = "x"; then
  for x in /usr/lib64 /usr/local/gnu/lib /usr/local/gnulib /opt/dce/lib /sw/lib /usr/pkg/lib /usr/X11R7/lib
  do
    if test -d "$x"; then
      y=`expr " $LDFLAGS " : ".* -L$x "`
      if test $y -eq 0; then
        LDFLAGS="$LDFLAGS -L$x"
      fi
    fi
  done
fi

#
# Add to the default list of places in CPPFLAGS to match LDFLAGS above
# Don't add if cross-compiling (setting host), to avoid using native libs.
# 
if test "x$host" = "x"; then
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

ACX_PTHREAD
CC=$PTHREAD_CC
CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
CPPFLAGS="$PTHREAD_CFLAGS $CPPFLAGS"
LDFLAGS="$PTHREAD_LDFLAGS $LDFLAGS"


dnl ######################################################################
dnl Checks for programs.
dnl ######################################################################

AC_PROG_CC
AC_EXEEXT

dnl Those two lines will emit deprecation warnings on sufficiently new
dnl systems. Unfortunately those cannot be replaced by LT_INIT due to older
dnl systems not having libtool >= 2
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL
AC_CONFIG_LIBOBJ_DIR(pub)
AC_FUNC_GETLOADAVG 
AC_PATH_PROG(GETCONF, getconf, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin:/sw/bin)

dnl ######################################################################
dnl Checks for libraries.
dnl ######################################################################

AC_CHECK_LIB(m,main)

AC_ARG_WITH(diagnostics,
	AS_HELP_STRING(--with-diagnostics, [enable test suite]),[],
	with_diagnostics=no)

if test "x$with_diagnostics" != xno; then
   AC_DEFINE(BUILD_TESTSUITE, 1, [Build Test suite])
fi

dnl Now check for database connectors


dnl ######################################################################
dnl  Look for SQL connectors.
dnl ######################################################################

AC_ARG_WITH([sql],
    [AS_HELP_STRING([--without-sql], [disable SQL database connectors])],[],[with_sql=yes])
AS_IF([test "x$with_sql" = "xyes"], [WITH_SQL=1], [WITH_SQL=0])

if test $WITH_SQL = 1; then
  case "$target_os" in
     linux*)
        if [ `which pg_config` ]; then
          CPPFLAGS=-I`pg_config --includedir`
        fi
  esac
  AC_CHECK_LIB(mysqlclient,main)
  AC_CHECK_LIB(pq,main)
  AC_CHECK_HEADERS(mysql/mysql.h)
  AC_CHECK_HEADERS(pgsql/libpq-fe.h)
  AC_CHECK_HEADERS(libpq-fe.h)
fi

dnl
dnl Enable large file support
dnl
AC_SYS_LARGEFILE
AC_TYPE_OFF_T

dnl
dnl Enterprise extensions
dnl

sinclude([nova/options.m4])
AM_CONDITIONAL([HAVE_NOVA], [test "x$with_nova" != xno && test -d ${srcdir}/nova])
sinclude([constellation/options.m4])
AM_CONDITIONAL([HAVE_CONSTELLATION], [test "x$with_constellation" != xno && test -d ${srcdir}/constellation])
sinclude([galaxy/options.m4])
AM_CONDITIONAL([HAVE_GALAXY], [test "x$with_galaxy" != xno && test -d ${srcdir}/galaxy])

dnl
dnl Various functions
dnl

AC_CHECK_FUNC(door_create,door)
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))
AC_CHECK_FUNC(getaddrinfo, , AC_CHECK_LIB(socket, getaddrinfo))

AC_SEARCH_LIBS(socket, socket, ,
    [AC_CHECK_LIB(nsl, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)])

dnl 
dnl Choose a database
dnl

AC_ARG_WITH([tokyocabinet],
    [AS_HELP_STRING([--with-tokyocabinet], [use Tokyo Cabinet to store runtime data])])
AS_IF([test "x$with_tokyocabinet" = "xyes"], [WITH_TOKYO=1], [WITH_TOKYO=0])

if test $WITH_TOKYO = 1; then
  AC_MSG_CHECKING(for TokyoCabinet)
  AC_CHECK_LIB(tokyocabinet,main, [], [AC_MSG_ERROR(Cannot find Tokyo Cabinet)])
  AC_CHECK_HEADERS(tcutil.h)
  AC_CHECK_HEADERS(tchdb.h)
  AC_DEFINE(TCDB, 1, [Define if Tokyo Cabinet is available.])
fi

dnl else qdbm?

AC_ARG_WITH([qdbm],
    [AS_HELP_STRING([--with-qdbm], [use QDBM to store runtime data])])
AS_IF([test "x$with_qdbm" = "xyes"], [WITH_QDBM=1], [WITH_QDBM=0])

if test $WITH_QDBM = 1; then
  AC_MSG_CHECKING(for QDBM)
  AC_CHECK_LIB(qdbm,main, [], [AC_MSG_ERROR(Cannot find Quick Database Manager)])
  AC_CHECK_HEADERS(depot.h)
  AC_DEFINE(QDB, 1, [Define if QDBM is available.])
fi

dnl
dnl  Look for Berkeley DB.
dnl 

if test $WITH_QDBM = 0 -a $WITH_TOKYO = 0; then
  AC_ARG_WITH(berkeleydb,
    [  --with-berkeleydb[=PATH] directory where BerkeleyDB exists],
    BERKELEY_DB_DIR=$with_berkeleydb,
    BERKELEY_DB_DIR=default)

  AC_MSG_CHECKING(for BerkeleyDB location in $BERKELEY_DB_DIR)

  if test "x$BERKELEY_DB_DIR" = "xno" ; then
    AC_MSG_RESULT( )
    AC_MSG_ERROR(This release of cfengine requires a version of BerkeleyDB 4.4 or later)
  else
    if test "x$BERKELEY_DB_DIR" = "xdefault" ; then
        for v in BerkeleyDB.4.2 BerkeleyDB.4.3 BerkeleyDB.4.4 BerkeleyDB.4.5 BerkeleyDB.4.6 BerkeleyDB.4.7 BerkeleyDB.4.8; do
            for d in $prefix /opt /usr/local /usr/pkg /usr; do
                test -d "$d/$v" && BERKELEY_DB_DIR="$d/$v"
            done
        done
    fi

    if test "x$BERKELEY_DB_DIR" = "xdefault" ; then

        for d in $prefix /opt /usr/local /usr; do
            for v in db-4 db4 db3 db db40; do

                if test -f "$d/include/$v/db.h" ; then
                    echo "Found header in $d/include/$v "
                    test "x$d" != "x/usr" && BERKELEY_DB_LDFLAGS="-L$d/lib"
                    BERKELEY_DB_CFLAGS="-I$d/include/$v"
                    late_LIBS=$LIBS
		    # In RedHat 8, for instance, we have /usr/include/db4
		    # and libdb-4.0.a.  Debian has /usr/lib/libdb-4.1.a, for
		    # instance.  Look for the appropriate library.
                    if test $v = db4 -o $v = db40; then
			save_CFLAGS="$CFLAGS"
			save_LDFLAGS="$LDFLAGS"
			CFLAGS="$CFLAGS $BERKELEY_DB_CFLAGS"
			LDFLAGS="$LDFLAGS $BERKELEY_DB_LDFLAGS"
			AC_SEARCH_LIBS(db_create, 
				[db-4 db4 db-4.8 db-4.7 db-4.6 db-4.5 db-4.4 db-4.3 db-4.2 db-4.1 db-4.0], 
				[BERKELEY_DB_LIB=$ac_cv_search_db_create])
			CFLAGS="$save_CFLAGS"
			LDFLAGS="$save_LDFLAGS"
		    else
                    	BERKELEY_DB_LIB="-l$v"
                    fi
		    LIBS=$late_LIBS
                    AC_MSG_RESULT($d)
                    break
                fi
            done

	    test "x$BERKELEY_DB_LIB" != "x" && break
            if test -f "$d/include/db.h"; then
                if test "x$d" != "x/usr"; then
                    BERKELEY_DB_LDFLAGS="-L$d/lib64 -L$d/lib"
                    BERKELEY_DB_CFLAGS="-I$d/include"
                fi
                BERKELEY_DB_LIB="-ldb"
                AC_MSG_RESULT($d)
                break
            fi
	done

        if test "x$BERKELEY_DB_LIB" = "x" ; then
            AC_MSG_ERROR(Cannot find BerkeleyDB)
        fi

    elif test -f "$BERKELEY_DB_DIR/include/db.h";then
        case "$target_os" in
        solaris*)
            #If we are staticlly linking the BDB files, we do not want a
            #-R flag.  If .so's are present, assume we are dynamic linking
            if test -n "`ls $BERKELEY_DB_DIR/lib/*.so 2>/dev/null`"
            then
                BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib -R$BERKELEY_DB_DIR/lib"
            else
                BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib"
            fi;;
        *)         BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib";;
        esac
  
        BERKELEY_DB_CFLAGS="-I$BERKELEY_DB_DIR/include"
        BERKELEY_DB_LIB="-ldb"
        AC_MSG_RESULT($BERKELEY_DB_DIR)

    elif test -d "$BERKELEY_DB_DIR"; then

        BERKELEY_DB_CFLAGS="-I$BERKELEY_DB_DIR/include"
        for v in . db48 db47 db46 db45 db44 db43 db42 db41 db40 db4 db33 db32 db3 db; do
            if test -f "$BERKELEY_DB_DIR/include/$v/db.h"; then
                BERKELEY_DB_INCLUDEDIR="$BERKELEY_DB_DIR/include/$v"
                BERKELEY_DB_CFLAGS="-I$BERKELEY_DB_INCLUDEDIR"
		break
            fi
        done

        if test -f "$BERKELEY_DB_INCLUDEDIR/db.h"; then
            BERKELEY_DB_LIB="-ldb"
            for v in db-4.8 db48 db-4.7 db4.7 db47 db-4.6 db4.6 db46 db-4.5 db4.5 db45 db-4.4 db4.4 db44; do
                if test -f "$BERKELEY_DB_DIR/lib/lib$v.so"; then
                    BERKELEY_DB_LIB="-l$v"
                    break
                fi
                if test -f "$BERKELEY_DB_DIR/lib64/lib$v.so"; then
                    BERKELEY_DB_LIB="-l$v"
                    break
                fi
            done

            case "$target_os" in
            solaris*)
                #If we are staticlly linking the BDB files, we do not want a
                #-R flag.  If .so's are present, assume we are dynamic linking
                if test -n "`ls $BERKELEY_DB_DIR/lib/*.so 2>/dev/null`"
                then
                    BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib -R$BERKELEY_DB_DIR/lib"
                else
                    BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib"
                fi;;
            *)         BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib";;

            esac
  
            AC_MSG_RESULT($BERKELEY_DB_DIR)
        else
           AC_MSG_ERROR(Cannot find BerkeleyDB)
        fi

    else
        AC_MSG_ERROR(Cannot find BerkeleyDB)
    fi
    AC_DEFINE(USE_BERKELEY_DB, 1, [Define if BerkeleyDB is available.])

    BERKELEY_DB_SAVE_LDFLAGS=$LDFLAGS
    BERKELEY_DB_SAVE_CPPFLAGS=$CPPFLAGS
    BERKELEY_DB_SAVE_LIBS=$LIBS
    LDFLAGS="$LDFLAGS $BERKELEY_DB_LDFLAGS"
    CFLAGS="$CFLAGS $BERKELEY_DB_CFLAGS"
    LIBS="$LIBS $BERKELEY_DB_LIB"

    AC_MSG_CHECKING(for Berkeley DB API)

    AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <db.h>
#include <stdio.h>
int main(void)
{
    printf("%d.%d.%d ",DB_VERSION_MAJOR,DB_VERSION_MINOR,DB_VERSION_PATCH);
    if (DB_VERSION_MAJOR < 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR < 1))
        {
        exit(1);
        }
    else
        {
        exit(0);
        }
}
       ]])],[AC_MSG_RESULT(OK)],[AC_DEFINE(CF_OLD_DB,1,Define if old Berkeley API)
    ],[])

    AC_CHECK_LIB(db, db_create)

    LDFLAGS=$BERKELEY_DB_SAVE_LDFLAGS
    CPPFLAGS=$BERKELEY_DB_SAVE_CPPFLAGS
    LIBS=$BERKELEY_DB_SAVE_LIBS

fi
AC_SUBST(BERKELEY_DB_LDFLAGS)
AC_SUBST(BERKELEY_DB_CFLAGS)
AC_SUBST(BERKELEY_DB_LIB)

fi

dnl END check database


dnl
dnl Search for openssl
dnl


AC_ARG_WITH(openssl,
    [  --with-openssl[=PATH] directory where OpenSSL exists],
    OPENSSL_LIB_DIR=$with_openssl,
    OPENSSL_LIB_DIR=default)
AC_MSG_CHECKING(for OpenSSL location)
if test x"$OPENSSL_LIB_DIR" = xno ; then
    AC_MSG_RESULT(no)
    AC_MSG_ERROR(This release of cfengine requires OpenSSL 0.9.7 or later)
else
    if test x"$OPENSSL_LIB_DIR" = xdefault ; then
        for d in $prefix /opt/ssl /usr/local/ssl /usr/local /usr; do
            if test -f "$d/include/openssl/opensslv.h"; then
                OPENSSL_LIB_LDFLAGS="-L$d/lib${libsuff}"
                OPENSSL_LIB_CPPFLAGS="-I$d/include"
                OPENSSL_LIB_LIB="-lcrypto"
                AC_MSG_RESULT($d)
                break
            fi
        done

        if test x"$OPENSSL_LIB_LIB" = x ; then
            AC_MSG_ERROR(Cannot find OpenSSL)
        fi
    elif test -f "$OPENSSL_LIB_DIR/include/openssl/ssl.h";then
        case "$target_os" in
        solaris*)
            #If we are staticlly linking the SSL files, we do not want a
            #-R flag.  If .so's are present, assume we are dynamic linking
            if test -n "`ls $OPENSSL_LIB_DIR/lib/*.so 2>/dev/null`"
            then
                OPENSSL_LIB_LDFLAGS="-L$OPENSSL_LIB_DIR/lib -R$OPENSSL_LIB_DIR/lib"
            else
                OPENSSL_LIB_LDFLAGS="-L$OPENSSL_LIB_DIR/lib"
            fi;;
        *)         OPENSSL_LIB_LDFLAGS="-L$OPENSSL_LIB_DIR/lib";;
        esac
  
        OPENSSL_LIB_CPPFLAGS="-I$OPENSSL_LIB_DIR/include"
        OPENSSL_LIB_LIB="-lcrypto"
        AC_MSG_RESULT($OPENSSL_LIB_DIR)
    else
        AC_MSG_ERROR(Cannot find OpenSSL)
    fi
    AC_DEFINE(USE_OPENSSL_LIB, 1, [Define if OpenSSL is available.])

    OPENSSL_SAVE_LDFLAGS=$LDFLAGS
    OPENSSL_SAVE_CPPFLAGS=$CPPFLAGS
    OPENSSL_SAVE_LIBS=$LIBS

    LDFLAGS="$LDFLAGS $OPENSSL_LIB_LDFLAGS"
    CPPFLAGS="$CPPFLAGS $OPENSSL_LIB_CPPFLAGS"
    LIBS="$LIBS $OPENSSL_LIB_LIB"

    AC_MSG_CHECKING(for OpenSSL Version)

    AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <openssl/opensslv.h>
int main(void)
{
    if (OPENSSL_VERSION_NUMBER < 0x0090602fL)
        exit(1);
    exit(0);
}
       ]])],[AC_MSG_RESULT(OK)],[AC_MSG_ERROR(This release of cfengine requires openssl 0.9.7 or later)],[AC_MSG_WARN(Assuming 0.9.7 or later when cross-compiling)
    ])

    dnl ######################################################################
    dnl Checks for OpenSSL crypto lib
    dnl ######################################################################

    AC_CHECK_LIB(crypto,main)


    LDFLAGS=$OPENSSL_SAVE_LDFLAGS
    CPPFLAGS=$OPENSSL_SAVE_CPPFLAGS
    LIBS=$OPENSSL_SAVE_LIBS

fi
AC_SUBST(OPENSSL_LIB_LDFLAGS)
AC_SUBST(OPENSSL_LIB_CPPFLAGS)
AC_SUBST(OPENSSL_LIB_LIB)

#
# Others
#

dnl ######################################################################
dnl Checks for perl compatible regular expressions
dnl ######################################################################

AC_ARG_WITH([pcre],
			[AS_HELP_STRING([--with-pcre[[=PATH]]], [Build with support for PCRE (optionally search in PATH)])])
AS_IF([test "x$with_pcre" != "xno"], [
    AC_MSG_CHECKING([for PCRE])
    if test "x$with_pcre" = "xyes" -o "x$with_pcre" = "x" ; then
		for d in $prefix /usr/local /usr; do
			if test -f "$d/include/pcre.h" ; then
				CPPFLAGS="$CPPFLAGS -I$d/include"
				LDFLAGS="$LDFLAGS -L$d/lib"
				AC_MSG_RESULT($d)
				FOUND_PCRE=1
				break
			fi
		done
		if test "x$FOUND_PCRE" = "x" ; then
			AC_MSG_NOTICE([Cannot find PCRE - specify the path])
		fi
	else
		if test -f "${withval}/include/pcre.h" ; then
			CPPFLAGS="$CPPFLAGS -I${withval}/include"
			LDFLAGS="$LDFLAGS -L${withval}/lib"
			AC_MSG_RESULT("${withval}")
		else
			AC_MSG_NOTICE([Cannot find PCRE in $withval])
		fi
	fi

	AC_CHECK_HEADERS([pcre.h pcre/pcre.h])
	AC_CHECK_LIB(pcre, main, [], [AC_MSG_WARN([Cannot find PCRE])])
	AC_DEFINE(USE_PCRE, 1, [Define if PCRE is available.])
])

if test "x$ac_cv_lib_pcre_main" != "xyes"; then

	AC_MSG_ERROR([PCRE is required])
fi

LIBS="$LIBS $BERKELEY_DB_LDFLAGS $BERKELEY_DB_LIB $OPENSSL_LIB_LDFLAGS $OPENSSL_LIB_LIB"

dnl AC_CHECK_LIB(rx, main) deprecated
AC_CHECK_LIB(rt, main)

dnl ######################################################################
dnl Checks for header files.
dnl ######################################################################

AC_CHECK_HEADERS(unistd.h stdlib.h sys/loadavg.h)
AC_CHECK_HEADERS(sys/mount.h)
AC_CHECK_HEADERS(utime.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(malloc.h sys/malloc.h)
AC_CHECK_HEADERS(vfs.h)
AC_CHECK_HEADERS(sys/vfs.h)
AC_CHECK_HEADERS(sys/sockio.h)
AC_CHECK_HEADERS(sys/statvfs.h)
AC_CHECK_HEADERS(sys/statfs.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(sys/filesys.h)
AC_CHECK_HEADERS(dustat.h)
AC_CHECK_HEADERS(sys/systeminfo.h)
AC_CHECK_HEADERS(sys/acl.h winsock2.h)
AC_CHECK_HEADERS(zone.h)

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

dnl ######################################################################
dnl Check for special functions
dnl ######################################################################

AC_REPLACE_FUNCS([unsetenv])
AC_REPLACE_FUNCS(strndup)

AC_CHECK_FUNCS(getcwd getnetgrent waitpid seteuid setegid setreuid setregid)
AC_CHECK_FUNCS(uname gethostname realpath chflags)
AC_CHECK_FUNCS(strstr strsep putenv drand48 srand48 getaddrinfo)
AC_CHECK_FUNCS(bcopy mkfifo statfs statvfs door)
AC_CHECK_FUNCS(sysinfo setsid strdup strrchr strerror snprintf sysconf)
AC_CHECK_FUNCS(getzoneid,getzonenamebyid)

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

dnl #######################################################################
dnl Large File Support
dnl #######################################################################

lfsresult=no
AC_MSG_CHECKING(whether to compile with large file support)
if test "$GETCONF" != "false"; then
	lfscflags=`$GETCONF LFS_CFLAGS 2>/dev/null`
	if test -n "$lfscflags"; then
		CFLAGS="$CFLAGS $lfscflags"
		lfsresult=yes
	fi
	lfsldflags=`$GETCONF LFS_LDFLAGS 2>/dev/null`
	test -n "$lfsldflags" && LDFLAGS="$LDFLAGS $lfsldflags"
fi
AC_MSG_RESULT([$lfsresult])

dnl #######################################################################
dnl Handle DCE + threading stuff
dnl #######################################################################

AC_CHECK_LIB(pthread,main)         dnl POSIX threads

if test "$ac_cv_lib_pthread_main" = "yes"; then
	AC_CHECK_HEADERS(pthread.h)
	AC_CHECK_HEADERS(sched.h)
fi



AC_CHECK_FUNC(lchown, AC_DEFINE(HAVE_LCHOWN, [], [Whether to use lchown(3) to change ownerships]))

AC_CHECK_FUNC(pthread_attr_setstacksize, AC_DEFINE(HAVE_PTHREAD_ATTR_SETSTACKSIZE, [], [Whether the thread library has setstacksize]), AC_CHECK_LIB(pthread, pthread_create))
AC_CHECK_FUNC(pthread_sigmask, AC_DEFINE(HAVE_PTHREAD_SIGMASK, [], [Whether the thread library has setmask]), AC_CHECK_LIB(pthread, pthread_create))

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

AC_MSG_CHECKING(for GCC specific compile flags)
if test x"$GCC" = "xyes"; then
	
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

AM_CONDITIONAL(CROSS_COMPILING, test "x$cross_compiling" = "xyes")

dnl ######################################################################
dnl Summarize
dnl ######################################################################

AC_MSG_RESULT( )
AC_MSG_RESULT(    Summary of options...)


if test "x$ac_cv_lib_pcre_main" = "xyes"; then
  AC_MSG_RESULT([-> PCRE: enabled])
else
  AC_MSG_RESULT([-> PCRE: disabled, falling back to POSIX builtins])
fi

if test "x$ac_cv_lib_mysqlclient_main" = "xyes"; then
  AC_MSG_RESULT([-> MySQL connector: enabled])
else
  AC_MSG_RESULT([-> MySQL connector: disabled])
fi

if test "x$ac_cv_lib_pq_main" = "xyes"; then
  AC_MSG_RESULT([-> PostgreSQL connector: enabled])
else
  AC_MSG_RESULT([-> PostgreSQL connector: disabled])
fi

if test "x$ac_cv_lib_mysqlclient_main" = "xyes" || test "x$ac_cv_lib_pq_main" = "xyes"
then
  AC_MSG_RESULT([-> Knowledge map: enabled])
else
  AC_MSG_RESULT([-> Knowledge map: disabled, no SQL connector is configured])
fi

if test $WITH_TOKYO = 1; then
  AC_MSG_RESULT([-> DB: Tokyo Cabinet])
elif test $WITH_QDBM = 1; then
  AC_MSG_RESULT([-> DB: QDBM])
else
  AC_MSG_RESULT([-> DB: Berkeley DB (path: $BERKELEY_DB_DIR)])
fi

m4_sinclude([nova/config.m4])
m4_sinclude([constellation/config.m4])
m4_sinclude([galaxy/config.m4])

AC_MSG_RESULT( )



dnl ######################################################################
dnl Now make the Makefiles
dnl ######################################################################

AC_CONFIG_FILES([Makefile pub/Makefile src/Makefile src/cf3lex.l docs/Makefile examples/Makefile inputs/Makefile])

m4_sinclude([nova/output.m4])
m4_sinclude([constellation/output.m4])
m4_sinclude([galaxy/output.m4])

AC_OUTPUT

rm -f src/cf3parse.c src/cf3parse.h

AC_MSG_RESULT(DONE: Configuration done. Run make/gmake to build cfengine.)

