noinst_LIBRARIES = libcf-serverd.a

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

LDADD = libcf-serverd.a \
	../libpromises/libpromises.la

libcf_serverd_a_SOURCES = \
        cf-serverd.c \
        cf-serverd-functions.c \
        cf-serverd-functions.h \
        server.c server.h \
        server_transform.c server_transform.h

if !HAVE_ENTERPRISE
libcf_serverd_a_SOURCES += \
	cf-serverd-enterprise-stubs.c cf-serverd-enterprise-stubs.h
endif

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-serverd
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
