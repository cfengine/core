AM_CFLAGS = $(NOVA_CFLAGS) @CFLAGS@ -I$(srcdir)/../libpromises

LDADD = ../libpromises/libpromises.la

sbin_PROGRAMS = \
        cf-runagent

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
