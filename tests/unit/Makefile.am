#
#  Copyright 2017 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by CFEngine AS.
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
	$(ENTERPRISE_CFLAGS) \
	-I$(srcdir)/../../libcfnet \
	-I$(srcdir)/../../libenv \
	-I$(srcdir)/../../libpromises \
	-I$(srcdir)/../../libutils \
	-I$(srcdir)/../../cf-monitord \
	-I$(srcdir)/../../cf-serverd \
	-I$(srcdir)/../../cf-agent \
	-I$(srcdir)/../../cf-execd \
	-I$(srcdir)/../../cf-key \
	-DTESTDATADIR='"$(srcdir)/data"'

LDADD = ../../libpromises/libpromises.la libtest.la

# automake does not support "maude_LIBS" variables. We can only alter
# the generic LIBS one. In case the functions are mocked in the test
# implementation, then we are pretty sure that they will be overriden by
# our local implementation. So we include *everything*...
LIBS = $(CORE_LIBS)
AM_LDFLAGS = $(CORE_LDFLAGS)


check_LTLIBRARIES = libtest.la
libtest_la_SOURCES = cmockery.c cmockery.h schema.h test.c test.h \
	../../libutils/alloc.c \
	../../libpromises/patches.c \
	../../libpromises/constants.c \
        ../../libpromises/known_dirs.c \
        ../../libutils/map.c \
        ../../libutils/array_map.c \
        ../../libutils/hash.c \
        ../../libutils/hash_map.c

libtest_la_LIBADD = ../../libcompat/libcompat.la

check_LTLIBRARIES += libstr.la
libstr_la_SOURCES = \
	../../libutils/buffer.c \
	../../libutils/encode.c \
	../../libutils/logging.c \
	../../libutils/misc_lib.c \
	../../libutils/regex.c \
	../../libutils/sequence.c \
	../../libutils/string_lib.c \
	../../libutils/writer.c
libstr_la_LIBADD = libtest.la


check_LTLIBRARIES += libdb.la
libdb_la_SOURCES = db_stubs.c \
	../../libpromises/dbm_api.c \
	../../libpromises/mutex.c \
	../../libpromises/dbm_quick.c \
	../../libpromises/dbm_tokyocab.c \
	../../libpromises/dbm_lmdb.c \
	../../libpromises/dbm_migration.c \
	../../libpromises/dbm_migration_lastseen.c \
	../../libutils/logging.c \
	../../libutils/atexit.c
if HPUX
libdb_la_SOURCES += ../../libpromises/cf3globals.c
endif
libdb_la_LIBADD = libstr.la ../../libutils/libutils.la
#libdb_la_CPPFLAGS = $(LMDB_CPPFLAGS) $(TOKYOCABINET_CPPFLAGS) $(QDBM_CPPFLAGS)
# Make sure that source files are compiled to separate object files libdb_la-file.o
libdb_la_CFLAGS = $(AM_CFLAGS)

check_PROGRAMS = \
	arg_split_test \
	assoc_test \
	atexit_test \
	csv_writer_test \
	item_test \
	rlist_test \
	domainname_test \
	set_test \
	set_domainname_test \
	str_test \
	json_test \
	csv_parser_test \
	evalfunction_test \
	eval_context_test \
	regex_test \
	alloc_test \
	string_writer_test \
	file_writer_test \
	xml_writer_test \
	sequence_test \
	lastseen_test \
	lastseen_migration_test \
	changes_migration_test \
	db_test \
	db_concurrent_test \
	misc_lib_test \
	item_lib_test \
	crypto_symmetric_test \
	persistent_lock_test  \
	thread_test \
	package_versions_compare_test \
	files_lib_test \
	file_lib_test \
	map_test \
	parsemode_test \
	parser_test \
	policy_test \
	sort_test \
	file_name_test \
	logging_test \
	logging_timestamp_test \
	granules_test \
	scope_test \
	conversion_test \
	files_interfaces_test \
	refcount_test \
	list_test \
	buffer_test \
	cf_key_functions_test \
	connection_management_test \
	expand_test \
	string_expressions_test \
	var_expressions_test \
	process_terminate_unix_test \
	process_test \
	exec-config-test \
	generic_agent_test \
	syntax_test \
	sysinfo_test \
	ipaddress_test \
	hashes_test \
	rb-tree-test \
	variable_test \
	protocol_test \
	mon_cpu_test \
	mon_load_test \
	mon_processes_test \
	mustache_test \
	class_test \
	version_test \
	hash_test \
	key_test \
	cf_upgrade_test \
	queue_test \
	matching_test \
	ring_buffer_test \
	strlist_test \
	addr_lib_test \
	libcompat_test \
	split_process_line_test \
        new_packages_promise_test

