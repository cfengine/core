AM_CFLAGS = $(NOVA_CFLAGS) $(GCC_CFLAG) -I$(top_srcdir)/src

noinst_PROGRAMS = mock-package-manager

mock_package_manager_SOURCES = mock-package-manager.c
mock_package_manager_LDADD = ../../src/libpromises.la


EXTRA_DIST = default.cf.sub testall

TESTS = testall
TESTS_ENVIRONMENT = env AGENT=`pwd`/../../src/cf-agent MOCK_PACKAGE_MANAGER=`pwd`/mock-package-manager

dist-hook:
	tar -c -f - [0-9]* | tar -x -C $(distdir) -f -

CLEANFILES = *.gcno *.gcda
