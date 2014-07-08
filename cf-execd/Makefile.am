noinst_LTLIBRARIES = libcf-execd.la

AM_CPPFLAGS = \
	$(OPENSSL_CPPFLAGS) \
	-I$(srcdir)/../libpromises \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libenv \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_execd_la_LIBADD = ../libutils/libutils.la ../libpromises/libpromises.la

libcf_execd_la_SOURCES = \
        cf-execd.c cf-execd.h \
        cf-execd-runner.c cf-execd-runner.h \
        exec-config.c exec-config.h \
        execd-config.c execd-config.h

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-execd
 # Workaround for automake madness (try removing it if you want to know why).
 cf_execd_CFLAGS = $(AM_CFLAGS)
 cf_execd_LDADD = libcf-execd.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