if HAVE_AVAHI_CLIENT
if HAVE_AVAHI_COMMON
check_PROGRAMS += \
    findhub_test \
    avahi_config_test
endif
endif

check_SCRIPTS = dynamic_dependency_test.sh

if WITH_INIT_D_SCRIPT
check_SCRIPTS += init_script_test.sh
endif

EXTRA_DIST   += $(check_SCRIPTS)

TESTS = $(check_PROGRAMS) $(check_SCRIPTS)

if AIX
XFAIL_TESTS = evalfunction_test     # Redmine 6318
XFAIL_TESTS += set_domainname_test  # Redmine 6317
endif

if HPUX
XFAIL_TESTS = mon_load_test # Redmine #3569
endif

#
# OS X uses real system calls instead of our stubs unless this option is used
#
TESTS_ENVIRONMENT = DYLD_FORCE_FLAT_NAMESPACE=yes

atexit_test_SOURCES = atexit_test.c
atexit_test_LDADD = libtest.la libdb.la

csv_writer_test_SOURCES = csv_writer_test.c ../../libutils/csv_writer.c
csv_writer_test_LDADD = libtest.la libstr.la

conversion_test_SOURCES = conversion_test.c ../../libpromises/conversion.c

if !BUILTIN_EXTENSIONS
    check_PROGRAMS += enterprise_extension_test

    enterprise_extension_test_SOURCES = enterprise_extension_test.c

    check_LTLIBRARIES += cfengine-enterprise.la
    cfengine_enterprise_la_SOURCES = enterprise_extension_test_lib.c
    cfengine_enterprise_la_LDFLAGS = $(AM_LDFLAGS) \
	-avoid-version -module -shared -export-dynamic -rpath /
    EXTRA_enterprise_extension_test_DEPENDENCIES = cfengine-enterprise.la
endif

set_domainname_test_SOURCES = set_domainname_test.c
set_domainname_test_LDADD = libstr.la ../../libpromises/libpromises.la

str_test_SOURCES = str_test.c
str_test_LDADD = libstr.la

xml_writer_test_SOURCES = xml_writer_test.c ../../libutils/xml_writer.c
xml_writer_test_LDADD = libtest.la libstr.la

list_test_SOURCES = list_test.c 

refcount_test_SOURCES = refcount_test.c ../../libutils/refcount.c

buffer_test_SOURCES = buffer_test.c ../../libutils/buffer.c
# Workaround for object file basename conflicts, search the web for
# "automake created with both libtool and without"
buffer_test_CPPFLAGS = $(AM_CPPFLAGS)

csv_parser_test_SOURCES = csv_parser_test.c ../../libutils/csv_parser.c
csv_parser_test_LDADD = libtest.la ../../libutils/libutils.la

regex_test_SOURCES = regex_test.c ../../cf-agent/match_scope.c

cf_key_functions_test_SOURCES = cf_key_functions_test.c ../../cf-key/cf-key-functions.c

ipaddress_test_SOURCES = ipaddress_test.c 

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

