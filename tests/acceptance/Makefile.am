AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) $(GCC_CFLAG)
INCLUDES = -I$(top_srcdir)/src


noinst_PROGRAMS = mock-package-manager

mock_package_manager_SOURCES = mock-package-manager.c
mock_package_manager_LDADD = ../../src/libpromises.la



if LINUX
#
# noinst/check libraries are "convenience" static ones unless explicitly
# overriden.
#
noinst_PROGRAMS += dnswrapper.so
dnswrapper_so_LINK = $(CCLD) $(dnswrapper_so_CFLAGS) $(CFLAGS) $(dnswrapper_so_LDFLAGS) $(LDFLAGS) -o $@
dnswrapper_so_CPPFLAGS = -fPIC
dnswrapper_so_LDFLAGS = -shared

#TESTS_ENVIRONMENT = env LD_PRELOAD="dnswrapper.so"
endif


EXTRA_DIST = default.cf.sub testall

TESTS = testall
TESTS_ENVIRONMENT = env AGENT=`pwd`/../../src/cf-agent MOCK_PACKAGE_MANAGER=`pwd`/mock-package-manager

dist-hook:
	tar -c -f - [0-9]* --exclude '*svn*' | tar -x -C $(distdir) -f -

CLEANFILES = *.gcno *.gcda
