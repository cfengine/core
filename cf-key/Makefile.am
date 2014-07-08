noinst_LTLIBRARIES = libcf-key.la

AM_CPPFLAGS = \
	$(OPENSSL_CPPFLAGS) \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libpromises \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_key_la_SOURCES = \
	cf-key.c \
        cf-key-functions.c cf-key-functions.h


libcf_key_la_LIBADD = ../libpromises/libpromises.la

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-key
 cf_key_LDADD = libcf-key.la
 cf_key_SOURCES =
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
