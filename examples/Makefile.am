#
#  Copyright 2017 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by CFEngine AS.
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
examplesdir = $(docdir)/examples
dist_examples_DATA = $(srcdir)/*.cf
dist_examples_SCRIPTS = remake_outputs.pl
FAKE_WORKDIR=/tmp/fake-cfengine-workdir
MAINTAINERCLEANFILES = Makefile.in mdate-sh

# only re-run the outputs for examples that already have an example_output block
remake:
	perl ./remake_outputs.pl $(shell grep -l example_output $(dist_examples_DATA))

recheck:
	perl ./remake_outputs.pl -c $(shell grep -l example_output $(dist_examples_DATA))

recheck_verbose:
	perl ./remake_outputs.pl -v -c $(shell grep -l example_output $(dist_examples_DATA))