findhub_test_SOURCES = findhub_test.c ../../cf-agent/findhub.c ../../cf-agent/load_avahi.c

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

#file_writer_test_CPPFLAGS = -I$(top_srcdir)/libutils
file_writer_test_SOURCES = file_writer_test.c gcov-stub.c

db_test_SOURCES = db_test.c
db_test_LDADD = libtest.la ../../libpromises/libpromises.la

db_concurrent_test_SOURCES = db_concurrent_test.c
#db_concurrent_test_CPPFLAGS = $(libdb_la_CPPFLAGS)
db_concurrent_test_LDADD = libdb.la

lastseen_test_SOURCES = lastseen_test.c \
	../../libpromises/item_lib.c  ../../libpromises/lastseen.c ../../libutils/statistics.c
#lastseen_test_CPPFLAGS = $(libdb_la_CPPFLAGS)
lastseen_test_LDADD = libdb.la ../../libpromises/libpromises.la

lastseen_migration_test_SOURCES = lastseen_migration_test.c \
	../../libpromises/lastseen.c ../../libutils/statistics.c ../../libpromises/item_lib.c
#lastseen_migration_test_CPPFLAGS = $(libdb_la_CPPFLAGS)
lastseen_migration_test_LDADD = libdb.la ../../libpromises/libpromises.la

CLEANFILES = *.gcno *.gcda cfengine-enterprise.so

package_versions_compare_test_SOURCES = package_versions_compare_test.c ../../cf-agent/package_module.c ../../cf-agent/verify_packages.c ../../cf-agent/verify_new_packages.c ../../cf-agent/vercmp.c ../../cf-agent/vercmp_internal.c ../../cf-agent/retcode.c ../../cf-agent/match_scope.c

#package_versions_compare_test_CPPFLAGS = $(AM_CPPFLAGS)
package_versions_compare_test_LDADD = ../../libpromises/libpromises.la libtest.la

file_lib_test_SOURCES = file_lib_test.c \
	../../libutils/file_lib.c \
	../../libutils/logging.c \
	../../libutils/misc_lib.c \
	../../libutils/string_lib.c \
	../../libutils/sequence.c \
	../../libutils/unix_dir.c \
	../../libutils/writer.c
file_lib_test_LDADD = libtest.la
file_lib_test_CPPFLAGS = $(AM_CPPFLAGS) -DTEST_SYMLINK_ATOMICITY

sort_test_SOURCES = sort_test.c
sort_test_LDADD = libtest.la ../../libpromises/libpromises.la

logging_test_SOURCES = logging_test.c ../../libpromises/syslog_client.c
logging_test_LDADD = libtest.la ../../libutils/libutils.la

logging_timestamp_test_SOURCES = logging_timestamp_test.c ../../libutils/logging.h
logging_timestamp_test_LDADD = libtest.la ../../libutils/libutils.la

connection_management_test_SOURCES = connection_management_test.c ../../cf-serverd/server_common.c ../../cf-serverd/server_tls.c
connection_management_test_LDADD = ../../libpromises/libpromises.la libtest.la ../../cf-serverd/libcf-serverd.la

rlist_test_SOURCES = rlist_test.c \
	../../libpromises/rlist.c
rlist_test_LDADD = libtest.la ../../libpromises/libpromises.la
# Workaround for object file basename conflicts, search the web for
# "automake created with both libtool and without"
rlist_test_CPPFLAGS = $(AM_CPPFLAGS)

process_test_LDADD = libtest.la ../../libpromises/libpromises.la

if LINUX

check_PROGRAMS += linux_process_test

linux_process_test_SOURCES = linux_process_test.c \
	../../libpromises/process_unix.c \
	../../libpromises/process_linux.c \
	../../libutils/file_lib.c
linux_process_test_LDADD = libtest.la ../../libutils/libutils.la

endif

if AIX

check_PROGRAMS += aix_process_test

