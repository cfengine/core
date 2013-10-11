sbin_PROGRAMS = cf-execd

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libpromises \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(NOVA_CPPFLAGS)

if HAVE_NOVA
AM_CPPFLAGS += -I$(abs_top_srcdir)/nova/libcfexecd
endif

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

LDADD = ../libpromises/libpromises.la
if HAVE_NOVA
if NT
LDADD += ../nova/libcfexecd/libcfexecd.la
endif
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
