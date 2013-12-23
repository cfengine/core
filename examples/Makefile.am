examplesdir = $(docdir)/examples
dist_examples_DATA = $(srcdir)/*.cf

MAINTAINERCLEANFILES = Makefile.in mdate-sh

# only re-run the outputs for examples that already have an example_output block
remake:
	perl ./remake_outputs.pl $(shell grep -l example_output $(dist_examples_DATA))

prep:
	test -f ~/.cfagent/bin/cf-agent || ln -fs ../cf-agent/cf-agent ~/.cfagent/bin/cf-agent
	test -f ~/.cfagent/bin/cf-promises || ln -fs ../cf-promises/cf-promises ~/.cfagent/bin/cf-promises

recheck: prep
	perl ./remake_outputs.pl -c $(shell grep -l example_output $(dist_examples_DATA))

recheck_verbose: prep
	perl ./remake_outputs.pl -v -c $(shell grep -l example_output $(dist_examples_DATA))
