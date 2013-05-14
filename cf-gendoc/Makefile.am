noinst_PROGRAMS = cf-gendoc

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(NOVA_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

LDADD = ../libpromises/libpromises.la

cf_gendoc_SOURCES = \
	cf-gendoc.c \
        manual.c manual.h \
        export_xml.c export_xml.h

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
