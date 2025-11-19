#
#  Copyright 2021 Northern.tech AS
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
# Just recursively include in dist tarball all data we need for unit tests
EXTRA_DIST = data

AM_CPPFLAGS = $(CORE_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS) \
	-I$(srcdir)/../../libcfecompat \
	-I$(srcdir)/../../libcfnet \
	-I$(srcdir)/../../libenv \
	-I$(srcdir)/../../libpromises \
	-I$(srcdir)/../../libntech/libutils \
	-I$(srcdir)/../../cf-monitord \
	-I$(srcdir)/../../cf-serverd \
	-I$(srcdir)/../../cf-agent \
	-I$(srcdir)/../../cf-execd \
	-I$(srcdir)/../../cf-key \
	-I$(srcdir)/../../cf-check \
	-DTESTDATADIR='"$(srcdir)/data"'

LDADD = ../../libpromises/libpromises.la libtest.la

# automake does not support "maude_LIBS" variables. We can only alter
# the generic LIBS one. In case the functions are mocked in the test
# implementation, then we are pretty sure that they will be overriden by
# our local implementation. So we include *everything*...
LIBS = $(CORE_LIBS)
AM_LDFLAGS = $(CORE_LDFLAGS)

AM_CFLAGS = $(PTHREAD_CFLAGS)


check_LTLIBRARIES = libtest.la
libtest_la_SOURCES = cmockery.c cmockery.h schema.h test.c test.h

libtest_la_LIBADD = ../../libntech/libcompat/libcompat.la


check_LTLIBRARIES += libdb.la
libdb_la_SOURCES = db_stubs.c \
	../../libpromises/dbm_api.c \
	../../libpromises/dbm_quick.c \
	../../libpromises/dbm_tokyocab.c \
	../../libpromises/dbm_lmdb.c \
	../../libpromises/dbm_migration.c \
	../../libpromises/dbm_migration_lastseen.c \
	../../libpromises/global_mutex.c \
	../../cf-check/backup.c \
	../../cf-check/diagnose.c \
	../../cf-check/lmdump.c \
	../../cf-check/repair.c \
	../../cf-check/replicate_lmdb.c \
	../../cf-check/utilities.c \
	../../cf-check/validate.c \
	../../libntech/libutils/logging.c \
	../../libntech/libutils/mutex.c \
	../../libntech/libutils/cleanup.c
if HPUX
libdb_la_SOURCES += ../../libpromises/cf3globals.c
endif
libdb_la_LIBADD = ../../libntech/libutils/libutils.la
#libdb_la_CPPFLAGS = $(LMDB_CPPFLAGS) $(TOKYOCABINET_CPPFLAGS) $(QDBM_CPPFLAGS)
# Make sure that source files are compiled to separate object files
# libdb_la-file.o
libdb_la_CFLAGS = $(AM_CFLAGS)

check_PROGRAMS = \
	iteration_test

TESTS = $(check_PROGRAMS)

processes_select_test_SOURCES = processes_select_test.c
processes_select_test_LDADD = libtest.la ../../libpromises/libpromises.la

#
# OS X uses real system calls instead of our stubs unless this option is used
#
TESTS_ENVIRONMENT = DYLD_FORCE_FLAT_NAMESPACE=yes

conversion_test_SOURCES = conversion_test.c ../../libpromises/conversion.c


set_domainname_test_SOURCES = set_domainname_test.c
set_domainname_test_LDADD = libtest.la ../../libpromises/libpromises.la

regex_test_SOURCES = regex_test.c ../../libpromises/match_scope.c

protocol_test_SOURCES = protocol_test.c \
	../../cf-serverd/server_common.c \
	../../cf-serverd/server_tls.c \
	../../cf-serverd/server.c \
	../../cf-serverd/cf-serverd-enterprise-stubs.c \
	../../cf-serverd/server_transform.c \
	../../cf-serverd/cf-serverd-functions.c \
	../../cf-serverd/server_access.c \
	../../cf-serverd/strlist.c
protocol_test_LDADD = ../../libpromises/libpromises.la libtest.la

if HAVE_AVAHI_CLIENT
if HAVE_AVAHI_COMMON

findhub_test_SOURCES = findhub_test.c \
	../../cf-agent/findhub.c \
	../../cf-agent/load_avahi.c

avahi_config_test_SOURCES = avahi_config_test.c \
	../../cf-serverd/server_common.c \
	../../cf-serverd/server_tls.c \
	../../cf-serverd/server.c \
	../../cf-serverd/server_transform.c \
	../../cf-serverd/cf-serverd-enterprise-stubs.c \
	../../cf-serverd/server_access.c \
	../../cf-serverd/server_classic.c \
	../../cf-serverd/strlist.c
avahi_config_test_LDADD = ../../libpromises/libpromises.la libtest.la

endif
endif

db_test_SOURCES = db_test.c
db_test_LDADD = libtest.la ../../libpromises/libpromises.la

db_concurrent_test_SOURCES = db_concurrent_test.c
#db_concurrent_test_CPPFLAGS = $(libdb_la_CPPFLAGS)
db_concurrent_test_LDADD = libtest.la libdb.la

