AM_CFLAGS = $(GCC_CFLAG) $(NOVA_CFLAGS) @CFLAGS@
AM_YFLAGS = -d
LDADD = libpromises.la

# libcfpub

.PRECIOUS: ../pub/libcfpub.la

../pub/libcfpub.la:
	$(MAKE) -C ../pub $(AM_MAKEFLAGS) libcfpub.la

# libpromises

projlib_LTLIBRARIES = libpromises.la

libpromises_la_LDFLAGS = -version-info 1:0:0 -no-undefined
libpromises_la_LIBADD = ../pub/libcfpub.la $(NOVA_LDADD)

libpromises_la_CFLAGS = $(AM_CFLAGS)

# Separate out a library. Note that cf3parse.y should be built before cf3lex.l

libpromises_la_SOURCES = \
        cf3parse.y cf3parse.h \
        cf3lex.l \
        addr_lib.c addr_lib.h \
        alloc.c alloc.h \
        alphalist.c alphalist.h \
        acl.c acl.h \
        acl_posix.c acl_posix.h \
        args.c args.h \
        array_map.c array_map_priv.h \
        assoc.c assoc.h \
        atexit.c atexit.h \
        attributes.c attributes.h \
        bool.h \
        bootstrap.c bootstrap.h \
        cf.events.h \
        cf3.defs.h \
        cf3.extern.h \
        cf3globals.c \
        cfstream.c cfstream.h \
        chflags.c \
        classes.c \
        client_code.c client_code.h \
        client_protocol.c client_protocol.h \
        communication.c communication.h \
        comparray.c \
        compiler.h \
        config.h \
        constants.c \
        constraints.c constraints.h \
        conversion.c conversion.h \
        crypto.c crypto.h \
        csv_writer.c csv_writer.h \
        dbm_api.c dbm_api.h dbm_lib.h dbm_lib.c dbm_priv.h \
        dbm_migration.c dbm_migration.h dbm_migration_lastseen.c dbm_migration_bundles.c \
        dbm_quick.c \
        dbm_tokyocab.c \
        dir.c dir.h dir_priv.h \
        dtypes.c \
        enterprise_stubs.c \
        env_context.c env_context.h \
        evalfunction.c evalfunction.h \
        exec_tools.c \
        expand.c expand.h \
        files_copy.c \
        files_editline.c \
        files_hashes.c files_hashes.h \
        files_interfaces.c files_interfaces.h \
        files_lib.c files_lib.h \
        files_links.c \
        files_names.c files_names.h \
        files_operators.c files_operators.h \
        files_properties.c \
        files_repository.c files_repository.h \
        files_select.c \
        fncall.c fncall.h \
        full_write.c \
        generic_agent.c generic_agent.h \
        granules.c granules.h \
        hashes.c hashes.h \
        hash_map.c hash_map_priv.h \
        html.c \
        install.c \
        instrumentation.c instrumentation.h \
        item_lib.c item_lib.h \
        iteration.c \
        json.c json.h \
        keyring.c keyring.h\
        lastseen.c lastseen.h \
        logging.c \
        logic_expressions.c logic_expressions.h \
        map.c map.h \
        map_common.h \
        matching.c matching.h \
        misc_lib.c misc_lib.h \
        mod_access.c mod_access.h \
        mod_common.c \
        mod_databases.c mod_databases.h \
        mod_environ.c mod_environ.h \
        mod_exec.c mod_exec.h \
        mod_files.c mod_files.h \
        mod_interfaces.c mod_interfaces.h \
        mod_knowledge.c mod_knowledge.h \
        mod_measurement.c mod_measurement.h \
        mod_methods.c mod_methods.h \
        mod_outputs.c mod_outputs.h \
        mod_packages.c mod_packages.h \
        mod_process.c mod_process.h \
        mod_report.c mod_report.h \
        mod_services.c mod_services.h \
        mod_storage.c mod_storage.h \
        modes.c \
        monitoring.h \
        net.c \
        nfs.c \
        ontology.c ontology.h \
        policy.c policy.h \
        parser.c parser.h \
        parser_state.h \
        patches.c \
        pipes.c pipes.h \
        platform.h \
        proc_keyvalue.c proc_keyvalue.h \
        processes_select.c \
        promises.c promises.h \
        prototypes3.h \
        recursion.c \
        reporting.c reporting.h \
        rlist.c rlist.h \
        scope.c scope.h \
        selfdiagnostic.c \
        sequence.c sequence.h \
        set.c set.h \
        show.h \
        signals.c \
        sockaddr.c \
        sort.c sort.h \
	statistics.c statistics.h \
        storage_tools.c \
        string_expressions.c string_expressions.h \
        string_lib.c string_lib.h \
        string_map.c string_map.h \
        syntax.c syntax.h \
        sysinfo.c sysinfo.h \
        timeout.c \
        transaction.c transaction.h \
        unix.c unix.h \
        vars.c vars.h \
        verify_processes.c verify_processes.h \
        verify_reports.c \
        writer.c writer.h \
        xml_writer.c xml_writer.h

