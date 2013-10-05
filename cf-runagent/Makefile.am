noinst_LTLIBRARIES = libcf-runagent.la

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

libcf_runagent_la_LIBADD = ../libpromises/libpromises.la

libcf_runagent_la_SOURCES = cf-runagent.c

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-runagent
 cf_runagent_LDADD = libcf-runagent.la
 cf_runagent_SOURCES =
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
