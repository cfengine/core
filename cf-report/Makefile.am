AM_CFLAGS = $(NOVA_CFLAGS) @CFLAGS@ -I$(srcdir)/../libpromises

sbin_PROGRAMS = \
        cf-report

cf_report_LDADD =
if HAVE_NOVA
cf_report_LDADD += ../nova/src/libcfreport.la
endif
cf_report_LDADD += ../libpromises/libpromises.la


CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
