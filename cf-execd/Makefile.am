sbin_PROGRAMS = cf-execd
noinst_LIBRARIES = libcf-execd.a

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libpromises \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

LDADD = ../libpromises/libpromises.la

cf_execd_SOURCES = \
        cf-execd.c cf-execd.h \
        cf-execd-runner.c cf-execd-runner.h \
        exec-config.c exec-config.h

libcf_execd_a_SOURCES = $(cf_execd_SOURCES)

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
