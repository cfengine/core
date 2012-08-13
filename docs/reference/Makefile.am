TEX_INCLUDEDIR=$(srcdir)/../tex-include

.PRECIOUS: ../../src/cf-gendoc

../../src/cf-gendoc:
	$(MAKE) -C ../../src $(AM_MAKEFLAGS) cf-gendoc

cf3-Reference.texinfo: ../../src/cf-gendoc \
    generate_manual.cf \
    $(filter-out cf3-Reference.texinfo,$(wildcard *.texinfo)) \
    $(wildcard */*.texinfo)
	$(AM_V_GEN)../../src/cf-gendoc -i . -o $@

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


if HTML_DOCS
html: cf3-Reference.html
endif

if PDF_DOCS
pdf: cf3-Reference.pdf
endif

REFDIR=$(DESTDIR)/$(projdocdir)/reference

dist-reference-%: %
	$(MKDIR_P) $(distdir)/$^
	$(INSTALL_DATA) $(subst *,\*,$(wildcard $(srcdir)/$^/*.texinfo)) $(distdir)/$^

dist-reference: $(addprefix dist-reference-,bodyparts bundletypes control functions promises varcontexts vars)
	$(INSTALL_DATA) $(filter-out cf3-Reference.texinfo,$(wildcard $(srcdir)/*.texinfo)) $(distdir)

if HTML_DOCS
install-html: html
	$(MKDIR_P) $(REFDIR)/html
	$(INSTALL_DATA) cf3-Reference.html $(REFDIR)/html
	$(INSTALL_DATA) `$(srcdir)/../tools/extract-images cf3-Reference.texinfo | sort | uniq` $(REFDIR)/html
endif

if PDF_DOCS
install-pdf: pdf
	$(MKDIR_P) $(REFDIR)/pdf
	$(INSTALL_DATA) cf3-Reference.pdf $(REFDIR)/pdf
endif

all: html pdf
install-data-hook: install-html install-pdf
dist-hook: dist-reference

EXTRA_DIST = CFEngineFrontPage.pdf NewLogo.pdf filelogic.png generate_manual.cf

CLEANFILES = cf3-Reference.html cf3-Reference.pdf cf3-Reference.texinfo
