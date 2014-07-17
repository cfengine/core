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
