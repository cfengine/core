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
noinst_LTLIBRARIES = libenv.la

libenv_la_SOURCES = \
	constants.c constants.h \
        sysinfo.c sysinfo.h sysinfo_priv.h \
        time_classes.c time_classes.h \
	unix_iface.c unix_iface.h \
        zones.c zones.h

if SOLARIS
libenv_la_LIBADD = -lkstat
endif

AM_CPPFLAGS  = -I$(top_srcdir)/libutils
AM_CPPFLAGS += $(OPENSSL_CPPFLAGS)	     # because libutils needs it
AM_CPPFLAGS += $(PCRE_CPPFLAGS)

# Those dependencies are ought to go away ASAP
AM_CPPFLAGS += -I$(top_srcdir)/libcfnet
AM_CPPFLAGS += -I$(top_srcdir)/libpromises

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = config.h.in

# libcompat dependency

.PRECIOUS: ../libcompat/libcompat.la

../libcompat/libcompat.la:
	$(MAKE) -C ../libcompat $(AM_MAKEFLAGS) libcompat.la
