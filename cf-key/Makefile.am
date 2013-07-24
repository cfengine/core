noinst_LIBRARIES = libcf-key.a

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libpromises \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

libcf_key_a_SOURCES = \
	cf-key.c \
        cf-key-functions.c cf-key-functions.h


LDADD = libcf-key.a ../libpromises/libpromises.la

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-key
 cf_key_SOURCES =
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
