SUBDIRS = selinux

# include unconditionally in the distribution tarball
EXTRA_DIST= init.d/cfengine3.in \
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
# cf-support is POSIX sh; Windows gets the PowerShell port and a .cmd wrapper.
# All three are distributed regardless of build host: the dist tarball is
# produced on Unix and consumed by the Windows packaging.
if WINDOWS
bin_SCRIPTS = cf-support.ps1 cf-support.cmd
else
bin_SCRIPTS = cf-support
endif

EXTRA_DIST += cf-support cf-support.ps1 cf-support.cmd

check-local:
	@if command -v shellcheck 2>/dev/null; then \
	  shellcheck --version; \
	  shellcheck cf-support; \
	else \
	  echo "Install shellcheck to check cf-support script."; \
	fi
