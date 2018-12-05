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
noinst_LTLIBRARIES = libcf-check.la

AM_CPPFLAGS = -I$(srcdir)/../libutils \
	$(LMDB_CPPFLAGS)

AM_CFLAGS = \
	$(LMDB_CFLAGS) \
	$(PTHREAD_CFLAGS)

AM_LDFLAGS = \
	$(LMDB_LDFLAGS)

libcf_check_la_LIBADD = ../libutils/libutils.la \
	$(LMDB_LIBS) \
	$(PTHREAD_LIBS)

libcf_check_la_SOURCES = \
	backup.c backup.h \
	cf-check.c \
	diagnose.c diagnose.h \
	lmdump.c lmdump.h

if !BUILTIN_EXTENSIONS
  bin_PROGRAMS = cf-check

  # Workaround for automake madness (try removing it if you want to know why).
  cf_check_CFLAGS = $(AM_CFLAGS)

  # Build both a libcf-check.la library, and a cf-check executable
  cf_check_LDADD = libcf-check.la
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
