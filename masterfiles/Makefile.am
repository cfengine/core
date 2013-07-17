masterfilesdir = $(projdatadir)/CoreBase
dist_masterfiles_DATA = $(srcdir)/promises.cf $(srcdir)/def.cf $(srcdir)/cf-sketch-runfile.cf $(srcdir)/update.cf

#failsafedir = $(masterfilesdir)/failsafe
#dist_failsafe_DATA = $(srcdir)/failsafe/failsafe.cf

updatedir = $(masterfilesdir)/update

librariesdir = $(masterfilesdir)/libraries/
dist_libraries_DATA = $(srcdir)/libraries/cfengine_stdlib.cf

splitlibbasedir = $(masterfilesdir)/lib/
dist_splitlibbase_DATA = $(srcdir)/lib/README.md

splitlib3_5dir = $(masterfilesdir)/lib/3.5/
dist_splitlib3_5_DATA = $(srcdir)/lib/3.5/common.cf \
                        $(srcdir)/lib/3.5/processes.cf \
                        $(srcdir)/lib/3.5/paths.cf \
                        $(srcdir)/lib/3.5/monitor.cf \
                        $(srcdir)/lib/3.5/files.cf \
                        $(srcdir)/lib/3.5/packages.cf \
                        $(srcdir)/lib/3.5/commands.cf \
                        $(srcdir)/lib/3.5/bundles.cf \
                        $(srcdir)/lib/3.5/guest_environments.cf \
                        $(srcdir)/lib/3.5/services.cf \
                        $(srcdir)/lib/3.5/storage.cf \
                        $(srcdir)/lib/3.5/databases.cf

splitlib3_6dir = $(masterfilesdir)/lib/3.6/
dist_splitlib3_6_DATA = $(srcdir)/lib/3.6/common.cf \
                        $(srcdir)/lib/3.6/processes.cf \
                        $(srcdir)/lib/3.6/paths.cf \
                        $(srcdir)/lib/3.6/monitor.cf \
                        $(srcdir)/lib/3.6/files.cf \
                        $(srcdir)/lib/3.6/packages.cf \
                        $(srcdir)/lib/3.6/commands.cf \
                        $(srcdir)/lib/3.6/bundles.cf \
                        $(srcdir)/lib/3.6/guest_environments.cf \
                        $(srcdir)/lib/3.6/services.cf \
                        $(srcdir)/lib/3.6/storage.cf \
                        $(srcdir)/lib/3.6/databases.cf

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
