noinst_LTLIBRARIES = libcf-monitord.la

AM_CPPFLAGS = \
	$(OPENSSL_CPPFLAGS) \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libenv \
	-I$(srcdir)/../libpromises \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_monitord_la_LIBADD = ../libpromises/libpromises.la

libcf_monitord_la_SOURCES = \
        verify_measurements.c verify_measurements.h \
	cf-monitord-enterprise-stubs.c cf-monitord-enterprise-stubs.h \
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

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-monitord
 # Workaround for automake madness (try removing it if you want to know why).
 cf_monitord_CFLAGS = $(AM_CFLAGS)
 cf_monitord_LDADD = libcf-monitord.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
