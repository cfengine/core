AM_CFLAGS = $(NOVA_CFLAGS) @CFLAGS@

# libcfpub

.PRECIOUS: ../pub/libcfpub.la

../pub/libcfpub.la:
	$(MAKE) -C ../pub $(AM_MAKEFLAGS) libcfpub.la

# libutils

noinst_LTLIBRARIES = libutils.la

libutils_la_LIBADD = ../pub/libcfpub.la

libutils_la_SOURCES = \
	alloc.h alloc.c \
	compiler.h \
	sequence.c sequence.h \
	set.c set.h \
	platform.h \
	bool.h \
	map.c map.h map_common.h \
	array_map.c array_map_priv.h \
	hash_map.c hash_map_priv.h \
	misc_lib.c misc_lib.h \
	writer.c writer.h

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in conf.h.in stamp-h.in
