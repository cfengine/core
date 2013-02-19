noinst_LTLIBRARIES = libutils.la

libutils_la_LIBADD = ../pub/libcfpub.la

libutils_la_SOURCES = \
	alloc.c alloc.h \
	atexit.c atexit.h \
	compiler.h \
	hashes.c hashes.h \
	sequence.c sequence.h \
	set.c set.h \
	statistics.c statistics.h \
	string_lib.c string_lib.h \
	string_map.c string_map.h \
	platform.h \
	proc_keyvalue.c proc_keyvalue.h \
	bool.h \
	json.c json.h \
	refcount.c refcount.h refcount_p.h \
	list.c list.h list_p.h \
	buffer.c buffer.h \
	map.c map.h map_common.h \
	array_map.c array_map_priv.h \
	hash_map.c hash_map_priv.h \
	misc_lib.c misc_lib.h \
	writer.c writer.h \
	csv_writer.c csv_writer.h \
	ip_address.c ip_address.h \
	xml_writer.c writer.h

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
MAINTAINERCLEANFILES = conf.h.in

# libcfpub dependency

.PRECIOUS: ../pub/libcfpub.la

../pub/libcfpub.la:
	$(MAKE) -C ../pub $(AM_MAKEFLAGS) libcfpub.la
