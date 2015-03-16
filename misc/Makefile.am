if WITH_INIT_D_SCRIPT
initddir = $(INIT_D_PATH)
dist_initd_SCRIPTS = init.d/cfengine3
endif

if WITH_SYSTEMD_SERVICE
systemddir = $(SYSTEMD_SERVICE_PATH)
dist_systemd_DATA = systemd/cfengine3.service \
	systemd/cfengine3-web.service
endif
