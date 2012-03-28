AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src

LDADD = ../../src/libpromises.la libtest.la

check_LTLIBRARIES = libtest.la
libtest_la_SOURCES = cmockery.c cmockery.h test.c test.h ../../src/alloc.c
libtest_la_LIBADD = ../../pub/libcfpub.la

check_PROGRAMS = \
	alphalist_test \
	arg_split_test \
	assoc_test \
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
	db_test

TESTS = $(check_PROGRAMS)

csv_writer_test_SOURCES = csv_writer_test.c ../../src/csv_writer.c ../../src/writer.c
csv_writer_test_LDADD = libtest.la

rlist_test_SOURCES = rlist_test.c \
	../../src/rlist.c \
	../../src/writer.c
rlist_test_LDADD = libtest.la

set_domainname_test_SOURCES = set_domainname_test.c \
	../../src/sysinfo.c ../../src/string_lib.c ../../src/writer.c
set_domainname_test_LDADD = libtest.la

str_test_SOURCES = str_test.c ../../src/string_lib.c ../../src/writer.c
str_test_LDADD = libtest.la

xml_writer_test_SOURCES = xml_writer_test.c ../../src/xml_writer.c ../../src/writer.c
xml_writer_test_LDADD = libtest.la

#
# file_writer_test overrides fclose(3) which causes gcov to fail as gcov
# instrumentation code uses fclose(3).
#
file_writer_test_SOURCES = file_writer_test.c gcov-stub.c \
        ../../src/writer.c
file_writer_test_CFLAGS = $(AM_CFLAGS) -fno-profile-arcs -fno-test-coverage
file_writer_test_LDADD = libtest.la
file_writer_test_LDLFAGS =

db_test_SOURCES = db_test.c ../../src/dbm_api.c ../../src/dbm_quick.c ../../src/dbm_tokyocab.c ../../src/dbm_lib.c
db_test_LDADD = libtest.la

CLEANFILES = *.gcno *.gcda

