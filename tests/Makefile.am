# Info used in building the MAN files
man_MANS = 

# Info used in building the Info files
info_TEXINFOS = 
infofiles = $(info_TEXINFOS:.texinfo=.info)
MAKEINFOHTML = $(MAKEINFO) --html --no-split

# Info used in building and installing HTML files
htmlfiles = $(info_TEXINFOS:.texinfo=.html)

htmldir = $(pkgdatadir)/html
html_DATA = $(htmlfiles)

# Info used in building and installing Postscript files
psfiles = $(info_TEXINFOS:.texinfo=.ps)
pdffiles = $(info_TEXINFOS:.texinfo=.pdf)
dvifiles = $(info_TEXINFOS:.texinfo=.dvi)

docdir = $(pkgdatadir)/doc
doc_DATA = $(psfiles) $(pdffiles)

# Make sure these get distributed with everything else.

EXTRA_DIST = mod_common.cf mod_exec.cf mod_process.cf mod_routing.cf mod_map.cf mod_files_copy.cf mod_files.cf mod_storage.cf promises.cf mod_monitor.cf mod_access.cf

SUFFIXES = html latex pdf ps dvi

%.html : %.texinfo
	${MAKEINFO} -I $(srcdir) --html --no-split $<

%.dvi : %.latex
	${LATEX} $<

%.dvi : %.texinfo
	${TEXI2DVI} $<

%.pdf : %.ps
	${PS2PDF} $<

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
# 
CLEANFILES = $(htmlfiles) $(psfiles) $(pdffiles)

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
