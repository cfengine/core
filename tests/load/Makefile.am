AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src -I../../src

check_PROGRAMS = \
	tcdb_load

tcdb_load_SOURCES = tcdb_load.c $(srcdir)/../../src/dbm_tokyocab.c $(srcdir)/../../src/dbm_api.c $(srcdir)/../../src/dbm_quick.c $(srcdir)/../../src/alloc.c
tcdb_load_LDADD = ../../pub/libcfpub.la

TESTS =
