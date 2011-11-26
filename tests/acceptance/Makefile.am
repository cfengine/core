AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS)

INCLUDES = -I$(abs_top_srcdir)/src

noinst_PROGRAMS = mock-package-manager

mock_package_manager_SOURCES = mock-package-manager.c
mock_package_manager_LDADD = ../../src/libpromises.la

TESTS = testall
