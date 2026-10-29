#
#  Copyright 2026 Northern.tech AS
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

# The test scripts, with their policy next to them
REACTOR_TEST_SH = $(sort $(wildcard \
	$(srcdir)/tests/*/*.sh \
	$(srcdir)/tests/*/unsafe/*.sh))
REACTOR_TEST_CF = $(REACTOR_TEST_SH:.sh=.cf)

# cf-reactor is not built with builtin extensions (e.g. on Windows)
if !BUILTIN_EXTENSIONS
TESTS = $(patsubst $(srcdir)/%,%,$(REACTOR_TEST_SH))
endif

# Each test is run by run_test.sh, with the binaries of this build. The ones
# in an unsafe/ directory are skipped unless UNSAFE_TESTS=1.
TESTS_ENVIRONMENT = env \
	CF_REACTOR=$(abs_top_builddir)/cf-reactor/cf-reactor \
	CF_PROMISES=$(abs_top_builddir)/cf-promises/cf-promises \
	$(SHELL) $(srcdir)/run_test.sh

EXTRA_DIST = lib.cf lib.sh run.sh run_test.sh \
	$(patsubst $(srcdir)/%,%,$(REACTOR_TEST_SH) $(REACTOR_TEST_CF))
