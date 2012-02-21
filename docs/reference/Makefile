TEX_INCLUDEDIR = ../tex-include
CFKNOW=../../src/cf-know

all: cf3-Reference.pdf cf3-Reference.texinfo # cfnova.pdf Cfengine3-Solutions.pdf

%.pdf : %.texinfo
	texi2pdf -I $(TEX_INCLUDEDIR) $<

cf3-Reference.texinfo: bodypart* var* func* promise* control* $(CFKNOW)
	$(CFKNOW) -f ./generate_manual.cf
