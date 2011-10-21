
all: build_solutions_guide cf3-solutions.texinfo SpecialTopic_Adoption.pdf  SpecialTopic_ITIL.pdf\
     SpecialTopic_Packages.pdf SpecialTopic_DevOps.pdf SpecialTopic_MissionCritical.pdf\
     SpecialTopic_Reporting.pdf SpecialTopic_BDMA.pdf\
     SpecialTopic_Knowledge.pdf SpecialTopic_Rollback.pdf\
     SpecialTopic_Change.pdf    SpecialTopic_Modules.pdf\
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
     cf3-bestpractice.pdf SpecialTopic_Editing.pdf cfnova.pdf cf3-solutions.pdf cf3-conceptguide.pdf cf3-quickstart.pdf cf3-nova-2-1-supplement.pdf cf3-nova-mission-portal.pdf cf3-constellation-mission-obs.pdf



cf3-solutions.texinfo: cf3-solutions.in
	./build_solutions_guide

%.pdf : %.texinfo
	texi2pdf $<

build_solutions_guide: build_solutions_guide.c
	gcc -o build_solutions_guide build_solutions_guide.c
