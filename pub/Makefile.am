noinst_LTLIBRARIES = libcfpub.la

AM_CPPFLAGS = @CPPFLAGS@ \
	$(NOVA_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

libcfpub_la_LIBADD = $(LTLIBOBJS)

libcfpub_la_SOURCES = \
	getopt.c \
	getopt1.c

EXTRA_DIST = \
	getopt.h

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~
