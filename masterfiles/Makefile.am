masterfilesdir = $(projdatadir)/CoreBase
dist_masterfiles_DATA = $(srcdir)/promises.cf $(srcdir)/def.cf $(srcdir)/cf-sketch-runfile.cf $(srcdir)/update.cf

#failsafedir = $(masterfilesdir)/failsafe
#dist_failsafe_DATA = $(srcdir)/failsafe/failsafe.cf

updatedir = $(masterfilesdir)/update

librariesdir = $(masterfilesdir)/libraries/
dist_libraries_DATA = $(srcdir)/libraries/cfengine_stdlib.cf

controlsdir = $(masterfilesdir)/controls
dist_controls_DATA = $(srcdir)/controls/cf_execd.cf  $(srcdir)/controls/cf_agent.cf $(srcdir)/controls/cf_runagent.cf $(srcdir)/controls/cf_monitord.cf $(srcdir)/controls/cf_serverd.cf

servicesdir = $(masterfilesdir)/services
dist_services_DATA = $(srcdir)/services/init_msg.cf
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
