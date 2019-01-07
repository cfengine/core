#
#  Copyright 2018 Northern.tech AS
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
noinst_LTLIBRARIES = libcf-agent.la

AM_CPPFLAGS = -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../cf-check \
	$(ENTERPRISE_CPPFLAGS) \
	$(OPENSSL_CPPFLAGS) \
	$(PCRE_CPPFLAGS) \
	$(LIBVIRT_CPPFLAGS) \
	$(POSTGRESQL_CPPFLAGS) \
	$(MYSQL_CPPFLAGS) \
	$(LIBXML2_CPPFLAGS) \
	$(PAM_CPPFLAGS)

AM_CFLAGS = \
	$(ENTERPRISE_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(PCRE_CFLAGS) \
	$(LIBVIRT_CFLAGS) \
	$(POSTGRESQL_CFLAGS) \
	$(MYSQL_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	$(PAM_CFLAGS)

AM_LDFLAGS = \
	$(OPENSSL_LDFLAGS) \
	$(PCRE_LDFLAGS) \
	$(LIBVIRT_LDFLAGS) \
	$(POSTGRESQL_LDFLAGS) \
	$(MYSQL_LDFLAGS) \
	$(LIBXML2_LDFLAGS) \
	$(PAM_LDFLAGS)

libcf_agent_la_LIBADD = ../libpromises/libpromises.la \
	$(OPENSSL_LIBS) \
	$(PCRE_LIBS) \
	$(LIBVIRT_LIBS) \
	$(POSTGRESQL_LIBS) \
	$(MYSQL_LIBS) \
	$(LIBXML2_LIBS) \
	$(PAM_LIBS)

libcf_agent_la_SOURCES = \
        agent-diagnostics.c agent-diagnostics.h \
        tokyo_check.c tokyo_check.h \
        abstract_dir.c abstract_dir.h \
        cf-agent.c \
	cf-agent-enterprise-stubs.c cf-agent-enterprise-stubs.h \
        comparray.c comparray.h \
        acl_posix.c acl_posix.h \
        cf_sql.c cf_sql.h \
	files_changes.c files_changes.h \
        promiser_regex_resolver.c promiser_regex_resolver.h \
        retcode.c retcode.h \
        verify_acl.c verify_acl.h \
        verify_files.c verify_files.h \
        verify_files_utils.c verify_files_utils.h \
        verify_files_hashes.c verify_files_hashes.h \
        verify_storage.c verify_storage.h \
        verify_exec.c verify_exec.h \
        verify_methods.c verify_methods.h \
        verify_databases.c verify_databases.h \
        verify_processes.c verify_processes.h \
        verify_services.c verify_services.h \
        verify_environments.c verify_environments.h \
        files_edit.c files_edit.h \
        files_editline.c files_editline.h \
        files_editxml.c files_editxml.h \
        files_links.c files_links.h \
        files_operators.c files_operators.h \
        files_properties.c files_properties.h \
        files_repository.c files_repository.h \
        files_select.c files_select.h \
        vercmp_internal.c vercmp_internal.h \
        vercmp.c vercmp.h \
        package_module.c package_module.h \
        verify_packages.c verify_packages.h \
        verify_new_packages.c verify_new_packages.h \
        verify_users.c verify_users.h \
        cf-agent-windows-functions.h

if !NT
libcf_agent_la_SOURCES += nfs.c nfs.h

if HAVE_USERS_PROMISE_DEPS
  libcf_agent_la_SOURCES += verify_users_pam.c
else
  libcf_agent_la_SOURCES += verify_users_stub.c
endif
endif

if HAVE_AVAHI_CLIENT
if HAVE_AVAHI_COMMON
libcf_agent_la_SOURCES += \
		findhub.c findhub_priv.h findhub.h \
		load_avahi.c load_avahi.h
endif
endif

if !BUILTIN_EXTENSIONS
  bin_PROGRAMS = cf-agent

  # Workaround for automake madness (try removing it if you want to know why).
  cf_agent_CFLAGS = $(AM_CFLAGS)

  # Build both a libcf-agent.la library, and a cf-agent executable
  cf_agent_LDADD = libcf-agent.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
