sbin_PROGRAMS = cf-runagent

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	$(NOVA_CPPFLAGS)

if HAVE_NOVA
AM_CPPFLAGS += -I$(abs_top_srcdir)/nova/libcfrunagent
endif

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

LDADD = ../libpromises/libpromises.la
if HAVE_NOVA
LDADD += ../nova/libcfrunagent/libcfrunagent.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
