noinst_LTLIBRARIES = libcf-promises.la

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils -I$(srcdir)/../libcfnet \
	$(OPENSSL_CPPFLAGS) \
	$(PCRE_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ -I$(srcdir) \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_promises_la_LIBADD = ../libpromises/libpromises.la

libcf_promises_la_SOURCES = cf-promises.c

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-promises
 cf_promises_LDADD = libcf-promises.la
 cf_promises_SOURCES =
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
