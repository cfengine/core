SUBDIRS = pub $(NOVA_SUBDIR) $(CONSTELLATION_SUBDIR) src docs examples inputs

DIST_SUBDIRS = pub src docs examples inputs

# Set explicitly to avoid picking up {nova,constellation,galaxy}/*.m4

DIST_COMMON = README Makefile.am Makefile.in configure AUTHORS COPYING aclocal.m4 \
	ChangeLog INSTALL NEWS TODO config.guess config.sub depcomp install-sh \
	ltmain.sh missing ylwrap m4/acinclude.m4 m4/libtool.m4 m4/ltoptions.m4 \
	m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4 configure.ac

EXTRA_DIST = COPYING ChangeLog INSTALL NEWS README LICENSE

docdir = @docdir@
doc_DATA = README ChangeLog INSTALL NEWS

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
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess config.sub \
	configure install-sh missing mkinstalldirs depcomp ylwrap \
	ltmain.sh mdate-sh

#
# Pass proper flags to aclocal to pick up Libtool macros
#
ACLOCAL_AMFLAGS = -I m4
