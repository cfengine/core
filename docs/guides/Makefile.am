TEX_INCLUDEDIR = ../tex-include

# Syntax errors:
#    SpecialTopic_Vision
#    SpecialTopic_Comparison
#    OrionCloudPack

# Missing .png pictures:

COMMON_GUIDES = \
    Licensing \
    NovaEvaluationGuide \
    NovaEvaluationGuideSupplement \
    Enterprise-2-2-OwnersManual \
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
    UpgradingCfengineNova \
    cf3-bestpractice \
    cf3-conceptguide \
    cf3-constellation-mission-obs \
    cf3-glossary \
    cf3-quickstart \
    cf3-solutions \
    cf3-tutorial \
    cf3-upgrade \
    cf_Quickref3

HTML_GUIDES = $(COMMON_GUIDES)

PDF_GUIDES = $(COMMON_GUIDES) \
    cf3-nova-mission-portal \
    cf3-nova-2-1-supplement

.PRECIOUS: ../tools/build-solutions-guide

../tools/build-solutions-guide:
	$(MAKE) -C ../tools $(AM_MAKEFLAGS) build-solutions-guide

cf3-solutions.texinfo: cf3-solutions.texinfo.in ../tools/build-solutions-guide
	$(AM_V_GEN)../tools/build-solutions-guide $(top_srcdir)/examples < $< > $@ || (rm -f $@; false)


%.html: %.texinfo
	$(AM_V_GEN)$(MAKEINFO) \
	    $^ -o $@ \
	    -I $(TEX_INCLUDEDIR) \
	    --error-limit=0 \
	    --html \
	    --no-split \
	    --css-include=cfcomdoc.css

TEXI2PDFFLAGS = -I $(TEX_INCLUDEDIR) --batch $(if $(filter-out 0,$(V)),,--quiet)

%.pdf: %.texinfo
	$(AM_V_GEN)$(srcdir)/../tools/texi2pdfclean $< $(TEXI2PDF) -o $@ $(TEXI2PDFFLAGS)


html: $(patsubst %,%.html,$(HTML_GUIDES))

pdf: $(patsubst %,%.pdf,$(PDF_GUIDES))

all:

CLEANFILES = cf3-solutions.texinfo $(patsubst %,%.pdf,$(PDF_GUIDES)) $(patsubst %,%.html,$(HTML_GUIDES))
