AUTOMAKE_OPTIONS = foreign

MAKEFLAGS = $(if $(filter-out 0,$(V)),,--no-print-directory --quiet)

LCOV_FLAGS = $(if $(filter-out 0,$(V)),,-q)

SUBDIRS = pub $(NOVA_SUBDIR) $(CONSTELLATION_SUBDIR) src docs examples masterfiles tests/unit $(CFMOD_SUBDIR)

DIST_SUBDIRS = pub src docs examples masterfiles

# Set explicitly to avoid picking up {nova,constellation,galaxy}/*.m4

DIST_COMMON = README Makefile.am Makefile.in configure AUTHORS aclocal.m4 \
	ChangeLog INSTALL config.guess config.sub depcomp install-sh \
	ltmain.sh missing ylwrap m4/acinclude.m4 m4/libtool.m4 m4/ltoptions.m4 \
	m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4 m4/cf3_with_library.m4 \
	configure.ac autogen.sh

EXTRA_DIST = ChangeLog INSTALL README LICENSE

docdir = @docdir@
doc_DATA = README ChangeLog

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES =

#
# Get everything removed down to where rebuilding requires:
# "configure; make; make install"
#
DISTCLEANFILES = 

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess config.sub \
	configure install-sh missing mkinstalldirs depcomp ylwrap \
	ltmain.sh mdate-sh

#
# Pass proper flags to aclocal to pick up Libtool macros
#
ACLOCAL_AMFLAGS = -I m4

dist-hook:
	mkdir $(distdir)/tests
	(cd $(srcdir)/tests && find . -name README -o -name testall -o -name '*.cf*' -a \! -path '*succeeded*' | tar -c -f - -T-) | tar -x -C $(distdir)/tests -v -f -

#
# Code coverage
#

coverage:
	-$(MAKE) check -k
	$(LCOV) $(LCOV_FLAGS) --capture --directory . --output-file cfengine-lcov.info --test-name CFENGINE --no-checksum --compat-libtool
	$(LCOV) $(LCOV_FLAGS) --remove cfengine-lcov.info '/usr/include/*' --output-file cfengine-lcov.info
	LANG=C $(LCOV_GENHTML) $(LCOV_FLAGS) --prefix . --output-directory coverage-html --title "CFEngine Code Coverage" --legend --show-details cfengine-lcov.info
	@echo
	@echo " Code coverage information: file://$(abs_top_builddir)/coverage-html/index.html"
	@echo
