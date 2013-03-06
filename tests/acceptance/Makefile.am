AM_CFLAGS = $(NOVA_CFLAGS) -I$(top_srcdir)/libpromises -I$(srcdir)/../../libutils

noinst_PROGRAMS = mock-package-manager

mock_package_manager_SOURCES = mock-package-manager.c
mock_package_manager_LDADD = ../../libpromises/libpromises.la


EXTRA_DIST = default.cf.sub testall

TESTS = testall
TESTS_ENVIRONMENT = env \
	AGENT=`pwd`/../../cf-agent/cf-agent \
	CF_PROMISES=`pwd`/../../cf-promises/cf-promises \
	CF_SERVERD=`pwd`/../../cf-serverd/cf-serverd \
	CF_KEY=`pwd`/../../cf-key/cf-key \
	MOCK_PACKAGE_MANAGER=`pwd`/mock-package-manager

testall: setperms

# Permissions should be already set in distributed tarballs, this is for compiling from repository
setperms:
	@if find $(srcdir) -name '*.cf*' | xargs ls -l | cut -c5-10 | grep -q w; \
	then \
		echo -e "\n==== WARNING chmod'ing $(srcdir) to remove writable permissions! ====\n"; \
		chmod -R go-w $(srcdir); \
	fi

dist-hook:
	cd $(srcdir); tar -c -f - [0-9]* | tar -x -C $(abspath $(distdir)) -f -
	chmod -R go-w $(distdir)

CLEANFILES = *.gcno *.gcda
