pkgdata_SCRIPTS =							\
	cf.chflags.example						\
	cf.freebsd.example						\
	cf.ftp.example							\
	cf.groups.example						\
	cf.linux.example						\
	cf.main.example							\
	cf.motd.example							\
	cf.preconf.example						\
	cf.services.example						\
	cf.site.example							\
	cf.solaris.example						\
	cf.sun4.example							\
	cf.users.example						\
	cfservd.conf.example						\
	cfagent.conf.example						\
	cfagent.conf-advanced.example					\
	update.conf.example						\
	cfrc.example							\
	cfrun.hosts.example

EXTRA_DIST = $(pkgdata_SCRIPTS)

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES =

#
# Get everything removed down to where rebuilding requires:
# "configure; make; make install"
#
DISTCLEANFILES = 

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in
