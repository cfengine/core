# Just recursively include in dist tarball all data we need for unit tests
EXTRA_DIST = data

AM_CFLAGS = $(NOVA_CFLAGS) -I$(srcdir)/../../libcfnet -I$(srcdir)/../../libpromises -I$(srcdir)/../../libutils \
	-DTESTDATADIR='"$(srcdir)/data"' -I$(srcdir)/../../cf-serverd -I$(srcdir)/../../cf-agent -I$(srcdir)/../../cf-execd -I$(srcdir)/../../cf-key

LDADD = ../../libpromises/libpromises.la libtest.la

check_LTLIBRARIES = libtest.la
libtest_la_SOURCES = cmockery.c cmockery.h schema.h test.c test.h ../../libutils/alloc.c ../../libpromises/patches.c ../../libpromises/constants.c
libtest_la_LIBADD = ../../libcompat/libcompat.la

check_LTLIBRARIES += libstr.la
libstr_la_SOURCES = ../../libutils/string_lib.c ../../libutils/writer.c
libstr_la_LIBADD = libtest.la


check_LTLIBRARIES += libdb.la
libdb_la_SOURCES = ../../libpromises/dbm_api.c ../../libpromises/dbm_quick.c ../../libpromises/dbm_tokyocab.c ../../libpromises/dbm_migration.c ../../libpromises/dbm_migration_lastseen.c ../../libpromises/dbm_migration_bundles.c ../../libutils/atexit.c
if HPUX
libdb_la_SOURCES += ../../libpromises/cf3globals.c
endif
libdb_la_LIBADD = libstr.la

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
	evalfunction_test \
	regex_test \
	alloc_test \
	string_writer_test \
	file_writer_test \
	xml_writer_test \
	sequence_test \
	db_test \
	lastseen_test \
	lastseen_migration_test \
	dbm_migration_bundles_test \
	misc_lib_test \
	crypto_symmetric_test \
	persistent_lock_test  \
	thread_test \
	package_versions_compare_test \
	files_lib_test \
	map_test \
	parser_test \
	policy_test \
	sort_test \
	file_name_test \
	logging_test \
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
	exec-config-test \
	generic_agent_test \
	syntax_test \
       ipaddress_test \
	hashes_test 

if HAVE_AVAHI_CLIENT
if HAVE_AVAHI_COMMON
check_PROGRAMS += \
    findhub_test \
    avahi_config_test
endif
endif

TESTS = $(check_PROGRAMS)

#
# OS X uses real system calls instead of our stubs unless this option is used
#
TESTS_ENVIRONMENT = DYLD_FORCE_FLAT_NAMESPACE=yes

atexit_test_SOURCES = atexit_test.c
atexit_test_LDADD = libtest.la libdb.la

csv_writer_test_SOURCES = csv_writer_test.c ../../libutils/csv_writer.c
csv_writer_test_LDADD = libtest.la libstr.la

conversion_test_SOURCES = conversion_test.c ../../libpromises/conversion.c

set_domainname_test_SOURCES = set_domainname_test.c \
	../../libpromises/sysinfo.c
set_domainname_test_LDADD = libstr.la ../../libutils/libutils.la

str_test_SOURCES = str_test.c
str_test_LDADD = libstr.la

xml_writer_test_SOURCES = xml_writer_test.c ../../libutils/xml_writer.c
xml_writer_test_LDADD = libtest.la libstr.la

list_test_SOURCES = list_test.c 

refcount_test_SOURCES = refcount_test.c ../../libutils/refcount.c

buffer_test_SOURCES = buffer_test.c ../../libutils/buffer.c

cf_key_functions_test_SOURCES = cf_key_functions_test.c ../../cf-key/cf-key-functions.c

#ipaddress_test_SOURCES = ipaddress_test.c 

if HAVE_AVAHI_CLIENT
if HAVE_AVAHI_COMMON

findhub_test_SOURCES = findhub_test.c ../../cf-agent/findhub.c ../../cf-agent/load_avahi.c

if HAVE_NOVA
avahi_config_test_SOURCES = avahi_config_test.c ../../cf-serverd/server.c ../../cf-serverd/cf-serverd-enterprise-stubs.c ../../cf-serverd/server_transform.c
avahi_config_test_LDADD = libtest.la ../../nova/libcfserverd/libcfserverd.la
else
avahi_config_test_SOURCES = avahi_config_test.c ../../cf-serverd/server.c ../../cf-serverd/cf-serverd-enterprise-stubs.c ../../cf-serverd/server_transform.c
avahi_config_test_LDADD = ../../libpromises/libpromises.la libtest.la
endif

endif
endif
#
# file_writer_test overrides fclose(3) which causes gcov to fail as gcov
# instrumentation code uses fclose(3).
#
file_writer_test_SOURCES = file_writer_test.c gcov-stub.c
file_writer_test_CFLAGS = $(AM_CFLAGS) -fno-profile-arcs -fno-test-coverage
file_writer_test_LDADD = libstr.la
file_writer_test_LDFLAGS =

db_test_SOURCES = db_test.c
db_test_LDADD = libdb.la

lastseen_test_SOURCES = lastseen_test.c ../../libpromises/lastseen.c ../../libutils/statistics.c
lastseen_test_LDADD = libdb.la

lastseen_migration_test_SOURCES = lastseen_migration_test.c ../../libpromises/lastseen.c ../../libutils/statistics.c
lastseen_migration_test_LDADD = libdb.la

dbm_migration_bundles_test_SOURCES = dbm_migration_bundles_test.c
dbm_migration_bundles_test_LDADD = libdb.la


CLEANFILES = *.gcno *.gcda

package_versions_compare_test_SOURCES = package_versions_compare_test.c ../../cf-agent/verify_packages.c ../../cf-agent/vercmp.c ../../cf-agent/vercmp_internal.c ../../cf-agent/retcode.c

package_versions_compare_test_CPPFLAGS = $(AM_CPPFLAGS)
package_versions_compare_test_LDADD = ../../libpromises/libpromises.la libtest.la
if HAVE_NOVA
package_versions_compare_test_CPPFLAGS += -I$(abs_top_srcdir)/nova/libcfagent
package_versions_compare_test_LDADD += ../../nova/libcfagent/libcfagent.la
endif

sort_test_SOURCES = sort_test.c ../../libpromises/sort.c
sort_test_LDADD = libtest.la

logging_test_SOURCES = logging_test.c ../../libpromises/syslog_client.c mock_logging.c
logging_test_LDADD = libtest.la

connection_management_test_SOURCES = connection_management_test.c ../../cf-serverd/server.c
connection_management_test_LDADD = ../../libpromises/libpromises.la libtest.la

rlist_test_SOURCES = rlist_test.c \
       ../../libpromises/rlist.c ../../libutils/logging.c
rlist_test_LDADD = libtest.la libstr.la

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
	../../cf-execd/exec-config.c
exec_config_test_LDADD = libtest.la ../../libpromises/libpromises.la
