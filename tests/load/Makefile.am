AM_CFLAGS = $(NOVA_CFLAGS) $(GCC_CFLAG) -I$(srcdir)/../../src -I../../src

EXTRA_DIST = run_db_load

check_PROGRAMS = db_load lastseen_load

TESTS = run_db_load

db_load_SOURCES = db_load.c 
db_load_LDADD = ../unit/libdb.la


lastseen_load_SOURCES = lastseen_load.c $(srcdir)/../../src/lastseen.c $(srcdir)/../../src/statistics.c
lastseen_load_LDADD = ../unit/libdb.la

