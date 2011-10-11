AM_CFLAGS = $(GCC_CFLAG) $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) @CFLAGS@
AM_YFLAGS = -d
LEXLIB = @LEXLIB@
LDADD = libpromises.la
BUILT_SOURCES = cf3parse.h

lib_LTLIBRARIES = libpromises.la

libpromises_la_LDFLAGS = -version-info 1:0:0 -no-undefined
libpromises_la_LIBADD = ../pub/libcfpub.la $(NOVA_LDADD) $(CONSTELLATION_LDADD)

libpromises_la_CFLAGS = $(AM_CFLAGS)

# Separate out a library

libpromises_la_SOURCES =                                                \
        cf3parse.y                                                      \
        cf3lex.l                                                        \
        alloc.c                                                         \
        patches.c                                                       \
        attributes.c                                                    \
        install.c                                                       \
        generic_agent.c                                                 \
        dbm_api.c                                                       \
        dbm_berkeley.c                                                  \
        dbm_quick.c                                                     \
        dbm_tokyocab.c                                                  \
        dbm_sqlite3.c                                                   \
        dir.c                                                           \
        dtypes.c                                                        \
        classes.c                                                       \
        env_context.c                                                   \
        string_expressions.c                                            \
        logic_expressions.c                                             \
        files_interfaces.c                                              \
        files_properties.c                                              \
        files_select.c                                                  \
        files_operators.c                                               \
        files_repository.c                                              \
        files_copy.c                                                    \
        files_editline.c                                                \
        files_edit.c                                                    \
        files_links.c                                                   \
        files_hashes.c                                                  \
        files_names.c                                                   \
        chflags.c                                                       \
        modes.c                                                         \
        exec_tools.c                                                    \
        nfs.c                                                           \
        item-lib.c                                                      \
        alphalist.c                                                     \
        cf_sql.c                                                        \
        client_protocol.c                                               \
        client_code.c                                                   \
        communication.c                                                 \
        net.c                                                           \
        sockaddr.c                                                      \
        recursion.c                                                     \
        processes_select.c                                              \
        fncall.c                                                        \
        cf3globals.c                                                    \
        reporting.c                                                     \
        evalfunction.c                                                  \
        sysinfo.c                                                       \
        conversion.c                                                    \
        scope.c                                                         \
        assoc.c                                                         \
        comparray.c                                                     \
        vars.c                                                          \
        args.c                                                          \
        hashes.c                                                        \
        crypto.c                                                        \
        sort.c                                                          \
	keyring.c							\
        full-write.c                                                    \
        expand.c                                                        \
        matching.c                                                      \
        selfdiagnostic.c                                                \
        instrumentation.c                                               \
        granules.c                                                      \
        timeout.c                                                       \
        promises.c                                                      \
        ontology.c							\
        constraints.c                                                   \
        iteration.c                                                     \
        rlist.c                                                         \
        syntax.c                                                        \
        logging.c                                                       \
        signals.c                                                       \
        transaction.c                                                   \
        cfstream.c                                                      \
        pipes.c                                                         \
        html.c                                                          \
        interfaces.c                                                    \
        storage_tools.c                                                 \
        verify_reports.c                                                \
        verify_processes.c                                              \
        enterprise_stubs.c                                              \
	bootstrap.c							\
	mod_defaults.c							\
        mod_common.c                                                    \
        mod_access.c                                                    \
        mod_exec.c                                                      \
        mod_methods.c                                                   \
        mod_interfaces.c                                                \
        mod_packages.c                                                  \
        mod_files.c                                                     \
        mod_report.c                                                    \
        mod_storage.c                                                   \
        mod_knowledge.c                                                 \
        mod_measurement.c                                               \
        mod_databases.c                                                 \
        mod_services.c                                                  \
        mod_process.c   			                        \
	mod_environ.c							\
	mod_outputs.c							\
        unix.c

# Automake need to be guided due to symlinking
libpromises_la_DEPENDENCIES = ../pub/libcfpub.la $(NOVA_LDADD) $(CONSTELLATION_LDADD)

sbin_PROGRAMS = cf-know cf-promises cf-agent cf-monitord cf-serverd cf-execd cf-runagent cf-key cf-report

if HAVE_NOVA
  sbin_PROGRAMS += cf-hub
endif

cf_promises_SOURCES =                                                   \
        cfpromises.c

cf_key_SOURCES =                                                        \
        cfkey.c

cf_report_SOURCES =                                                     \
        cfreport.c

cf_agent_SOURCES =                                                      \
        agent.c                                                         \
        verify_files.c                                                  \
        verify_storage.c                                                \
        verify_exec.c                                                   \
        verify_methods.c                                                \
        verify_interfaces.c                                             \
        verify_packages.c                                               \
        verify_databases.c                                              \
	verify_services.c						\
	verify_environments.c						\
        agentdiagnostic.c

cf_serverd_SOURCES =                                                    \
        server.c                                                        \
        server_transform.c

cf_monitord_SOURCES =                                                   \
        verify_measurements.c                                           \
        env_monitor.c                                                   \
        mon_cpu.c                                                       \
        mon_disk.c                                                      \
        mon_entropy.c                                                   \
        mon_load.c                                                      \
        mon_network_sniffer.c                                           \
        mon_network.c                                                   \
        mon_processes.c                                                 \
        mon_temp.c                                                      \
        monitor.c

cf_monitord_LDADD = libpromises.la
if HAVE_NOVA
cf_monitord_LDADD += ../nova/src/libcfmonitord.la
endif

cf_execd_SOURCES =                                                      \
        exec.c

if HAVE_NOVA
cf_hub_SOURCES =

cf_hub_LDADD =                                                          \
        ../nova/src/libcfhub.la libpromises.la
endif

cf_runagent_SOURCES =                                                   \
        runagent.c

cf_know_SOURCES =                                                       \
        cfknow.c                                                        \
        manual.c  
EXTRA_DIST = cf3lex.c cf3parse.c

#
# Pickup header files here so Automake knows about them
#
noinst_HEADERS =                                                        \
        cf.defs.h                                                       \
        cf.extern.h                                                     \
        config.h                                                        \
        prototypes3.h                                                   \
        cf3.defs.h                                                      \
        cf3.server.h                                                    \
        cf3parse.h                                                      \
        cf3.extern.h                                                    \
        cf.events.h                                                     \
        string_expressions.h                                            \
        logic_expressions.h                                             \
        compiler.h                                                      \
        monitoring.h                                                    \
        bool.h

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej

#
# Get everything removed down to where rebuilding requires:
# "make; make install"
#
CLEANFILES = cf3lex.c cf3parse.c cf3parse.h

#
# Get everything removed down to where rebuilding requires:
# "configure; make; make install"
#
DISTCLEANFILES = cflex.c cfparse.c cfparse.h

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = Makefile.in conf.h.in stamp-h.in

SUFFIXES= .h
cf3.defs.h:
	cd ./../core/src; svn update
	ln -fs ../../core/src/cf3.defs.h cf3.defs.h