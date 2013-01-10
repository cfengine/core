AM_CFLAGS = $(NOVA_CFLAGS) -I$(top_srcdir)/libpromises

noinst_PROGRAMS = mock-package-manager

mock_package_manager_SOURCES = mock-package-manager.c
mock_package_manager_LDADD = ../../libpromises/libpromises.la


EXTRA_DIST = default.cf.sub testall

TESTS = testall
TESTS_ENVIRONMENT = env AGENT=`pwd`/../../cf-agent/cf-agent CF_PROMISES=`pwd`/../../cf-promises/cf-promises CF_SERVERD=`pwd`/../../cf-serverd/cf-serverd MOCK_PACKAGE_MANAGER=`pwd`/mock-package-manager

dist-hook:
	tar -c -f - [0-9]* | tar -x -C $(distdir) -f -

CLEANFILES = *.gcno *.gcda
