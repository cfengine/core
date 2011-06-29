
all: cf3-Reference.pdf cf3-Reference.texinfo # cfnova.pdf Cfengine3-Solutions.pdf

%.pdf : %.texinfo
	texi2pdf $<

cf3-Reference.texinfo: bodypart* var* func* promise* control* /usr/local/sbin/cf-know
	../../src/cf-know -f ./generate_manual.cf