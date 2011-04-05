
AM_CFLAGS = @CFLAGS@ @GCC_CFLAG@ -fPIC -DPIC
INCLUDES = @BERKELEY_DB_CFLAGS@ @OPENSSL_LIB_CPPFLAGS@

AM_CPPFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS)

noinst_LTLIBRARIES = libcfpub.la

libcfpub_la_SOURCES =							\
	getopt.c							\
	getopt1.c							\
	regex.c								\
	snprintf.c

libcfpub_la_LIBADD = $(LTLIBOBJS)

EXTRA_DIST =								\
	getopt.h							\
	global.h							\
	gnuregex.h							\
	snprintf.h							
#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES =

#
# Get everything removed down to where rebuilding requires:
# "configure; make; make install"
#
DISTCLEANFILES = 

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in
