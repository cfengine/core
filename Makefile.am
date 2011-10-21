AUTOMAKE_OPTIONS = foreign

SUBDIRS = pub $(NOVA_SUBDIR) $(CONSTELLATION_SUBDIR) src docs examples masterfiles $(CFMOD_SUBDIR)

DIST_SUBDIRS = pub src docs examples masterfiles

# Set explicitly to avoid picking up {nova,constellation,galaxy}/*.m4

DIST_COMMON = README Makefile.am Makefile.in configure AUTHORS aclocal.m4 \
	ChangeLog INSTALL config.guess config.sub depcomp install-sh \
	ltmain.sh missing ylwrap m4/acinclude.m4 m4/libtool.m4 m4/ltoptions.m4 \
	m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4 m4/cf3_with_library.m4 \
	configure.ac autogen.sh

EXTRA_DIST = ChangeLog INSTALL README LICENSE

docdir = @docdir@
doc_DATA = README ChangeLog

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

check:
	@echo "In order to run testsuite, you need to run the following command:"
	@echo "  cd tests; ./testall --agent=\$workdir/bin/cf-agent"
	@echo "where \$workdir is the work directory for CFEngine (/var/cfengine by default)."
	@echo
	@echo "You need to have sudo to run testsuite (in order to test functionality which"
	@echo "is only available to root). Refer to $(srcdir)/tests/README for further info."

dist-hook:
	mkdir $(distdir)/tests
	(cd $(srcdir)/tests && find . -name README -o -name testall -o -name '*.cf*' -a \! -path '*succeeded*' | tar -c -f - -T-) | tar -x -C $(distdir)/tests -v -f -
