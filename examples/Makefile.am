examplesdir = $(docdir)/examples
dist_examples_DATA = $(srcdir)/*.cf

MAINTAINERCLEANFILES = Makefile.in mdate-sh

# only re-run the outputs for examples that already have an example_output block
remake:
	perl ./remake_outputs.pl $(shell grep -l example_output $(dist_examples_DATA))

check:
	perl ./remake_outputs.pl -c $(shell grep -l example_output $(dist_examples_DATA))
