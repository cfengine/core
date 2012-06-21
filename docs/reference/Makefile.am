TEX_INCLUDEDIR=$(srcdir)/../tex-include

.PRECIOUS: ../../src/cf-know

../../src/cf-know:
	$(MAKE) -C ../../src $(AM_MAKEFLAGS) cf-know

cf3-Reference.texinfo: ../../src/cf-know \
    generate_manual.cf \
    $(filter-out cf3-Reference.texinfo,$(wildcard *.texinfo)) \
    $(wildcard */*.texinfo)
	chmod g-w generate_manual.cf
	$(AM_V_GEN)../../src/cf-know -f ./generate_manual.cf

%.html: %.texinfo
	$(AM_V_GEN)$(MAKEINFO) \
	    $^ -o $@ \
	    -I $(TEX_INCLUDEDIR) \
	    --error-limit=0 \
	    --html \
        --css-include=../guides/cfcomdoc.css \
	    --no-split

TEXI2PDFFLAGS = -I $(TEX_INCLUDEDIR) --batch $(if $(filter-out 0,$(V)),,--quiet)

%.pdf: %.texinfo
	$(AM_V_GEN)$(srcdir)/../tools/texi2pdfclean $^ $(TEXI2PDF) -o $@ $(TEXI2PDFFLAGS)



html: cf3-Reference.html

pdf: cf3-Reference.pdf

all:

install: install-html install-pdf

REFDIR=$(DESTDIR)/$(projdocdir)/reference

dist-reference-%: %
	$(MKDIR_P) $(distdir)/$^
	$(INSTALL_DATA) $(subst *,\*,$(wildcard $(srcdir)/$^/*.texinfo)) $(distdir)/$^

dist-reference: $(addprefix dist-reference-,bodyparts bundletypes control functions promises varcontexts vars)
	$(INSTALL_DATA) $(filter-out cf3-Reference.texinfo,$(wildcard $(srcdir)/*.texinfo)) $(distdir)

install-html: html
	$(MKDIR_P) $(REFDIR)/html
	$(INSTALL_DATA) cf3-Reference.html $(REFDIR)/html
	$(INSTALL_DATA) `$(srcdir)/../tools/extract-images cf3-Reference.texinfo | sort | uniq` $(REFDIR)/html

install-pdf: pdf
	$(MKDIR_P) $(REFDIR)/pdf
	$(INSTALL_DATA) cf3-Reference.pdf $(REFDIR)/pdf

if PDF_DOCS
all: pdf
install-data-hook: install-pdf
dist-hook: dist-reference
endif

if HTML_DOCS
all: html
install-data-hook: install-html
dist-hook: dist-reference
endif

EXTRA_DIST = CFEngineFrontPage.pdf NewLogo.pdf filelogic.png generate_manual.cf

CLEANFILES = cf3-Reference.html cf3-Reference.pdf cf3-Reference.texinfo
