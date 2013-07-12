sbin_PROGRAMS = cf-monitord

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libpromises \
	$(ENTERPRISE_CPPFLAGS)

if HAVE_NOVA
AM_CPPFLAGS += -I$(abs_top_srcdir)/enterprise/libcfmonitord
endif

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

cf_monitord_LDADD = ../libpromises/libpromises.la
if HAVE_NOVA
cf_monitord_LDADD += ../enterprise/libcfmonitord/libcfmonitord.la
endif

cf_monitord_SOURCES = \
        verify_measurements.c verify_measurements.h \
        env_monitor.c env_monitor.h \
        mon.h \
        mon_cpu.c \
        mon_disk.c \
        mon_entropy.c \
        mon_load.c \
        mon_network_sniffer.c \
        mon_network.c \
        mon_processes.c \
        mon_temp.c \
        cf-monitord.c

if !HAVE_NOVA
cf_monitord_SOURCES += \
	cf-monitord-enterprise-stubs.c cf-monitord-enterprise-stubs.h
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
