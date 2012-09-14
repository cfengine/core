
AM_CFLAGS = @CFLAGS@ @GCC_CFLAG@

AM_CPPFLAGS = $(NOVA_CFLAGS)

noinst_LTLIBRARIES = libcfpub.la

libcfpub_la_SOURCES = \
	getopt.c \
	getopt1.c

libcfpub_la_LIBADD = $(LTLIBOBJS)

EXTRA_DIST = \
	getopt.h

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES = *.gcno *.gcda

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
