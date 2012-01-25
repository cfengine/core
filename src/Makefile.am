AM_CFLAGS = $(GCC_CFLAG) $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) @CFLAGS@
AM_YFLAGS = -d
LDADD = libpromises.la

# libpromises

projlib_LTLIBRARIES = libpromises.la

libpromises_la_LDFLAGS = -version-info 1:0:0 -no-undefined
libpromises_la_LIBADD = ../pub/libcfpub.la $(NOVA_LDADD) $(CONSTELLATION_LDADD)

libpromises_la_CFLAGS = $(AM_CFLAGS)

# Separate out a library

libpromises_la_SOURCES =                                                \
        cf3parse.y                                                      \
        cf3lex.l                                                        \
        alloc.c alloc.h                                                 \
        patches.c                                                       \
        attributes.c attributes.h                                       \
        install.c                                                       \
        generic_agent.c generic_agent.h                                 \
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
        files_repository.c files_repository.h                           \
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
        ontology.c ontology.h                                           \
        item_lib.c                                                      \
        string_lib.c string_lib.h                                       \
        files_lib.c files_lib.h                                         \
        addr_lib.c                                                      \
        package_lib.c                                                   \
        alphalist.c alphalist.h                                         \
        cf_sql.c                                                        \
        client_protocol.c client_protocol.h                             \
        client_code.c                                                   \
        communication.c                                                 \
        net.c                                                           \
        sockaddr.c                                                      \
        recursion.c                                                     \
        processes_select.c                                              \
        fncall.c                                                        \
        cf3globals.c                                                    \
        reporting.c                                                     \
        evalfunction.c evalfunction.h                                   \
        sysinfo.c                                                       \
        conversion.c                                                    \
        scope.c                                                         \
        assoc.c assoc.h                                                 \
        comparray.c                                                     \
        vars.c                                                          \
        args.c                                                          \
        hashes.c                                                        \
        crypto.c                                                        \
        sort.c                                                          \
        keyring.c                                                       \
        full_write.c                                                    \
        expand.c                                                        \
        matching.c                                                      \
        selfdiagnostic.c                                                \
        instrumentation.c                                               \
        granules.c                                                      \
        timeout.c                                                       \
        promises.c                                                      \
        constraints.c constraints.h                                     \
        iteration.c                                                     \
        rlist.c rlist.h                                                 \
        syntax.c syntax.h                                               \
        logging.c                                                       \
        signals.c                                                       \
        transaction.c                                                   \
        cfstream.c                                                      \
        pipes.c                                                         \
        html.c                                                          \
        storage_tools.c                                                 \
        verify_reports.c                                                \
        verify_processes.c                                              \
        enterprise_stubs.c                                              \
        bootstrap.c	                                                   \
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
        mod_process.c                                                   \
        mod_environ.c                                                   \
        mod_outputs.c                                                   \
        writer.c writer.h                                               \
        unix.c                                                          \
        json.c json.h                                                   \
        csv_writer.c csv_writer.h                                       \
        xml_writer.c xml_writer.h                                       \
        proc_keyvalue.c proc_keyvalue.h                                 \
        show.h                                                          \
        parser.c parser.h                                               \
        parser_state.h                                                  \
        constants.c

# Automake need to be guided due to symlinking
libpromises_la_DEPENDENCIES = ../pub/libcfpub.la $(NOVA_LDADD) $(CONSTELLATION_LDADD)

sbin_PROGRAMS = cf-know cf-promises cf-agent cf-monitord cf-serverd cf-execd cf-runagent cf-key cf-report

if HAVE_NOVA
if MONGO
  sbin_PROGRAMS += cf-hub
endif
endif

cf_promises_SOURCES =                                                   \
        cf-promises.c

cf_key_SOURCES =                                                        \
        cf-key.c

cf_report_SOURCES =                                                     \
        cf-report.c

cf_agent_SOURCES =                                                      \
        cf-agent.c                                                      \
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
        cf-serverd.c                                                    \
        server_transform.c

cf_monitord_SOURCES =                                                   \
        verify_measurements.c                                           \
        env_monitor.c env_monitor.h                                     \
        mon_cpu.c                                                       \
        mon_disk.c                                                      \
        mon_entropy.c                                                   \
        mon_load.c                                                      \
        mon_network_sniffer.c                                           \
        mon_network.c                                                   \
        mon_processes.c                                                 \
        mon_temp.c                                                      \
        cf-monitord.c

cf_monitord_LDADD = libpromises.la
if HAVE_NOVA
cf_monitord_LDADD += ../nova/src/libcfmonitord.la
endif

cf_execd_SOURCES =                                                      \
        cf-execd.c

if HAVE_NOVA
if MONGO
cf_hub_SOURCES =

cf_hub_LDADD =                                                          \
        ../nova/src/libcfhub.la libpromises.la
endif
endif

cf_runagent_SOURCES =                                                   \
        cf-runagent.c

cf_know_SOURCES =                                                       \
        cf-know.c                                                       \
        manual.c manual.h 						\
        export_xml.c export_xml.h

cf_know_LDADD = libpromises.la

if HAVE_NOVA
cf_know_LDADD += ../nova/src/libcfknow.la
endif

#
# Pickup header files here so Automake knows about them
#
noinst_HEADERS =                                                        \
        cf.defs.h                                                       \
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
