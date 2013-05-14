sbin_PROGRAMS = cf-promises

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	$(NOVA_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ -I$(srcdir) \
	$(NOVA_CFLAGS)

LDADD = ../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
