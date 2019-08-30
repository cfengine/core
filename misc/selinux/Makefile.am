if WITH_SELINUX
cfengine-enterprise.pp: cfengine-enterprise.te cfengine-enterprise.fc
	$(MAKE) -f /usr/share/selinux/devel/Makefile -j1

selinuxdir = $(prefix)/selinux
selinux_DATA = cfengine-enterprise.pp

clean-local:
	rm -rf tmp
endif

# explicit DISTFILES are required for these files to be part of a 'make dist'
# tarball even without running './configure --with-selinux-policy'
DISTFILES  = Makefile.in Makefile.am cfengine-enterprise.te cfengine-enterprise.fc

CLEANFILES = cfengine-enterprise.pp cfengine-enterprise.if
