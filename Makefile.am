#
#  Copyright 2021 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#
AUTOMAKE_OPTIONS = foreign

MAKEFLAGS = $(if $(filter-out 0,$(V)),,--no-print-directory --quiet)

LCOV_FLAGS = $(if $(filter-out 0,$(V)),,-q)

SUBDIRS = \
	libntech \
	libcfecompat \
	libcfnet \
	libenv \
	cf-check \
	libpromises \
	cf-agent \
	cf-execd \
	cf-key \
	cf-monitord \
	cf-promises \
	cf-runagent \
	cf-serverd \
	cf-testd \
	cf-upgrade \
	cf-net \
	cf-secret \
	misc \
	python \
	ext \
	examples \
	tests \
	contrib/vagrant-ci/centos-9s-x64


# Hide the buildsystem's username, at least with GNU tar.
TAR_OPTIONS = --owner=0 --group=0
export TAR_OPTIONS


EXTRA_DIST = ChangeLog INSTALL README.md LICENSE CFVERSION

doc_DATA = README.md ChangeLog

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

list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'

install-data-local:
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/inputs
	$(MKDIR_P) -m 755 $(DESTDIR)$(workdir)/modules
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/outputs
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/ppkeys
	$(MKDIR_P) -m 700 $(DESTDIR)$(workdir)/plugins
	$(MKDIR_P) -m 750 $(DESTDIR)$(workdir)/state


tar-package:
	pkgdir=`mktemp -d`  &&  export pkgdir  &&  \
	origdir=`pwd`       &&  export origdir &&  \
	$(MAKE) DESTDIR=$$pkgdir install  &&  \
	(   cd $$pkgdir  &&  \
	    find . -name '*.cf*' | xargs -n1 chmod go-w  &&  \
	    tardir=.  &&  \
	    $(am__tar) | GZIP=$(GZIP_ENV) gzip -c  \
	        > "$$origdir"/$(PACKAGE)-$(VERSION).pkg.tar.gz  \
	)  ;  \
	[ x$$pkgdir != x ]  &&  rm -rf $$pkgdir

#
# Code coverage
#

clean-coverage:
	find -L $(srcdir) -name '*.gcda' -delete

run-coverage:
	-$(MAKE) check -k


collect-coverage:
	mkdir -p coverage
	$(LCOV) $(LCOV_FLAGS) --capture --initial --directory . --output-file coverage/cfengine-lcov-base.info
	$(LCOV) $(LCOV_FLAGS) --capture --directory . --output-file coverage/lcov.info --test-name CFENGINE --no-checksum --compat-libtool
	$(LCOV) $(LCOV_FLAGS) -a coverage/cfengine-lcov-base.info -a coverage/lcov.info --output-file coverage/cfengine-lcov.info
	$(LCOV) $(LCOV_FLAGS) --remove coverage/lcov.info '/usr/include/*' --output-file coverage/lcov.info
	LANG=C $(LCOV_GENHTML) $(LCOV_FLAGS) --prefix . --output-directory coverage-html --title "CFEngine Code Coverage" --legend --show-details coverage/lcov.info
	@echo
	@echo " Code coverage information: file://"`pwd`"/coverage-html/index.html"
	@echo " Code coverage summary for IDEs: file://"`pwd`"/coverage/lcov.info"
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
$(srcdir)/libpromises/enterprise_extension.c: libpromises/extensions_template.c.pre libpromises/enterprise_extension.sed
	$(V_SED) $(SED) -f $(srcdir)/libpromises/enterprise_extension.sed $< > $@
$(srcdir)/libpromises/enterprise_extension.h: libpromises/extensions_template.h.pre libpromises/enterprise_extension.sed
	$(V_SED) $(SED) -f $(srcdir)/libpromises/enterprise_extension.sed $< > $@
V_PERL = $(cf__v_PERL_$(V))
cf__v_PERL_ = $(cf__v_PERL_$(AM_DEFAULT_VERBOSITY))
cf__v_PERL_0 = @echo "  PERL    " "$@";
cf__v_PERL_1 =
V_SED = $(cf__v_SED_$(V))
cf__v_SED_ = $(cf__v_SED_$(AM_DEFAULT_VERBOSITY))
cf__v_SED_0 = @echo "  SED     " "$@";
cf__v_SED_1 =

vm-check:
	$(MAKE) -C contrib/vagrant-ci/centos-9s-x64/ vm-check

vm-check-full:
	$(MAKE) -C contrib/vagrant-ci/centos-9s-x64/ vm-check-full

destroy-vms:
	$(MAKE) -C contrib/vagrant-ci/centos-9s-x64/ destroy-vms

static-check:
	tests/static-check/run.sh

static-check-f%:
	STATIC_CHECKS_FEDORA_VERSION=$* tests/static-check/run.sh

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
#
CLEANFILES = cfengine-lcov-base.info cfengine-lcov.info \
	$(BUILT_SOURCES)