aix_process_test_SOURCES = aix_process_test.c \
	../../libpromises/process_unix.c \
	../../libpromises/process_aix.c \
	../../libutils/file_lib.c
aix_process_test_LDADD = libtest.la ../../libutils/libutils.la

endif

if SOLARIS

check_PROGRAMS += solaris_process_test

solaris_process_test_SOURCES = solaris_process_test.c \
	../../libpromises/process_unix.c \
	../../libpromises/process_solaris.c \
	../../libutils/file_lib.c
solaris_process_test_LDADD = libtest.la ../../libutils/libutils.la

endif

process_terminate_unix_test_SOURCES = process_terminate_unix_test.c \
	../../libpromises/process_unix.c
process_terminate_unix_test_LDADD = libtest.la ../../libutils/libutils.la

exec_config_test_SOURCES = exec-config-test.c \
	../../cf-execd/exec-config.c ../../cf-execd/execd-config.c
exec_config_test_LDADD = libtest.la ../../libpromises/libpromises.la

sysinfo_test_LDADD = libtest.la \
	../../libenv/libenv.la \
	../../libpromises/libpromises.la

mon_cpu_test_SOURCES = mon_cpu_test.c ../../cf-monitord/mon.h ../../cf-monitord/mon_cpu.c
mon_cpu_test_LDADD = ../../libpromises/libpromises.la libtest.la

mon_load_test_SOURCES = mon_load_test.c ../../cf-monitord/mon.h ../../cf-monitord/mon_load.c
mon_load_test_LDADD = ../../libpromises/libpromises.la libtest.la

mon_processes_test_SOURCES = mon_processes_test.c ../../cf-monitord/mon.h ../../cf-monitord/mon_processes.c
mon_processes_test_LDADD = ../../libpromises/libpromises.la libtest.la

# tls_generic_test uses stub functions interposition which does not work (yet)
# under OS X. Another way of stubbing functions from libpromises is needed.
if !XNU
check_PROGRAMS += tls_generic_test
tls_generic_test_SOURCES = tls_generic_test.c
tls_generic_test_LDADD = libtest.la \
	../../libutils/libutils.la \
	../../libpromises/libpromises.la \
	../../libcfnet/libcfnet.la \
	../../cf-serverd/libcf-serverd.la
endif

version_test_SOURCES = version_test.c

hash_test_SOURCES = hash_test.c
hash_test_LDADD = ../../libutils/libutils.la libtest.la

key_test_SOURCES = key_test.c
key_test_LDADD = ../../libpromises/libpromises.la ../../libutils/libutils.la libtest.la

strlist_test_SOURCES = strlist_test.c ../../cf-serverd/strlist.c ../../cf-serverd/strlist.h

libcompat_test_CPPFLAGS = -I$(top_srcdir)/libcompat
libcompat_test_SOURCES = libcompat_test.c

CFUPGRADE=../../cf-upgrade
cf_upgrade_test_SOURCES = cf_upgrade_test.c $(CFUPGRADE)/alloc-mini.c $(CFUPGRADE)/alloc-mini.h $(CFUPGRADE)/command_line.c \
$(CFUPGRADE)/command_line.h $(CFUPGRADE)/configuration.c $(CFUPGRADE)/configuration.h $(CFUPGRADE)/log.c $(CFUPGRADE)/log.h \
$(CFUPGRADE)/process.c $(CFUPGRADE)/process.h $(CFUPGRADE)/update.c $(CFUPGRADE)/update.h
cf_upgrade_test_CFLAGS = -I$(CFUPGRADE)

queue_test_SOURCES = queue_test.c

if !NT
init_script_test_helper_SOURCES = init_script_test_helper.c
init_script_test.sh: init_script_test_helper
CLEANFILES += init_script_test_helper
endif
EXTRA_DIST += init_script_test_helper.c
EXTRA_DIST += init_script_test.sh
