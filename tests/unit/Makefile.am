AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG)

LDADD = ../../src/libpromises.la libtest.la

check_LTLIBRARIES = libtest.la
libtest_la_SOURCES = cmockery.c test.c ../../src/alloc.c
libtest_la_LIBADD = ../../pub/libcfpub.la

check_PROGRAMS = \
	alphalist_test \
	arg_split_test \
	assoc_test \
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
	file_writer_test

TESTS = $(check_PROGRAMS)

set_domainname_test_SOURCES = set_domainname_test.c \
	../../src/sysinfo.c ../../src/string_lib.c
set_domainname_test_LDADD = libtest.la

str_test_SOURCES = str_test.c ../../src/string_lib.c
str_test_LDADD = libtest.la
