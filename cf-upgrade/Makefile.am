sbin_PROGRAMS = cf-upgrade

LIBS=
CORE_LIBS=
cf_upgrade_CPPFLAGS=
cf_upgrade_CFLAGS = $(CORE_CFLAGS)
cf_upgrade_LDFLAGS = $(LIBS) $(CORE_LIBS)


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
