AM_CFLAGS = $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS)
INCLUDES = -I$(abs_top_srcdir)/src


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
dnswrapper_so_CFLAGS = -fPIC -fpic
dnswrapper_so_LDFLAGS = -shared

#TESTS_ENVIRONMENT = env LD_PRELOAD="$(abs_builddir)/dnswrapper.so"
endif



TESTS = testall
