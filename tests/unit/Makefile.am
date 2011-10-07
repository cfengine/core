check_PROGRAMS = \
	alphalist_test	\
	assoc_test		\
	item_test

MOCKERY_SOURCES = cmockery.c mock_cfstream.c

alphalist_test_SOURCES = $(MOCKERY_SOURCES) alphalist_test.c
alphalist_test_LDADD = ../../src/libpromises.la

assoc_test_SOURCES = $(MOCKERY_SOURCES) assoc_test.c
assoc_test_LDADD = ../../src/libpromises.la

item_test_SOURCES = $(MOCKERY_SOURCES) item_test.c
item_test_LDADD = ../../src/libpromises.la

TESTS = $(check_PROGRAMS)