lastseen_test_SOURCES = lastseen_test.c \
	../../libntech/libutils/statistics.c
#lastseen_test_CPPFLAGS = $(libdb_la_CPPFLAGS)
lastseen_test_LDADD = libtest.la ../../libpromises/libpromises.la

lastseen_migration_test_SOURCES = lastseen_migration_test.c
#lastseen_migration_test_CPPFLAGS = $(libdb_la_CPPFLAGS)
lastseen_migration_test_LDADD = ../../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda cfengine-enterprise.so

package_versions_compare_test_SOURCES = package_versions_compare_test.c \
	../../cf-agent/package_module.c \
	../../cf-agent/verify_packages.c \
	../../cf-agent/verify_new_packages.c \
	../../cf-agent/vercmp.c \
	../../cf-agent/vercmp_internal.c \
	../../cf-agent/retcode.c \
	../../libpromises/match_scope.c

#package_versions_compare_test_CPPFLAGS = $(AM_CPPFLAGS)
package_versions_compare_test_LDADD = ../../libpromises/libpromises.la \
	libtest.la

files_copy_test_SOURCES  = files_copy_test.c
files_copy_test_LDADD    = libtest.la ../../libpromises/libpromises.la

sort_test_SOURCES = sort_test.c
sort_test_LDADD = libtest.la ../../libpromises/libpromises.la

logging_test_SOURCES = logging_test.c \
	../../libpromises/syslog_client.c \
	../../libpromises/patches.c \
	../../libpromises/constants.c
logging_test_LDADD = libtest.la ../../libntech/libutils/libutils.la

connection_management_test_SOURCES = connection_management_test.c \
	../../cf-serverd/server_common.c \
	../../cf-serverd/server_tls.c
connection_management_test_LDADD = ../../libpromises/libpromises.la \
	 libtest.la \
	../../cf-serverd/libcf-serverd.la

rlist_test_SOURCES = rlist_test.c
rlist_test_LDADD = libtest.la ../../libpromises/libpromises.la
# Workaround for object file basename conflicts, search the web for
# "automake created with both libtool and without"
rlist_test_CPPFLAGS = $(AM_CPPFLAGS)

process_test_LDADD = libtest.la ../../libpromises/libpromises.la




process_terminate_unix_test_SOURCES = process_terminate_unix_test.c \
	../../libpromises/process_unix.c
process_terminate_unix_test_LDADD = libtest.la ../../libntech/libutils/libutils.la

exec_config_test_SOURCES = exec-config-test.c \
	../../cf-execd/exec-config.c ../../cf-execd/execd-config.c
exec_config_test_LDADD = libtest.la ../../libpromises/libpromises.la

sysinfo_test_LDADD = libtest.la \
	../../libenv/libenv.la \
	../../libpromises/libpromises.la

mon_cpu_test_SOURCES = mon_cpu_test.c \
	../../cf-monitord/mon.h \
	../../cf-monitord/mon_cpu.c
mon_cpu_test_LDADD = ../../libpromises/libpromises.la libtest.la

mon_load_test_SOURCES = mon_load_test.c \
	../../cf-monitord/mon.h \
	../../cf-monitord/mon_load.c
mon_load_test_LDADD = ../../libpromises/libpromises.la libtest.la

mon_processes_test_SOURCES = mon_processes_test.c \
	../../cf-monitord/mon.h \
	../../cf-monitord/mon_processes.c
mon_processes_test_LDADD = ../../libpromises/libpromises.la libtest.la

key_test_SOURCES = key_test.c
key_test_LDADD = ../../libpromises/libpromises.la \
	../../libntech/libutils/libutils.la \
	libtest.la

strlist_test_SOURCES = strlist_test.c \
	../../cf-serverd/strlist.c \
	../../cf-serverd/strlist.h

verify_databases_test_LDADD = ../../cf-agent/libcf-agent.la libtest.la

iteration_test_SOURCES = iteration_test.c

cf_upgrade_test_SOURCES = cf_upgrade_test.c \
	$(top_srcdir)/cf-upgrade/alloc-mini.c \
	$(top_srcdir)/cf-upgrade/alloc-mini.h \
	$(top_srcdir)/cf-upgrade/command_line.c \
	$(top_srcdir)/cf-upgrade/command_line.h \
	$(top_srcdir)/cf-upgrade/configuration.c \
	$(top_srcdir)/cf-upgrade/configuration.h \
	$(top_srcdir)/cf-upgrade/log.c $(top_srcdir)/cf-upgrade/log.h \
	$(top_srcdir)/cf-upgrade/process.c $(top_srcdir)/cf-upgrade/process.h \
	$(top_srcdir)/cf-upgrade/update.c \
	$(top_srcdir)/cf-upgrade/update.h
cf_upgrade_test_CPPFLAGS = -I$(top_srcdir)/cf-upgrade -I$(top_srcdir)/libntech/libutils -I$(top_srcdir)
cf_upgrade_test_LDADD = libtest.la ../../libntech/libcompat/libcompat.la

EXTRA_DIST += init_script_test_helper.c
EXTRA_DIST += init_script_test.sh
