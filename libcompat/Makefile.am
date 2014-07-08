noinst_LTLIBRARIES = libcompat.la

AM_CPPFLAGS = -I$(top_srcdir)/libutils				# platform.h

libcompat_la_LIBADD = $(LTLIBOBJS)

libcompat_la_SOURCES = \
	generic_at.c generic_at.h \
	getopt.c \
	getopt1.c

EXTRA_DIST = \
	getopt.h

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~
