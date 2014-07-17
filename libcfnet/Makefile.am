noinst_LTLIBRARIES = libcfnet.la

AM_CPPFLAGS  = $(OPENSSL_CPPFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/libutils			    # platform.h
AM_CPPFLAGS += -I$(top_srcdir)/libpromises		    # cf3.defs.h

libcfnet_la_SOURCES = \
	misc.c \
	net.c net.h \
	communication.c communication.h \
	client_protocol.c client_protocol.h cfnet.h\
	client_code.c client_code.h \
	classic.c classic.h \
	tls_client.c tls_client.h \
	tls_generic.c tls_generic.h \
	connection_info.c connection_info.h \
	key.c key.h
