sbin_PROGRAMS = cf-key

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libpromises \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

cf_key_SOURCES = \
	cf-key.c \
        cf-key-functions.c cf-key-functions.h


LDADD = ../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
