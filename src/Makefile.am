AM_CFLAGS = $(GCC_CFLAG) @CFLAGS@  
AM_YFLAGS = -d
LEXLIB = @LEXLIB@
LDADD = $(BERKELEY_DB_LDFLAGSD) $(BERKELEY_DB_LIB) $(OPENSSL_LIB_LDFLAGS) $(OPENSSL_LIB_LIB) libpromises.la 
INCLUDES = $(BERKELEY_DB_CFLAGS) $(OPENSSL_LIB_CPPFLAGS)
AM_LDFLAGS = $(BERKELEY_DB_LDFLAGS) $(OPENSSL_LIB_LDFLAGS) @LDFLAGS@

lib_LTLIBRARIES = libpromises.la

libpromises_la_LDFLAGS = -version-info 1:0:0 -no-undefined

# Separate out a library

libpromises_la_SOURCES =						\
	fncall.c							\
	evalfunction.c							\
	sysinfo.c							\
	conversion.c							\
	scope.c								\
	assoc.c								\
	vars.c								\
	args.c								\
	hashes.c							\
	expand.c							\
	selfdiagnostic.c						\
	promises.c							\
	constraints.c							\
	iteration.c							\
	rlist.c


sbin_PROGRAMS = cfpromises

cfpromises_SOURCES =							\
	cfpromises.c							\
	cf3globals.c							\
	cf3parse.y							\
	cf3lex.l							\
	report.c							\
	install.c							\
	syntax.c							\
	mod_common.c							\
	mod_access.c							\
	mod_exec.c							\
	mod_files.c							\
	mod_storage.c							\
	mod_knowledge.c							\
	mod_process.c


EXTRA_DIST = cf3lex.c cf3parse.c

#
# Pickup header files here so Automake knows about them
#
noinst_HEADERS =							\
	cf.defs.h							\
	prototypes.h							\
	cf.extern.h							\
	promises.h							\
	prototypes3.h							\
	cf3.defs.h							\
	cf3parse.h							\
	cf3.extern.h

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES = cf3lex.c cf3parse.c cf3parse.h

#
# Get everything removed down to where rebuilding requires:
# "configure; make; make install"
#
DISTCLEANFILES = cflex.c cfparse.c cfparse.h

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in conf.h.in stamp-h.in
