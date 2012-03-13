TEX_INCLUDEDIR=$(srcdir)/../tex-include

.PRECIOUS: ../../src/cf-know

../../src/cf-know:
	$(MAKE) -C ../../src $(AM_MAKEFLAGS) cf-know

cf3-Reference.texinfo: ../../src/cf-know \
    generate_manual.cf \
    $(filter-out cf3-Reference.texinfo,$(wildcard *.texinfo)) \
    $(wildcard */*.texinfo)
	$(AM_V_GEN)../../src/cf-know -f ./generate_manual.cf

%.html: %.texinfo
	$(AM_V_GEN)$(MAKEINFO) \
	    $^ -o $@ \
	    -I $(TEX_INCLUDEDIR) \
	    --error-limit=0 \
	    --html \
	    --no-split

TEXI2PDFFLAGS = -I $(TEX_INCLUDEDIR) --batch $(if $(filter-out 0,$(V)),,--quiet)

%.pdf: %.texinfo
	$(AM_V_GEN)$(srcdir)/../tools/texi2pdfclean $^ $(TEXI2PDF) -o $@ $(TEXI2PDFFLAGS)



html: cf3-Reference.html

pdf: cf3-Reference.pdf

all:

CLEANFILES = cf3-Reference.html cf3-Reference.pdf cf3-Reference.texinfo

