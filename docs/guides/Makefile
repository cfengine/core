
all: SpecialTopic_Adoption.pdf  SpecialTopic_ITIL.pdf\
     SpecialTopic_Reporting.pdf SpecialTopic_BDMA.pdf\
     SpecialTopic_Knowledge.pdf SpecialTopic_Rollback.pdf\
     SpecialTopic_Change.pdf    SpecialTopic_Modules.pdf\
     SpecialTopic_Schedule.pdf  SpecialTopic_Teamwork.pdf\
     SpecialTopic_Monitoring.pdf  cf3-glossary.pdf cf_Quickref3.pdf \
     SpecialTopic_DistributedScheduling.pdf  SpecialTopic_Iteration.pdf\
     SpecialTopic_Hierarchy.pdf SpecialTopic_Federation.pdf    cf3-upgrade.pdf \
     SpecialTopic_ApplMgt.pdf UpgradingCfengineNova.pdf Licensing.pdf OrionCloudPack.pdf\
     SpecialTopic_Virtualization.pdf SpecialTopic_Scalability.pdf\
     SpecialTopic_MenuDrivenConfig.pdf SpecialTopic_FIPS.pdf cf3-tutorial.pdf\
     SpecialTopic_Vision.pdf SpecialTopic_Security.pdf SpecialTopic_Comparison.pdf\
     SpecialTopic_ContentDrivenPolicies.pdf SpecialTopic_Cloud.pdf SpecialTopic_OpenNebula.pdf SpecialTopic_RBAC.pdf\
     NovaGetStarted.pdf cf3-bestpractice.pdf SpecialTopic_Editing.pdf cfnova.pdf cf3-solutions.pdf

cfengine-Reference.html: /home/mark/LapTop/GNU-cfengine/trunk/doc/cfengine-Reference.texinfo
	makeinfo  --html --no-split --css-include=cfcomdoc.css /home/mark/LapTop/GNU-cfengine/trunk/doc/cfengine-Reference.texinfo

cf3-tutorial.html: cf3-tutorial.texinfo
	makeinfo  --html --no-split --css-include=cfcomdoc.css cf3-tutorial.texinfo

cfengine-Anomaly.html: /home/mark/LapTop/GNU-cfengine/trunk/doc/cfengine-Anomaly.texinfo
	makeinfo  --html --no-split --css-include=cfcomdoc.css /home/mark/LapTop/GNU-cfengine/trunk/doc/cfengine-Anomaly.texinfo

cf_Modularize.html: /home/mark/LapTop/CfengineProjects/Training/cf-Modularize.texinfo
	makeinfo  --html --no-split --css-include=cfcomdoc.css /home/mark/LapTop/CfengineProjects/Training/cf-Modularize.texinfo

%.pdf : %.texinfo
	texi2pdf $<
