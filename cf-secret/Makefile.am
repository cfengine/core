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
noinst_LTLIBRARIES = libcf-secret.la

AM_CPPFLAGS = -I$(srcdir)/../libpromises -I$(srcdir)/../libntech/libutils \
	-I$(srcdir)/../libcfecompat \
	-I$(srcdir)/../libcfnet \
	$(OPENSSL_CPPFLAGS) \
	$(PCRE2_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

libcf_secret_la_LIBADD = ../libpromises/libpromises.la

libcf_secret_la_SOURCES = cf-secret.c

if !BUILTIN_EXTENSIONS
 bin_PROGRAMS = cf-secret
 cf_secret_LDADD = libcf-secret.la
 cf_secret_SOURCES =
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
