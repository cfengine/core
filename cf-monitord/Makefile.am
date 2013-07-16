sbin_PROGRAMS = cf-monitord
noinst_LIBRARIES = libcf-monitord.a

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libpromises \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

cf_monitord_LDADD = ../libpromises/libpromises.la

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

libcf_monitord_a_SOURCES = $(cf_monitord_SOURCES)

if !HAVE_ENTERPRISE
cf_monitord_SOURCES += \
	cf-monitord-enterprise-stubs.c cf-monitord-enterprise-stubs.h
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
