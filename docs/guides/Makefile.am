
all: cf3-solutions.texinfo SpecialTopic_Adoption.pdf  SpecialTopic_ITIL.pdf\
     SpecialTopic_Packages.pdf SpecialTopic_DevOps.pdf SpecialTopic_MissionCritical.pdf\
     SpecialTopic_Reporting.pdf SpecialTopic_BDMA.pdf\
     SpecialTopic_Knowledge.pdf SpecialTopic_Rollback.pdf\
     SpecialTopic_Change.pdf SpecialTopic_Agility.pdf    SpecialTopic_Modules.pdf\
     SpecialTopic_Schedule.pdf  SpecialTopic_Teamwork.pdf SpecialTopic_Scan.pdf\
     SpecialTopic_Monitoring.pdf  cf3-glossary.pdf cf_Quickref3.pdf \
     SpecialTopic_DistributedScheduling.pdf  SpecialTopic_Iteration.pdf\
     SpecialTopic_Hierarchy.pdf SpecialTopic_Federation.pdf    cf3-upgrade.pdf \
     SpecialTopic_ApplMgt.pdf UpgradingCfengineNova.pdf Licensing.pdf OrionCloudPack.pdf\
     SpecialTopic_Virtualization.pdf SpecialTopic_Scalability.pdf\
     SpecialTopic_MenuDrivenConfig.pdf SpecialTopic_FIPS.pdf cf3-tutorial.pdf\
     SpecialTopic_Vision.pdf SpecialTopic_Security.pdf SpecialTopic_Comparison.pdf\
     SpecialTopic_ContentDrivenPolicies.pdf SpecialTopic_Cloud.pdf SpecialTopic_OpenNebula.pdf \
     SpecialTopic_RBAC.pdf SpecialTopic_Windows.pdf\
     cf3-bestpractice.pdf SpecialTopic_Editing.pdf cf3-solutions.pdf cf3-conceptguide.pdf cf3-quickstart.pdf cf3-nova-2-1-supplement.pdf cf3-nova-mission-portal.pdf cf3-constellation-mission-obs.pdf NovaOwnersManual.pdf NovaEvaluationGuide.pdf NovaEvaluationGuideSupplement.pdf


$(builddir)/../tools/build-solutions-guide:
	cd $(builddir)/../tools && $(MAKE) $(AM_MAKEFLAGS) build-solutions-guide

cf3-solutions.texinfo: cf3-solutions.texinfo.in $(builddir)/../tools/build-solutions-guide
	$(AM_V_GEN)$(builddir)/../tools/build-solutions-guide $(top_srcdir)/examples < $< > $@ || rm -f $@

%.pdf : %.texinfo
	texi2pdf $<

