#
#  Copyright (C) CFEngine AS
#
#  This file is part of CFEngine 3 - written and maintained by CFEngine AS.
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

SUBDIRS = 25_cf-execd

AM_CPPFLAGS = $(CORE_CPPFLAGS) $(ENTERPRISE_CFLAGS) \
	-I$(top_srcdir)/libpromises \
	-I$(top_srcdir)/libutils \
	-I$(top_srcdir)/libcfnet

AM_CFLAGS  = $(CORE_CFLAGS)
AM_LDFLAGS = $(CORE_LDFLAGS)
LIBS       = $(CORE_LIBS)


noinst_PROGRAMS =

noinst_LTLIBRARIES = libmock_package_manager.la
libmock_package_manager_la_SOURCES = mock_package_manager.c
libmock_package_manager_la_LIBADD = ../../libpromises/libpromises.la

if !BUILTIN_EXTENSIONS
noinst_PROGRAMS += mock_package_manager

mock_package_manager_SOURCES =
mock_package_manager_LDADD = libmock_package_manager.la

noinst_PROGRAMS += no_fds
no_fds_LDADD = ../../libutils/libutils.la
endif # !BUILTIN_EXTENSIONS

if HAVE_LIBXML2
  noinst_PROGRAMS += xml-c14nize

  xml_c14nize_CPPFLAGS = \
	-I$(top_srcdir)/libpromises \
	-I$(srcdir)/../../libutils \
	$(LIBXML2_CPPFLAGS)

  xml_c14nize_CFLAGS  = $(LIBXML2_CFLAGS)
  xml_c14nize_LDFLAGS = $(LIBXML2_LDFLAGS)
  xml_c14nize_LDADD = ../../libutils/libutils.la \
	$(LIBXML2_LIBS)
endif

TESTS =
TESTS_ENVIRONMENT = env \
	AGENT=`pwd`/../../cf-agent/cf-agent \
	CF_PROMISES=`pwd`/../../cf-promises/cf-promises \
	CF_SERVERD=`pwd`/../../cf-serverd/cf-serverd \
	CF_KEY=`pwd`/../../cf-key/cf-key \
	CF_NET=`pwd`/../../cf-net/cf-net \
	RPMVERCMP=`pwd`/../../ext/rpmvercmp \
	MOCK_PACKAGE_MANAGER=`pwd`/mock_package_manager

if !HAVE_LIBXML2
TESTS_ENVIRONMENT += LIBXML2_TESTS=0
endif

if !HAVE_LIBCURL
TESTS_ENVIRONMENT += LIBCURL_TESTS=0
endif

check-local:
        # Keep the '+' in front for the command, needed for the sub-make
        # to run in parallel; TODO fix "make -n" not working.
	+ MAKE=$(MAKE) ./testall


EXTRA_DIST = default.cf.sub dcs.cf.sub plucked.cf.sub run_with_server.cf.sub \
	testall dummy_etc write_args.sh \
	root-MD5=617eb383deffef843ea856b129d0a423.pub \
	root-MD5=617eb383deffef843ea856b129d0a423.priv

# Recursively include all tests in the dist tarball and set proper permissions
EXTRA_DIST += $(wildcard [0-9]*)

dist-hook:
	chmod -R go-w $(distdir)


CLEANFILES = *.gcno *.gcda Makefile.testall

pluck:
	echo '### This is an auto-generated file, see Makefile.am and `make pluck` ###' > plucked.cf.sub
	../../contrib/cf-locate/cf-locate -f -p '( if_|run_|warn_only|INI_section|kept_successful_command|edit_line|set_user_field|set_variable_values\(|edit_field|set_line_based|location|replace_with|_cp|_dcp|empty|shell|immediate|perms| m\b|recurse|tidy| all|classes_generic|results| start|always|^value|edit_field line|insert_lines|(link|copy)from| file_mustache| (file|dir)_(copy|tidy|sync|make|empty|link|hardlink))' ../../../masterfiles/lib/*.cf >> plucked.cf.sub
