AM_CFLAGS = $(GCC_CFLAG) $(NOVA_CFLAGS) $(CONSTELLATION_CFLAGS) @CFLAGS@
AM_YFLAGS = -d
LDADD = libpromises.la

# libcfpub

.PRECIOUS: ../pub/libcfpub.la

../pub/libcfpub.la:
	$(MAKE) -C ../pub $(AM_MAKEFLAGS) libcfpub.la

# libpromises

projlib_LTLIBRARIES = libpromises.la

libpromises_la_LDFLAGS = -version-info 1:0:0 -no-undefined
libpromises_la_LIBADD = ../pub/libcfpub.la $(NOVA_LDADD) $(CONSTELLATION_LDADD)

libpromises_la_CFLAGS = $(AM_CFLAGS)

# Separate out a library. Note that cf3parse.y should be built before cf3lex.l

libpromises_la_SOURCES = \
        cf3parse.y cf3parse.h \
        cf3lex.l \
        addr_lib.c \
        alloc.c alloc.h \
        alphalist.c alphalist.h \
        args.c \
        assoc.c assoc.h \
        attributes.c attributes.h \
        bool.h \
        bootstrap.c \
        cf.defs.h \
        cf.events.h \
        cf3.defs.h \
        cf3.extern.h \
        cf3.server.h \
        cf3globals.c \
        cf_sql.c \
        cfstream.c \
        chflags.c \
        classes.c \
        client_code.c \
        client_protocol.c client_protocol.h \
        communication.c \
        comparray.c \
        compiler.h \
        config.h \
        constants.c \
        constraints.c constraints.h \
        conversion.c \
        crypto.c crypto.h \
        csv_writer.c csv_writer.h \
        dbm_api.c dbm_api.h dbm_lib.h dbm_lib.c dbm_priv.h \
        dbm_quick.c \
        dbm_tokyocab.c \
        dir.c dir.h dir_priv.h \
        dtypes.c \
        enterprise_stubs.c \
        env_context.c \
        evalfunction.c evalfunction.h \
        exec_tools.c \
        expand.c \
        files_copy.c \
        files_edit.c \
        files_editline.c \
        files_hashes.c \
        files_interfaces.c \
        files_lib.c files_lib.h \
        files_links.c \
        files_names.c files_names.h \
        files_operators.c \
        files_properties.c \
        files_repository.c files_repository.h \
        files_select.c \
        fncall.c fncall.h \
        full_write.c \
        generic_agent.c generic_agent.h \
        granules.c granules.h \
        hashes.c \
        html.c \
        install.c \
        instrumentation.c \
        item_lib.c \
        iteration.c \
        json.c json.h \
        keyring.c \
        lastseen.c lastseen.h \
        lastseen_migration.c \
        logging.c \
        logic_expressions.c logic_expressions.h \
        matching.c \
        misc_lib.c misc_lib.h \
        mod_access.c \
        mod_common.c \
        mod_databases.c \
        mod_environ.c \
        mod_exec.c \
        mod_files.c \
        mod_interfaces.c \
        mod_knowledge.c \
        mod_measurement.c \
        mod_methods.c \
        mod_outputs.c \
        mod_packages.c \
        mod_process.c \
        mod_report.c \
        mod_services.c \
        mod_storage.c \
        modes.c \
        monitoring.h \
        net.c \
        nfs.c \
        ontology.c ontology.h \
        package_lib.c \
        parser.c parser.h \
        parser_state.h \
        patches.c \
        pipes.c \
        proc_keyvalue.c proc_keyvalue.h \
        processes_select.c \
	process.h process_unix_priv.h \
        promises.c \
        prototypes3.h \
        recursion.c \
        reporting.c \
        rlist.c rlist.h \
        scope.c \
        selfdiagnostic.c \
        sequence.c sequence.h \
        show.h \
        signals.c \
        sockaddr.c \
        sort.c \
	statistics.c statistics.h \
        storage_tools.c \
        string_expressions.c string_expressions.h \
        string_lib.c string_lib.h \
        syntax.c syntax.h \
        sysinfo.c \
        timeout.c \
        transaction.c transaction.h \
        unix.c \
        vars.c vars.h \
        verify_processes.c \
        verify_reports.c \
        writer.c writer.h \
        xml_writer.c xml_writer.h

if !NT

libpromises_la_SOURCES += \
        process_unix.c

if LINUX
libpromises_la_SOURCES += \
        process_linux.c
endif

if !LINUX
libpromises_la_SOURCES += \
        process_unix_stub.c
endif

endif # !NT

if !NT
libpromises_la_SOURCES += \
        unix_dir.c
endif

BUILT_SOURCES = cf3parse.h

# Automake need to be guided due to symlinking
libpromises_la_DEPENDENCIES = ../pub/libcfpub.la $(NOVA_LDADD) $(CONSTELLATION_LDADD)

sbin_PROGRAMS = \
        cf-agent \
        cf-execd \
        cf-key \
        cf-know \
        cf-monitord \
        cf-promises \
        cf-report \
        cf-runagent \
        cf-serverd

if HAVE_NOVA
if MONGO
  sbin_PROGRAMS += cf-hub
endif
endif

cf_promises_SOURCES = \
        cf-promises.c

cf_key_SOURCES = \
        cf-key.c

cf_report_SOURCES = \
        cf-report.c

cf_agent_SOURCES = \
        cf-agent.c \
        verify_files.c \
        verify_storage.c \
        verify_exec.c \
        verify_methods.c \
        verify_interfaces.c \
        verify_packages.c \
        verify_databases.c \
        verify_services.c \
        verify_environments.c verify_environments.h \
        agentdiagnostic.c

cf_serverd_SOURCES = \
        cf-serverd.c \
        server_transform.c

cf_monitord_SOURCES = \
        verify_measurements.c \
        env_monitor.c env_monitor.h \
        mon_cpu.c \
        mon_disk.c \
        mon_entropy.c \
        mon_load.c \
        mon_network_sniffer.c \
        mon_network.c \
        mon_processes.c \
        mon_temp.c \
        cf-monitord.c

cf_monitord_LDADD =
if HAVE_NOVA
cf_monitord_LDADD += ../nova/src/libcfmonitord.la
endif
cf_monitord_LDADD += libpromises.la

cf_execd_SOURCES = \
        cf-execd.c \
        cf-execd-runner.c cf-execd-runner.h

if HAVE_NOVA
if MONGO
cf_hub_SOURCES =

cf_hub_LDADD = \
        ../nova/src/libcfhub.la libpromises.la
endif
endif

cf_runagent_SOURCES = \
        cf-runagent.c

cf_know_SOURCES = \
        cf-know.c \
        manual.c manual.h \
        export_xml.c export_xml.h

cf_know_LDADD = libpromises.la

if HAVE_NOVA
cf_know_LDADD += ../nova/src/libcfknow.la
endif

CLEANFILES = *.gcno *.gcda

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
