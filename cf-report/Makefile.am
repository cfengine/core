sbin_PROGRAMS = cf-report

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises \
	$(NOVA_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

LDADD = ../libpromises/libpromises.la
if HAVE_NOVA
LDADD += ../nova/libcfreport/libcfreport.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
