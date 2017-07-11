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

AM_CPPFLAGS = \
	$(OPENSSL_CPPFLAGS) \
	-I$(srcdir)/../../../libpromises \
	-I$(srcdir)/../../../libutils \
	-I$(srcdir)/../../../libcfnet \
	-I$(srcdir)/../../../libenv \
	-I$(srcdir)/../../../cf-execd \
	$(PCRE_CPPFLAGS) \
	$(ENTERPRISE_CPPFLAGS)

AM_CFLAGS = \
	$(PCRE_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(ENTERPRISE_CFLAGS)

if !NT
noinst_PROGRAMS = cf-execd-test

cf_execd_test_SOURCES = cf-execd-rpl-functions.c
cf_execd_test_LDADD = ../../../cf-execd/libcf-execd-test.la

endif
