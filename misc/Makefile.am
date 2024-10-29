SUBDIRS = selinux

# include unconditionally in the distribution tarball
EXTRA_DIST= init.d/cfengine3.in \
	systemd/cf-php-fpm.service.in \
	systemd/cf-apache.service.in \
	systemd/cf-execd.service.in \
	systemd/cf-hub.service.in \
	systemd/cf-monitord.service.in \
	systemd/cf-postgres.service.in \
	systemd/cf-serverd.service.in \
	systemd/cfengine3.service.in


# The following files are generated from the previous ones, during ./configure

if WITH_INIT_D_SCRIPT
initddir = $(INIT_D_PATH)
initd_SCRIPTS = init.d/cfengine3
endif

if WITH_SYSTEMD_SERVICE
systemddir = $(SYSTEMD_SERVICE_PATH)
systemd_DATA   = systemd/cfengine3.service
systemd_DATA  += systemd/cf-php-fpm.service
systemd_DATA  += systemd/cf-apache.service
systemd_DATA  += systemd/cf-execd.service
systemd_DATA  += systemd/cf-hub.service
systemd_DATA  += systemd/cf-reactor.service
systemd_DATA  += systemd/cf-monitord.service
systemd_DATA  += systemd/cf-postgres.service
systemd_DATA  += systemd/cf-serverd.service

install-data-hook:
	-systemctl --no-ask-password daemon-reload

endif
dist_bin_SCRIPTS = cf-support

check-local:
	@if command -v shellcheck 2>/dev/null; then \
	  shellcheck --version; \
	  shellcheck cf-support; \
	else \
	  echo "Install shellcheck to check cf-support script."; \
	fi
