AM_CFLAGS = $(GCC_CFLAG) $(NOVA_CFLAGS) @CFLAGS@ -I$(srcdir)/../libpromises

LDADD = ../libpromises/libpromises.la

noinst_PROGRAMS = \
        cf-gendoc

cf_gendoc_SOURCES = \
	cf-gendoc.c \
        manual.c manual.h \
        export_xml.c export_xml.h

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
