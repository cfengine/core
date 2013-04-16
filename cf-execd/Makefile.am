sbin_PROGRAMS = cf-execd

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	$(NOVA_CPPFLAGS)

if HAVE_NOVA
AM_CPPFLAGS += -I$(abs_top_srcdir)/nova/libcfexecd
endif

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

LDADD = ../libpromises/libpromises.la
if HAVE_NOVA
LDADD += ../nova/libcfexecd/libcfexecd.la
endif

cf_execd_SOURCES = \
        cf-execd.c \
        cf-execd-runner.c cf-execd-runner.h \
        exec-config.c exec-config.h

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
