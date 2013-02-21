projlib_LTLIBRARIES = libpromises.la

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libutils \
	$(NOVA_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

AM_YFLAGS = -d

AM_LDFLAGS = -version-info 1:0:0 -no-undefined

libpromises_la_LIBADD = ../libutils/libutils.la $(NOVA_LDADD)

libpromises_la_SOURCES = \
        cf3parse.y cf3parse.h \
        cf3lex.l \
        addr_lib.c addr_lib.h \
        alphalist.c alphalist.h \
        args.c args.h \
        assoc.c assoc.h \
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
        constants.c \
        conversion.c conversion.h \
        crypto.c crypto.h \
        dbm_api.c dbm_api.h dbm_lib.h dbm_lib.c dbm_priv.h \
        dbm_migration.c dbm_migration.h \
        dbm_migration_lastseen.c \
        dbm_migration_bundles.c \
        dbm_quick.c \
        dbm_tokyocab.c \
        dir.c dir.h dir_priv.h \
        enterprise_stubs.c \
        env_context.c env_context.h \
        evalfunction.c evalfunction.h \
        exec_tools.c exec_tools.h \
        expand.c expand.h \
        files_copy.c \
        files_hashes.c files_hashes.h \
        files_interfaces.c files_interfaces.h \
        files_lib.c files_lib.h \
        files_names.c files_names.h \
        files_properties.c files_properties.h \
        fncall.c fncall.h \
        full_write.c \
        generic_agent.c generic_agent.h \
        granules.c granules.h \
        instrumentation.c instrumentation.h \
        item_lib.c item_lib.h \
        iteration.c iteration.h \
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
        processes_select.c processes_select.h \
        promises.c promises.h \
        prototypes3.h \
        reporting.c reporting.h \
        rlist.c rlist.h \
        scope.c scope.h \
        signals.c signals.h \
        sockaddr.c \
        sort.c sort.h \
        storage_tools.c \
        string_expressions.c string_expressions.h \
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

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej

# libutils dependency

# Automake need to be guided due to symlinking
DEPENDENCIES = ../libutils/libutils.la $(NOVA_LDADD)

.PRECIOUS: ../libutils/libutils.la

../libutils/libutils.la:
	$(MAKE) -C ../libutils $(AM_MAKEFLAGS) libutils.la
