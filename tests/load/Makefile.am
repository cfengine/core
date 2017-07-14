#
#  Copyright 2017 Northern.tech AS
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
# automake does not support "maude_LIBS" variables. We can only alter
# the generic LIBS one. In case the functions are mocked in the test
# implementation, then we are pretty sure that they will be overriden by
# our local implementation. So we include *everything*...
LIBS = $(CORE_LIBS)
AM_LDFLAGS = $(CORE_LDFLAGS)

# Those tests use stub functions interposition which does not work (yet)
# under OS X. Another way of stubbing functions from libpromises is needed.
if !XNU
AM_CPPFLAGS = $(CORE_CPPFLAGS) \
	$(ENTERPRISE_CFLAGS) \
	-I../../libpromises \
	-I../../libutils \
	-I../../libcfnet \
	-I../../libpromises

EXTRA_DIST = \
	run_db_load.sh \
	run_lastseen_threaded_load.sh

TESTS = \
	run_db_load.sh \
	run_lastseen_threaded_load.sh

check_PROGRAMS = db_load lastseen_load lastseen_threaded_load


db_load_SOURCES = db_load.c
db_load_LDADD = ../unit/libdb.la


lastseen_load_SOURCES = lastseen_load.c \
	$(srcdir)/../../libpromises/lastseen.c \
	$(srcdir)/../../libutils/statistics.c
lastseen_load_LDADD = ../unit/libdb.la ../../libpromises/libpromises.la
endif

lastseen_threaded_load_LDADD = ../unit/libtest.la \
	../../libpromises/libpromises.la
