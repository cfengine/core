AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) -I$(srcdir)/../unit

check_PROGRAMS = \
	tcdb_load

tcdb_load_SOURCES = tcdb_load.c
tcdb_load_LDADD = ../../src/dbm_tokyocab.c ../../src/alloc.c

TESTS =
