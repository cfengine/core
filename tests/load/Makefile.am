AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src -I../../src

check_PROGRAMS = \
	tcdb_load

tcdb_load_SOURCES = tcdb_load.c
tcdb_load_LDADD = $(srcdir)/../../src/dbm_tokyocab.c $(srcdir)/../../src/dbm_api.c $(srcdir)/../../src/dbm_quick.c $(srcdir)/../../src/alloc.c ../../pub/libcfpub.la

TESTS =
