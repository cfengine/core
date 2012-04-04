AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src -I../../src

noinst_LTLIBRARIES = libdb.la

libdb_la_SOURCES = $(srcdir)/../../src/dbm_tokyocab.c $(srcdir)/../../src/dbm_api.c $(srcdir)/../../src/dbm_quick.c $(srcdir)/../../src/dbm_lib.c  $(srcdir)/../../src/alloc.c $(srcdir)/../../src/patches.c
libdb_la_LIBADD = ../../pub/libcfpub.la

EXTRA_DIST = run_db_load

check_PROGRAMS = db_load lastseen_load

TESTS = run_db_load

db_load_SOURCES = db_load.c 
db_load_LDADD = libdb.la


lastseen_load_SOURCES = lastseen_load.c $(srcdir)/../../src/lastseen.c $(srcdir)/../../src/statistics.c
lastseen_load_LDADD = libdb.la

