# Info used in building the MAN files
man_MANS = cf-agent.8 cf-serverd.8 cf-execd.8 cf-monitord.8 cf-report.8 cf-know.8 cf-runagent.8 cf-promises.8 cf-key.8

# Info used in building the Info files
#info_TEXINFOS = cf3-Reference.texinfo

#infofiles = $(info_TEXINFOS:.texinfo=.info)
#MAKEINFOHTML = $(MAKEINFO) --html --no-split

# Info used in building and installing HTML files
#htmlfiles = $(info_TEXINFOS:.texinfo=.html)

#htmldir = @htmldir@
#html_DATA = $(htmlfiles)

# Info used in building and installing Postscript files
#pdffiles = $(info_TEXINFOS:.texinfo=.pdf)
#dvifiles = $(info_TEXINFOS:.texinfo=.dvi)

docdir = @docdir@
doc_DATA = $(psfiles) $(pdffiles)

# Make sure these get distributed with everything else.

EXTRA_DIST = *.8 *.texinfo *.png *.pdf *.html

SUFFIXES = html pdf dvi

%.8 : ../src/%
	../src/$< -M > `echo $<.8 | sed s/src/docs/`

%.html : %.texinfo
	${MAKEINFO} -I $(srcdir) --html --no-split $<

%.dvi : %.texinfo
	${TEXI2DVI} $<

%.pdf : %.texinfo
	${TEXI2PDF} $<

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES = $(htmlfiles) $(psfiles)

#
# Get everything removed down to where rebuilding requires:
# "configure; make; make install"
#
DISTCLEANFILES = *.log *.aux *.mbs *.mb *.info*

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in mdate-sh
