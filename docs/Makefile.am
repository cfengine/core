
if !CROSS_COMPILING
man_MANS = cf-agent.8 cf-serverd.8 cf-execd.8 cf-monitord.8 cf-report.8 cf-know.8 cf-runagent.8 cf-promises.8 cf-key.8

if HAVE_NOVA
  man_MANS += cf-hub.8
endif
endif

# Make sure these get distributed with everything else.

EXTRA_DIST = $(man_MANS) *.png *.html

%.8 : ../src/%
	../src/$< -M > `echo $<.8 | sed s/src/docs/`

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES = $(man_MANS)

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
