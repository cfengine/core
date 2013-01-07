AM_CFLAGS = $(NOVA_CFLAGS) @CFLAGS@ -I$(srcdir)/../libpromises

sbin_PROGRAMS = \
        cf-monitord

cf_monitord_SOURCES = \
        verify_measurements.c \
        env_monitor.c env_monitor.h \
        monitoring.h \
        mon_cpu.c \
        mon_disk.c \
        mon_entropy.c \
        mon_load.c \
        mon_network_sniffer.c \
        mon_network.c \
        mon_processes.c \
        mon_temp.c \
        cf-monitord.c

cf_monitord_LDADD =
if HAVE_NOVA
cf_monitord_LDADD += ../nova/src/libcfmonitord.la
endif
cf_monitord_LDADD += ../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
