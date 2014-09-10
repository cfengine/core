AUTOMAKE_OPTIONS = foreign

MAKEFLAGS = $(if $(filter-out 0,$(V)),,--no-print-directory --quiet)

LCOV_FLAGS = $(if $(filter-out 0,$(V)),,-q)

SUBDIRS = libcompat libutils libcfnet libenv libpromises \
	cf-agent cf-execd cf-key cf-monitord \
	cf-promises cf-runagent cf-serverd cf-upgrade ext examples \
	tests

DIST_SUBDIRS = libcompat libutils libcfnet libenv libpromises cf-agent cf-execd	\
	cf-key cf-monitord cf-promises cf-runagent cf-serverd cf-upgrade ext	        \
	examples tests

# Set explicitly to avoid picking up {nova}/*.m4

DIST_COMMON = README.md Makefile.am Makefile.in configure AUTHORS aclocal.m4 \
	ChangeLog INSTALL config.guess config.sub depcomp install-sh \
	ltmain.sh missing ylwrap m4/acinclude.m4 m4/libtool.m4 m4/ltoptions.m4 \
	m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4 m4/cf3_with_library.m4 \
	m4/snprintf.m4 m4/old-autoconf.m4 m4/adl_recursive_eval.m4 m4/strndup.m4 \
	m4/cf3_check_proper_func.m4 m4/cf3_platforms.m4 m4/cf3_gcc_flags.m4 \
	m4/cf3_path_root_prog.m4 configure.ac autogen.sh configure_flags.env.in

EXTRA_DIST = ChangeLog INSTALL README.md LICENSE

docsdir = $(docdir)
docs_DATA = README.md ChangeLog

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~

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

install-data-local:
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/inputs
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/modules
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/outputs
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/ppkeys


#
# Code coverage
#

clean-coverage:
	find -L $(srcdir) -name '*.gcda' -delete

run-coverage:
	-$(MAKE) check -k


collect-coverage:
	$(LCOV) $(LCOV_FLAGS) --capture --initial --directory . --output-file cfengine-lcov-base.info
	$(LCOV) $(LCOV_FLAGS) --capture --directory . --output-file cfengine-lcov.info --test-name CFENGINE --no-checksum --compat-libtool
	$(LCOV) $(LCOV_FLAGS) -a cfengine-lcov-base.info -a cfengine-lcov.info --output-file cfengine-lcov.info
	$(LCOV) $(LCOV_FLAGS) --remove cfengine-lcov.info '/usr/include/*' --output-file cfengine-lcov.info
	LANG=C $(LCOV_GENHTML) $(LCOV_FLAGS) --prefix . --output-directory coverage-html --title "CFEngine Code Coverage" --legend --show-details cfengine-lcov.info
	@echo
	@echo " Code coverage information: file://"`pwd`"/coverage-html/index.html"
	@echo

coverage: clean-coverage run-coverage collect-coverage

################################################################################
# Identical to what is in libpromises/Makefile.am.
# This is because there is a circular dependency libcfnet <-> libpromises
# so we need to generate it before everything else.
################################################################################
BUILT_SOURCES = \
	$(srcdir)/libpromises/enterprise_extension.c \
	$(srcdir)/libpromises/enterprise_extension.h

ENTERPRISE_EXTENSION_GENERATE = $(SED) -f $(srcdir)/libpromises/enterprise_extension.sed $< > $@
ENTERPRISE_EXTENSION_MAKE = @if test -z "$(V)"; then echo "  SED   " $@; else echo "$(ENTERPRISE_EXTENSION_GENERATE)"; fi; $(ENTERPRISE_EXTENSION_GENERATE)
$(srcdir)/libpromises/enterprise_extension.c: libpromises/extensions_template.c libpromises/enterprise_extension.sed
	$(ENTERPRISE_EXTENSION_MAKE)
$(srcdir)/libpromises/enterprise_extension.h: libpromises/extensions_template.h libpromises/enterprise_extension.sed
	$(ENTERPRISE_EXTENSION_MAKE)


#
# Get everything removed down to where rebuilding requires:
# "make; make install"
#
CLEANFILES = cfengine-lcov-base.info cfengine-lcov.info \
	$(BUILT_SOURCES)
