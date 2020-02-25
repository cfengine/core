#
#  Copyright 2020 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#

########## Sources pre-generated and distributed in the tarball ##########
# Make sure you include the generated file as a dependency to a target
# (e.g. libpromises_la) so that it's generated during "make dist"

BUILT_SOURCES = enterprise_extension.c enterprise_extension.h
EXTRA_SCRIPTS = enterprise_extension.sed
EXTRA_DIST = $(EXTRA_SCRIPTS) $(BUILT_SOURCES)

enterprise_extension.c: extensions_template.c.pre enterprise_extension.sed
	$(V_SED) $(SED) -f $(srcdir)/enterprise_extension.sed $< > $@
enterprise_extension.h: extensions_template.h.pre enterprise_extension.sed
	$(V_SED) $(SED) -f $(srcdir)/enterprise_extension.sed $< > $@

CLEANFILES = enterprise_extension.c enterprise_extension.h
