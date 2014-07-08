noinst_LTLIBRARIES = libcompat.la

# platform.h is in libutils/
# and unfortunately platform.h #inludes stuff from both openssl and pcre
AM_CPPFLAGS = -I$(top_srcdir)/libutils \
	$(OPENSSL_CPPFLAGS) $(PCRE_CPPFLAGS)

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
