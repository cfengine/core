AM_CFLAGS = $(GCC_CFLAG) $(NOVA_CFLAGS) @CFLAGS@ -I$(srcdir)/../libpromises

sbin_PROGRAMS = cf-agent

cf_agent_SOURCES = \
        cf-agent.c \
        cf_sql.c cf_sql.h \
        nfs.c nfs.h \
        verify_files.c \
        verify_files_utils.c \
        verify_storage.c \
        verify_exec.c \
        verify_methods.c \
        verify_interfaces.c \
        verify_databases.c \
        verify_outputs.c \
        verify_services.c \
        verify_environments.c verify_environments.h \
        files_edit.c files_edit.h \
        files_editline.c \
        files_editxml.c \
        vercmp_internal.c vercmp_internal.h \
        vercmp.c vercmp.h \
        verify_packages.c \
        agentdiagnostic.c

LDADD =
if HAVE_NOVA
LDADD += ../nova/src/libcfagent.la
endif
LDADD += ../libpromises/libpromises.la \
	$(LIBXML2_LDFLAGS) $(LIBXML2_LIBS) \
	$(LIBVIRT_LDFLAGS) $(LIBVIRT_LIBS) \
	$(POSTGRESQL_LDFLAGS) $(POSTGRESQL_LIBS) \
	$(MYSQL_LDFLAGS) $(MYSQL_LIBS)

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in conf.h.in stamp-h.in
