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
noinst_LTLIBRARIES = libcf-promises.la

AM_CPPFLAGS = \
	-I$(srcdir)/../libntech/libutils \
	-I$(srcdir)/../libpromises \
	-I$(srcdir)/../libcfecompat \
	-I$(srcdir)/../libcfnet \
	@CPPFLAGS@ \
	$(OPENSSL_CPPFLAGS) \
	$(PCRE2_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

AM_LDFLAGS = \
	@LDFLAGS@

libcf_promises_la_LIBADD = ../libpromises/libpromises.la

libcf_promises_la_SOURCES = cf-promises.c

if !BUILTIN_EXTENSIONS
bin_PROGRAMS = cf-promises
cf_promises_LDADD = libcf-promises.la
cf_promises_SOURCES =
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
