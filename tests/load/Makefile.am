AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src -I../../src

check_PROGRAMS = \
	db_load

db_load_SOURCES = db_load.c $(srcdir)/../../src/dbm_tokyocab.c $(srcdir)/../../src/dbm_api.c $(srcdir)/../../src/dbm_quick.c $(srcdir)/../../src/alloc.c
db_load_LDADD = ../../pub/libcfpub.la

TESTS = run_db_load

