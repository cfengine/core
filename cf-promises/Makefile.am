sbin_PROGRAMS = cf-promises

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ -I$(srcdir) \
	$(ENTERPRISE_CFLAGS)

LDADD = ../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
