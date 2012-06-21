SUBDIRS = manpages tools guides reference tex-include

html:
	$(MAKE) -C guides $(AM_MAKEFLAGS) html
	$(MAKE) -C reference $(AM_MAKEFLAGS) html

pdf:
	$(MAKE) -C guides $(AM_MAKEFLAGS) pdf
	$(MAKE) -C reference $(AM_MAKEFLAGS) pdf
