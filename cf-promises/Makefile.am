noinst_LIBRARIES = libcf-promises.a

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ -I$(srcdir) \
	$(ENTERPRISE_CFLAGS)

LDADD = libcf-promises.a ../libpromises/libpromises.la

libcf_promises_a_SOURCES = cf-promises.c

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-promises
 cf_promises_SOURCES =
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
