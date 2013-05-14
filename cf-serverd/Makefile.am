sbin_PROGRAMS = cf-serverd

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(NOVA_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

LDADD = ../libpromises/libpromises.la
if HAVE_NOVA
LDADD += ../nova/libcfserverd/libcfserverd.la
endif

cf_serverd_SOURCES = \
        cf-serverd.c \
        cf-serverd-functions.c \
        cf-serverd-functions.h \
        server.c server.h \
        server_transform.c server_transform.h

if !HAVE_NOVA
cf_serverd_SOURCES += \
	cf-serverd-enterprise-stubs.c cf-serverd-enterprise-stubs.h
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
