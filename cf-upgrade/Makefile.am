#
#  Copyright 2019 Northern.tech AS
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
noinst_LTLIBRARIES = libcf-upgrade.la

AM_CPPFLAGS = -I$(srcdir)/../libntech/libutils \
	-I$(srcdir)/../libntech/libcompat \
	$(PCRE_CPPFLAGS) \
	$(LIBYAML_CPPFLAGS)

AM_CFLAGS = \
	$(PCRE_CFLAGS) \
	$(LIBYAML_CFLAGS) \
	$(PTHREAD_CFLAGS)

AM_LDFLAGS = \
	$(PCRE_LDFLAGS) \
	$(LIBYAML_LDFLAGS) \
	$(PTHREAD_CFLAGS)

libcf_upgrade_la_LIBADD = $(srcdir)/../libntech/libutils/libutils.la \
	$(PCRE_LIBS) \
	$(LIBYAML_LIBS) \
	$(PTHREAD_LIBS)

libcf_upgrade_la_SOURCES = \
	command_line.c command_line.h \
	configuration.c configuration.h \
	process.c process.h \
	update.c update.h \
	cf-upgrade.c

if !BUILTIN_EXTENSIONS
bin_PROGRAMS = cf-upgrade

# Workaround for automake madness (try removing it if you want to know why).
cf_upgrade_CFLAGS = $(AM_CFLAGS)

# Build both a libcf-upgrade.la library, and a cf-upgrade executable
cf_upgrade_LDADD = libcf-upgrade.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
