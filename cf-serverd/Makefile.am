#
#  Copyright 2017 Northern.tech AS
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
noinst_LTLIBRARIES = libcf-serverd.la

AM_CPPFLAGS = -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libenv \
	$(OPENSSL_CPPFLAGS) \
	$(PCRE_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_serverd_la_LIBADD = ../libpromises/libpromises.la

libcf_serverd_la_SOURCES = \
	cf-serverd.c \
	cf-serverd-enterprise-stubs.c cf-serverd-enterprise-stubs.h \
	cf-serverd-functions.c cf-serverd-functions.h \
	server_common.c server_common.h \
	server.c server.h \
	server_transform.c server_transform.h \
	server_classic.c server_classic.h \
	server_tls.c server_tls.h \
	server_access.c server_access.h \
	strlist.c strlist.h

if !BUILTIN_EXTENSIONS
 bin_PROGRAMS = cf-serverd
 # Workaround for automake madness (try removing it if you want to know why).
 cf_serverd_CFLAGS = $(AM_CFLAGS)
 cf_serverd_LDADD = libcf-serverd.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
