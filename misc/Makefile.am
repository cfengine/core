# include unconditionally in the distribution tarball
EXTRA_DIST= init.d/cfengine3.in \
	systemd/cfengine3-web.service.in \
	systemd/cfengine3.service.in


# The following files are generated from the previous ones, during ./configure

if WITH_INIT_D_SCRIPT
initddir = $(INIT_D_PATH)
initd_SCRIPTS = init.d/cfengine3
endif

if WITH_SYSTEMD_SERVICE
systemddir = $(SYSTEMD_SERVICE_PATH)
systemd_DATA   = systemd/cfengine3.service
systemd_DATA  += systemd/cfengine3-web.service
endif
