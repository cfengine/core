sbin_PROGRAMS = cf-agent

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libpromises -I$(srcdir)/../libutils \
	-I$(srcdir)/../libcfnet \
	$(NOVA_CPPFLAGS) \
	$(LIBVIRT_CPPFLAGS) \
	$(POSTGRESQL_CPPFLAGS) \
	$(MYSQL_CPPFLAGS) \
	$(LIBACL_CPPFLAGS) \
	$(LIBXML2_CPPFLAGS)

if HAVE_NOVA
AM_CPPFLAGS += -I$(abs_top_srcdir)/nova/libcfagent
endif

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS) \
	$(LIBVIRT_CFLAGS) \
	$(POSTGRESQL_CFLAGS) \
	$(MYSQL_CFLAGS) \
	$(LIBACL_CFLAGS) \
	$(LIBXML2_CFLAGS)

AM_LDFLAGS = \
	$(LIBVIRT_LDFLAGS) \
	$(POSTGRESQL_LDFLAGS) \
	$(MYSQL_LDFLAGS) \
	$(LIBACL_LDFLAGS) \
	$(LIBXML2_LDFLAGS)

if HAVE_AVAHI_COMMON
if HAVE_AVAHI_CLIENT
AM_LDFLAGS += -ldl
endif
endif

LDADD = ../libpromises/libpromises.la \
	$(LIBVIRT_LIBS) \
	$(POSTGRESQL_LIBS) \
	$(MYSQL_LIBS) \
	$(LIBACL_LIBS) \
	$(LIBXML2_LIBS)
if HAVE_NOVA
LDADD += ../nova/libcfagent/libcfagent.la
endif

cf_agent_SOURCES = \
        agent-diagnostics.c agent-diagnostics.h \
        tokyo_check.c tokyo_check.h \
        abstract_dir.c abstract_dir.h \
        cf-agent.c \
        cf_acl.c cf_acl.h \
        comparray.c comparray.h \
        acl_posix.c acl_posix.h \
        cf_sql.c cf_sql.h \
        promiser_regex_resolver.c promiser_regex_resolver.h \
        retcode.c retcode.h \
        verify_files.c verify_files.h \
        verify_files_utils.c verify_files_utils.h \
        verify_files_hashes.c verify_files_hashes.h \
        verify_storage.c verify_storage.h \
        verify_exec.c verify_exec.h \
        verify_methods.c verify_methods.h \
        verify_databases.c verify_databases.h \
        verify_processes.c verify_processes.h \
        verify_services.c verify_services.h \
        verify_environments.c verify_environments.h \
        files_edit.c files_edit.h \
        files_editline.c files_editline.h \
        files_editxml.c files_editxml.h \
        files_links.c files_links.h \
        files_operators.c files_operators.h \
        files_properties.c files_properties.h \
        files_repository.c files_repository.h \
        files_select.c files_select.h \
        vercmp_internal.c vercmp_internal.h \
        vercmp.c vercmp.h \
        verify_packages.c verify_packages.h

if !NT
cf_agent_SOURCES += nfs.c nfs.h
endif

if !HAVE_NOVA
cf_agent_SOURCES += cf-agent-enterprise-stubs.c cf-agent-enterprise-stubs.h
endif

if HAVE_AVAHI_CLIENT
if HAVE_AVAHI_COMMON
cf_agent_SOURCES += \
		findhub.c findhub_priv.h findhub.h \
		load_avahi.c load_avahi.h
endif
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej
