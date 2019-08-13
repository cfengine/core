#
#  Copyright 2019 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#
noinst_LTLIBRARIES = libcf-monitord.la

AM_CPPFLAGS = \
	-I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libenv \
	-I$(srcdir)/../libpromises \
	$(PCRE_CPPFLAGS) \
	$(OPENSSL_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_monitord_la_LIBADD = ../libpromises/libpromises.la

libcf_monitord_la_SOURCES = \
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
        history.c history.h \
        mon_cumulative.c mon_cumulative.h \
        probes.c probes.h \
        monitoring.c monitoring.h \
        cf-monitord.c

if LINUX
libcf_monitord_la_SOURCES += mon_io_linux.c mon_mem_linux.c
endif

if SOLARIS
libcf_monitord_la_SOURCES += mon_mem_solaris.c shared_kstat.c shared_kstat.h
endif

if !LINUX
libcf_monitord_la_SOURCES += mon_io_stub.c
if !SOLARIS
libcf_monitord_la_SOURCES += mon_mem_stub.c
endif
endif


if !BUILTIN_EXTENSIONS
 bin_PROGRAMS = cf-monitord
 # Workaround for automake madness (try removing it if you want to know why).
 cf_monitord_CFLAGS = $(AM_CFLAGS)
 cf_monitord_LDADD = libcf-monitord.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
