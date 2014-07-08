noinst_LTLIBRARIES = libcf-serverd.la

AM_CPPFLAGS = -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libenv \
	$(OPENSSL_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_serverd_la_LIBADD = ../libpromises/libpromises.la

libcf_serverd_la_SOURCES = \
	cf-serverd.c \
	cf-serverd-enterprise-stubs.c cf-serverd-enterprise-stubs.h \
	cf-serverd-functions.c cf-serverd-functions.h \
	server_common.c server_common.h \
	server.c server.h \
	server_transform.c server_transform.h \
	server_classic.c server_classic.h \
	server_tls.c server_tls.h \
	server_access.c server_access.h \
	strlist.c strlist.h

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-serverd
 # Workaround for automake madness (try removing it if you want to know why).
 cf_serverd_CFLAGS = $(AM_CFLAGS)
 cf_serverd_LDADD = libcf-serverd.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