if !NT
libpromises_la_SOURCES += \
        unix_dir.c
endif
if CYGWIN
libpromises_la_SOURCES += \
        unix_dir.c
endif

BUILT_SOURCES = cf3parse.h

# Automake need to be guided due to symlinking
libpromises_la_DEPENDENCIES = ../pub/libcfpub.la $(NOVA_LDADD)

sbin_PROGRAMS = \
        cf-agent \
        cf-execd \
        cf-key \
        cf-monitord \
        cf-promises \
        cf-report \
        cf-runagent \
        cf-serverd

noinst_PROGRAMS = \
        cf-gendoc

if HAVE_NOVA
if MONGO
  sbin_PROGRAMS += cf-hub
endif
endif

if HAVE_NOVA
  sbin_PROGRAMS += cf-know
endif

cf_agent_SOURCES = \
        cf-agent.c \
        cf_sql.c \
        verify_files.c \
        verify_storage.c \
        verify_exec.c \
        verify_methods.c \
        verify_interfaces.c \
        verify_databases.c \
        verify_outputs.c \
        verify_services.c \
        verify_environments.c verify_environments.h \
        files_edit.c \
        files_editxml.c \
        vercmp_internal.c vercmp_internal.h \
        vercmp.c vercmp.h \
        verify_packages.c \
        agentdiagnostic.c

cf_agent_LDADD =
if HAVE_NOVA
cf_agent_LDADD += ../nova/src/libcfagent.la
endif
cf_agent_LDADD += libpromises.la \
	$(LIBXML2_LDFLAGS) $(LIBXML2_LIBS) \
	$(LIBVIRT_LDFLAGS) $(LIBVIRT_LIBS) \
	$(POSTGRESQL_LDFLAGS) $(POSTGRESQL_LIBS) \
	$(MYSQL_LDFLAGS) $(MYSQL_LIBS)

cf_gendoc_SOURCES = \
	cf-gendoc.c \
        manual.c manual.h \
        export_xml.c export_xml.h

cf_serverd_SOURCES = \
        cf-serverd.c \
        cf-serverd-functions.c \
        cf-serverd-functions.h \
        server.c server.h \
        server_transform.c \
        enterprise_stubs_serverd.c

cf_serverd_LDADD =
if HAVE_NOVA
cf_serverd_LDADD += ../nova/src/libcfserverd.la
endif
cf_serverd_LDADD += libpromises.la

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

cf_report_LDADD =
if HAVE_NOVA
cf_report_LDADD += ../nova/src/libcfreport.la
endif
cf_report_LDADD += libpromises.la

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

if HAVE_NOVA
cf_know_SOURCES =

cf_know_LDADD = \
        ../nova/src/libcfknow.la libpromises.la
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
