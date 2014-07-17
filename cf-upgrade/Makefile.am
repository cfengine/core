sbin_PROGRAMS = cf-upgrade

LIBS=				 # This tool should not link to anything
AM_LDFLAGS=
AM_CPPFLAGS=

LDADD=../libcompat/libcompat.la

cf_upgrade_SOURCES = \
    alloc-mini.c alloc-mini.h \
    command_line.c command_line.h \
    configuration.c configuration.h \
    log.c log.h \
    process.c process.h \
    update.c update.h \
    cf-upgrade.c

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
