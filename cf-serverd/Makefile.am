noinst_LTLIBRARIES = libcf-serverd.la

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

LDADD = libcf-serverd.a \
	../libpromises/libpromises.la

libcf_serverd_la_SOURCES = \
        cf-serverd.c \
        cf-serverd-functions.c \
        cf-serverd-functions.h \
        server.c server.h \
        server_transform.c server_transform.h

if !HAVE_ENTERPRISE
libcf_serverd_la_SOURCES += \
	cf-serverd-enterprise-stubs.c cf-serverd-enterprise-stubs.h
endif

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
