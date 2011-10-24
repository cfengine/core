AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS)

check_PROGRAMS = \
	alphalist_test \
	arg_split_test \
	assoc_test \
	item_test \
	rlist_test \
	domainname_test \
	set_domainname_test \
	str_test \
	json_test

TEST_SOURCES = test.c ../../src/alloc.c
MOCKERY_SOURCES = cmockery.c $(TEST_SOURCES)

alphalist_test_SOURCES = $(MOCKERY_SOURCES) alphalist_test.c
alphalist_test_LDADD = ../../src/libpromises.la

arg_split_test_SOURCES = $(MOCKERY_SOURCES) arg_split_test.c
arg_split_test_LDADD = ../../src/libpromises.la

assoc_test_SOURCES = $(MOCKERY_SOURCES) assoc_test.c
assoc_test_LDADD = ../../src/libpromises.la

item_test_SOURCES = $(MOCKERY_SOURCES) item_test.c
item_test_LDADD = ../../src/libpromises.la

rlist_test_SOURCES = $(MOCKERY_SOURCES) rlist_test.c
rlist_test_LDADD = ../../src/libpromises.la

domainname_test_SOURCES = $(MOCKERY_SOURCES) domainname_test.c
domainname_test_LDADD = ../../src/libpromises.la

set_domainname_test_SOURCES = $(MOCKERY_SOURCES) set_domainname_test.c \
	../../src/sysinfo.c ../../src/string_lib.c
set_domainname_test_LDADD = ../../pub/libcfpub.la

str_test_SOURCES = $(MOCKERY_SOURCES) str_test.c ../../src/string_lib.c
str_test_LDADD = ../../pub/libcfpub.la

json_test_SOURCES = $(MOCKERY_SOURCES) json_test.c ../../src/json.h
json_test_LDADD = ../../src/libpromises.la


TESTS = $(check_PROGRAMS)
