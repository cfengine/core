noinst_LIBRARIES = libcf-monitord.a

AM_CPPFLAGS = @CPPFLAGS@ \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libpromises \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(ENTERPRISE_CFLAGS)

LDADD = libcf-monitord.a \
	../libpromises/libpromises.la

libcf_monitord_a_SOURCES = \
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

if !BUILTIN_EXTENSIONS
 sbin_PROGRAMS = cf-monitord
endif

if !HAVE_ENTERPRISE
libcf_monitord_a_SOURCES += \
	cf-monitord-enterprise-stubs.c cf-monitord-enterprise-stubs.h
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
