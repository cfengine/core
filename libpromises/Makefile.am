AM_CFLAGS = $(NOVA_CFLAGS) @CFLAGS@
AM_YFLAGS = -d
LDADD = libpromises.la

.PRECIOUS: ../libutils/libutils.la

../libutils/libutils.la:
	$(MAKE) -C ../libutils $(AM_MAKEFLAGS) libutils.la

# libpromises

projlib_LTLIBRARIES = libpromises.la

libpromises_la_LDFLAGS = -version-info 1:0:0 -no-undefined
libpromises_la_LIBADD = ../libutils/libutils.la $(NOVA_LDADD)

libpromises_la_CFLAGS = $(AM_CFLAGS)

# Separate out a library. Note that cf3parse.y should be built before cf3lex.l

libpromises_la_SOURCES = \
        cf3parse.y cf3parse.h \
        cf3lex.l \
        addr_lib.c addr_lib.h \
        alphalist.c alphalist.h \
        cf_acl.c cf_acl.h \
        acl_posix.c acl_posix.h \
        args.c args.h \
        assoc.c assoc.h \
        atexit.c atexit.h \
        attributes.c attributes.h \
        bootstrap.c bootstrap.h \
        cf.events.h \
        cf3.defs.h \
        cf3.extern.h \
        cf3globals.c \
        cfstream.c cfstream.h \
        chflags.c chflags.h \
        classes.c \
        client_code.c client_code.h \
        client_protocol.c client_protocol.h \
        communication.c communication.h \
        comparray.c \
        constants.c \
        constraints.c constraints.h \
        conversion.c conversion.h \
        crypto.c crypto.h \
        dbm_api.c dbm_api.h dbm_lib.h dbm_lib.c dbm_priv.h \
        dbm_migration.c dbm_migration.h \
        dbm_migration_lastseen.c \
        dbm_migration_bundles.c \
        dbm_quick.c \
        dbm_tokyocab.c \
        dir.c dir.h dir_priv.h \
        dtypes.c \
        enterprise_stubs.c \
        env_context.c env_context.h \
        evalfunction.c evalfunction.h \
        exec_tools.c exec_tools.h \
        expand.c expand.h \
        files_copy.c \
        files_hashes.c files_hashes.h \
        files_interfaces.c files_interfaces.h \
        files_lib.c files_lib.h \
        files_links.c files_links.h \
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
        html.c \
        instrumentation.c instrumentation.h \
        item_lib.c item_lib.h \
        iteration.c iteration.h \
        json.c json.h \
        keyring.c keyring.h\
        lastseen.c lastseen.h \
        logging.c logging.h \
        logic_expressions.c logic_expressions.h \
        matching.c matching.h \
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
        net.c net.h \
        policy.c policy.h \
        parser.c parser.h \
        parser_state.h \
        patches.c \
        pipes.c pipes.h \
        proc_keyvalue.c proc_keyvalue.h \
        processes_select.c \
        promises.c promises.h \
        prototypes3.h \
        reporting.c reporting.h \
        rlist.c rlist.h \
        scope.c scope.h \
        selfdiagnostic.c \
        signals.c signals.h \
        sockaddr.c \
        sort.c sort.h \
        statistics.c statistics.h \
        storage_tools.c \
        string_expressions.c string_expressions.h \
        string_map.c string_map.h \
        syntax.c syntax.h \
        sysinfo.c sysinfo.h \
        timeout.c \
        transaction.c transaction.h \
        unix.c unix.h \
        vars.c vars.h \
        verify_processes.c verify_processes.h \
        verify_reports.c

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
libpromises_la_DEPENDENCIES = ../libutils/libutils.la $(NOVA_LDADD)

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
