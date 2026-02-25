if WITH_SELINUX
cfengine-enterprise.te: cfengine-enterprise.te.all $(PLATFORM_SELINUX_POLICIES)
	cat cfengine-enterprise.te.all $(PLATFORM_SELINUX_POLICIES) > cfengine-enterprise.te

cfengine-enterprise.pp: cfengine-enterprise.te cfengine-enterprise.fc
	$(MAKE) -f /usr/share/selinux/devel/Makefile -j1

selinuxdir = $(prefix)/selinux
selinux_DATA = cfengine-enterprise.pp
selinux_DATA += cfengine-enterprise.te
selinux_DATA += cfengine-enterprise.fc

clean-local:
	rm -rf tmp
endif

# explicit DISTFILES are required for these files to be part of a 'make dist'
# tarball even without running './configure --with-selinux-policy'
DISTFILES  = Makefile.in Makefile.am cfengine-enterprise.fc cfengine-enterprise.te.all
DISTFILES += cfengine-enterprise.te.el9
DISTFILES += cfengine-enterprise.te.el10

CLEANFILES = cfengine-enterprise.pp cfengine-enterprise.if cfengine-enterprise.te
