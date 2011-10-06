check_PROGRAMS = alphalist_test

MOCKERY_SOURCES = cmockery.c mock_cfstream.c

alphalist_test_SOURCES = $(MOCKERY_SOURCES) alphalist_test.c
alphalist_test_LDADD = ../../src/libpromises.la

TESTS = $(check_PROGRAMS)
