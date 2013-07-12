sbin_PROGRAMS = cf-execd

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libpromises \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(ENTERPRISE_CPPFLAGS)

if HAVE_NOVA
AM_CPPFLAGS += -I$(abs_top_srcdir)/enterprise/libcfexecd
endif

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

LDADD = ../libpromises/libpromises.la
if HAVE_NOVA
LDADD += ../enterprise/libcfexecd/libcfexecd.la
endif

cf_execd_SOURCES = \
        cf-execd.c cf-execd.h \
        cf-execd-runner.c cf-execd-runner.h \
        exec-config.c exec-config.h

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
