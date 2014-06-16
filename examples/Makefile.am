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
