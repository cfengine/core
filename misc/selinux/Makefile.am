if WITH_SELINUX_POLICY
cfengine-enterprise.te: cfengine-enterprise.te.all $(PLATFORM_SELINUX_POLICIES)
	cat cfengine-enterprise.te.all $(PLATFORM_SELINUX_POLICIES) > cfengine-enterprise.te

cfengine-enterprise.pp cfengine-enterprise-unconfined.pp: cfengine-enterprise.te cfengine-enterprise.fc cfengine-enterprise-unconfined.te cfengine-enterprise-unconfined.fc
	rpm -qa selinux-policy # debug output to see version
	m4 --version
	autoconf --version
	automake --version
	sha256sum *
	$(MAKE) -f /usr/share/selinux/devel/Makefile -j1 2>&1 | tee log
	sudo dnf install -y ed
	[ "$?" != "0" ] && (
	  for line in $(grep m4: log | cut -d: -f3 | sort -u); do echo "$line-2;$line+2n" | ed -s tmp/all_interfaces.conf; done
	)

selinuxdir = $(prefix)/selinux
selinux_DATA = cfengine-enterprise.pp
selinux_DATA += cfengine-enterprise.te
selinux_DATA += cfengine-enterprise.fc
selinux_DATA += cfengine-enterprise-unconfined.pp
selinux_DATA += cfengine-enterprise-unconfined.te
selinux_DATA += cfengine-enterprise-unconfined.fc

clean-local:
	rm -rf tmp
endif

# explicit DISTFILES are required for these files to be part of a 'make dist'
# tarball even without running './configure --with-selinux-policy'
DISTFILES  = Makefile.in Makefile.am cfengine-enterprise.fc cfengine-enterprise.te.all
DISTFILES += cfengine-enterprise.te.el9
DISTFILES += cfengine-enterprise-unconfined.te
DISTFILES += cfengine-enterprise-unconfined.fc

CLEANFILES = cfengine-enterprise.pp cfengine-enterprise.if cfengine-enterprise.te cfengine-enterprise-unconfined.pp cfengine-enterprise-unconfined.if
