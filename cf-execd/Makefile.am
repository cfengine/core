noinst_LIBRARIES = libcf-execd.a

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libpromises \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

LDADD = libcf-execd.a \
	../libpromises/libpromises.la

libcf_execd_a_SOURCES = \
        cf-execd.c cf-execd.h \
        cf-execd-runner.c cf-execd-runner.h \
        exec-config.c exec-config.h

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-execd
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
