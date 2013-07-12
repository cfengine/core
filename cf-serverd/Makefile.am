sbin_PROGRAMS = cf-serverd

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

LDADD = ../libpromises/libpromises.la
if HAVE_ENTERPRISE
LDADD += ../enterprise/libcfserverd/libcfserverd.la
endif

cf_serverd_SOURCES = \
        cf-serverd.c \
        cf-serverd-functions.c \
        cf-serverd-functions.h \
        server.c server.h \
        server_transform.c server_transform.h

if !HAVE_ENTERPRISE
cf_serverd_SOURCES += \
	cf-serverd-enterprise-stubs.c cf-serverd-enterprise-stubs.h
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
