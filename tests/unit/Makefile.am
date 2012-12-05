SUBDIRS = data

AM_CFLAGS = $(NOVA_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src -DTESTDATADIR='"$(srcdir)/data"'

LDADD = ../../src/libpromises.la libtest.la

check_LTLIBRARIES = libtest.la
libtest_la_SOURCES = cmockery.c cmockery.h schema.h test.c test.h ../../src/alloc.c
libtest_la_LIBADD = ../../pub/libcfpub.la

check_LTLIBRARIES += libstr.la
libstr_la_SOURCES = ../../src/string_lib.c ../../src/writer.c
libstr_la_LIBADD = libtest.la


check_LTLIBRARIES += libdb.la
libdb_la_SOURCES = ../../src/dbm_api.c ../../src/dbm_quick.c ../../src/dbm_tokyocab.c ../../src/dbm_lib.c ../../src/patches.c ../../src/dbm_migration.c ../../src/dbm_migration_lastseen.c ../../src/dbm_migration_bundles.c ../../src/atexit.c
libdb_la_LIBADD = libstr.la

check_PROGRAMS = \
	alphalist_test \
	arg_split_test \
	assoc_test \
	atexit_test \
	csv_writer_test \
	item_test \
	rlist_test \
	domainname_test \
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
	policy_test \
	sort_test \
	file_name_test \
	logging_test \
	granules_test \
	scope_test \
	conversion_test \
	files_interfaces_test \
	timeout_test \
	connection_management_test

TESTS = $(check_PROGRAMS)

#
# OS X uses real system calls instead of our stubs unless this option is used
#
TESTS_ENVIRONMENT = DYLD_FORCE_FLAT_NAMESPACE=yes

atexit_test_SOURCES = atexit_test.c
atexit_test_LDADD = libtest.la libdb.la

csv_writer_test_SOURCES = csv_writer_test.c ../../src/csv_writer.c
csv_writer_test_LDADD = libtest.la libstr.la

conversion_test_SOURCES = conversion_test.c ../../src/conversion.c

rlist_test_SOURCES = rlist_test.c \
	../../src/rlist.c
rlist_test_LDADD = libtest.la libstr.la

set_domainname_test_SOURCES = set_domainname_test.c \
	../../src/sysinfo.c
set_domainname_test_LDADD = libstr.la

str_test_SOURCES = str_test.c
str_test_LDADD = libstr.la

xml_writer_test_SOURCES = xml_writer_test.c ../../src/xml_writer.c
xml_writer_test_LDADD = libtest.la libstr.la

#
# file_writer_test overrides fclose(3) which causes gcov to fail as gcov
# instrumentation code uses fclose(3).
#
file_writer_test_SOURCES = file_writer_test.c gcov-stub.c
file_writer_test_CFLAGS = $(AM_CFLAGS) -fno-profile-arcs -fno-test-coverage
file_writer_test_LDADD = libstr.la
file_writer_test_LDLFAGS =

db_test_SOURCES = db_test.c
db_test_LDADD = libdb.la

lastseen_test_SOURCES = lastseen_test.c ../../src/lastseen.c ../../src/statistics.c
lastseen_test_LDADD = libdb.la

lastseen_migration_test_SOURCES = lastseen_migration_test.c ../../src/lastseen.c ../../src/statistics.c
lastseen_migration_test_LDADD = libdb.la

dbm_migration_bundles_test_SOURCES = dbm_migration_bundles_test.c
dbm_migration_bundles_test_LDADD = libdb.la


CLEANFILES = *.gcno *.gcda

package_versions_compare_test_SOURCES = package_versions_compare_test.c ../../src/verify_packages.c ../../src/vercmp.c ../../src/vercmp_internal.c

sort_test_SOURCES = sort_test.c ../../src/sort.c
sort_test_LDADD = libtest.la

connection_management_test_SOURCES = connection_management_test.c ../../src/server.c
connection_management_test_LDADD = ../../src/libpromises.la libtest.la
