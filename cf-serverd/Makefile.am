AM_CFLAGS = $(NOVA_CFLAGS) @CFLAGS@ -I$(srcdir)/../libpromises

sbin_PROGRAMS = \
        cf-serverd

cf_serverd_SOURCES = \
        cf-serverd.c \
        cf-serverd-functions.c \
        cf-serverd-functions.h \
        server.c server.h \
        server_transform.c \
        enterprise_stubs_serverd.c

cf_serverd_LDADD =
if HAVE_NOVA
cf_serverd_LDADD += ../nova/libcfserverd/libcfserverd.la
endif
cf_serverd_LDADD += ../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
