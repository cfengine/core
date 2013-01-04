AM_CFLAGS = $(GCC_CFLAG) $(NOVA_CFLAGS) @CFLAGS@  -I$(srcdir)/../libpromises

sbin_PROGRAMS = cf-execd

cf_execd_SOURCES = \
        cf-execd.c \
        cf-execd-runner.c cf-execd-runner.h

LDADD = ../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
