TEX_INCLUDEDIR = ../tex-include

# Syntax errors:
#    SpecialTopic_Vision
#    SpecialTopic_Comparison
#    OrionCloudPack

COMMON_GUIDES = \
    SpecialTopic_Adoption \
    SpecialTopic_Agility \
    SpecialTopic_ApplMgt \
    SpecialTopic_BDMA \
    SpecialTopic_Cloud \
    SpecialTopic_Change \
    SpecialTopic_ContentDrivenPolicies \
    SpecialTopic_DevOps \
    SpecialTopic_DistributedScheduling \
    SpecialTopic_Editing \
    SpecialTopic_Federation \
    SpecialTopic_FIPS \
    SpecialTopic_Hierarchy \
    SpecialTopic_ITIL \
    SpecialTopic_Iteration \
    SpecialTopic_Knowledge \
    SpecialTopic_MenuDrivenConfig \
    SpecialTopic_MissionCritical \
    SpecialTopic_Modules \
    SpecialTopic_Monitoring \
    SpecialTopic_OpenNebula \
    SpecialTopic_Packages \
    SpecialTopic_RBAC \
    SpecialTopic_Reporting \
    SpecialTopic_Rollback \
    SpecialTopic_Scalability \
    SpecialTopic_Scan \
    SpecialTopic_Schedule \
    SpecialTopic_Security \
    SpecialTopic_Teamwork \
    SpecialTopic_Virtualization \
    SpecialTopic_Windows \
    cf3-bestpractice \
    cf3-conceptguide \
    cf3-glossary \
    cf3-quickstart \
    cf3-solutions \
    cf3-tutorial \
    cf3-upgrade \
    cf_Quickref3 \
    CfengineStdLibrary

HTML_ONLY_GUIDES =

PDF_ONLY_GUIDES =

if HAVE_NOVA
-include ../../nova/docs/Makefile.am
endif

HTML_GUIDES = $(COMMON_GUIDES) $(HTML_ONLY_GUIDES)

PDF_GUIDES = $(COMMON_GUIDES) $(PDF_ONLY_GUIDES)

.PRECIOUS: ../tools/build-solutions-guide

../tools/build-solutions-guide:
	$(MAKE) -C ../tools $(AM_MAKEFLAGS) build-solutions-guide

../tools/build-stdlib:
	$(MAKE) -C ../tools $(AM_MAKEFLAGS) build-stdlib

cf3-solutions.texinfo: cf3-solutions.texinfo.in ../tools/build-solutions-guide
	$(AM_V_GEN)../tools/build-solutions-guide $(top_srcdir)/examples < $< > $@ || (rm -f $@; false)

CfengineStdLibrary.texinfo: ../../masterfiles/libraries/cfengine_stdlib.cf ../tools/build-stdlib
	$(AM_V_GEN)../tools/build-stdlib $(srcdir)/../../masterfiles/libraries/cfengine_stdlib.cf || (rm -f $@; false)

%.html: %.texinfo
	$(AM_V_GEN)$(MAKEINFO) \
	    $^ -o $@ \
	    -I $(TEX_INCLUDEDIR) \
	    --error-limit=0 \
	    --html \
	    --no-split \
	    --no-validate \
	    --css-include=cfcomdoc.css

TEXI2PDFFLAGS = -I $(TEX_INCLUDEDIR) --batch $(if $(filter-out 0,$(V)),,--quiet)

%.pdf: %.texinfo
	$(AM_V_GEN)$(srcdir)/../tools/texi2pdfclean $< $(TEXI2PDF) -o $@ $(TEXI2PDFFLAGS)


if HTML_DOCS
html: $(patsubst %,%.html,$(HTML_GUIDES))
endif

if PDF_DOCS
pdf: $(patsubst %,%.pdf,$(PDF_GUIDES))
endif

GUIDEDIR=$(DESTDIR)/$(projdocdir)/guides

dist-guide-%: %
	$(INSTALL_DATA) $^ $(distdir)
	if test -n "`$(srcdir)/../tools/extract-images $^ | sort | uniq`"; then \
		$(INSTALL_DATA) `$(srcdir)/../tools/extract-images $^ | sort | uniq` $(distdir); \
	fi

dist-common: $(patsubst %,dist-guide-%.texinfo,$(COMMON_GUIDES))

if HTML_DOCS
install-html: html
	$(MKDIR_P) $(GUIDEDIR)/html
	$(INSTALL_DATA) $(patsubst %,%.html,$(HTML_GUIDES)) $(GUIDEDIR)/html
	$(INSTALL_DATA) `$(srcdir)/../tools/extract-images $(patsubst %,%.texinfo,$(HTML_GUIDES)) | sort | uniq` $(GUIDEDIR)/html
endif

dist-html-only:

if PDF_DOCS
install-pdf: pdf
	$(MKDIR_P) $(GUIDEDIR)/pdf
	$(INSTALL_DATA) $(patsubst %,%.pdf,$(PDF_GUIDES)) $(GUIDEDIR)/pdf
endif

if PDF_DOCS
dist-pdf-only: $(patsubst %,dist-guide-%.texinfo,$(PDF_ONLY_GUIDES))
else
dist-pdf-only:
endif

all: pdf html
install-data-hook: install-pdf install-html
dist-hook: dist-pdf-only dist-html-only dist-common

EXTRA_DIST = cf3-solutions.texinfo.in CFEngineFrontPage.pdf NewLogo.pdf cfcomdoc.css

CLEANFILES = cf3-solutions.texinfo $(patsubst %,%.pdf,$(PDF_GUIDES)) $(patsubst %,%.html,$(HTML_GUIDES))
