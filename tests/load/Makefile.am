AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src -I../../src

EXTRA_DIST = run_db_load

check_PROGRAMS = db_load

db_load_SOURCES = db_load.c $(srcdir)/../../src/dbm_tokyocab.c $(srcdir)/../../src/dbm_api.c $(srcdir)/../../src/dbm_quick.c $(srcdir)/../../src/dbm_lib.c  $(srcdir)/../../src/alloc.c $(srcdir)/../../src/patches.c
db_load_LDADD = ../../pub/libcfpub.la

TESTS = run_db_load

