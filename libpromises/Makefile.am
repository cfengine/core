projlib_LTLIBRARIES = libpromises.la

AM_CPPFLAGS = @CPPFLAGS@ -I$(srcdir)/../libutils -I $(srcdir)/../libcfnet \
	$(NOVA_CPPFLAGS)

AM_CFLAGS = @CFLAGS@ \
	$(NOVA_CFLAGS)

AM_YFLAGS = -d

AM_LDFLAGS = -version-info 1:0:0 -no-undefined

libpromises_la_LIBADD = ../libutils/libutils.la ../libcfnet/libcfnet.la $(NOVA_LDADD)

libpromises_la_SOURCES = \
        cf3parse.y cf3parse.h \
        cf3lex.l \
        addr_lib.c addr_lib.h \
        args.c args.h \
        assoc.c assoc.h \
        audit.c audit.h \
        attributes.c attributes.h \
        bootstrap.c bootstrap.h \
        cf3.defs.h \
        cf3.extern.h \
        cf3globals.c \
        chflags.c chflags.h \
        classes.c \
        constants.c \
        conversion.c conversion.h \
        crypto.c crypto.h \
        dbm_api.c dbm_api.h dbm_priv.h \
        dbm_migration.c dbm_migration.h \
        dbm_migration_lastseen.c \
        dbm_migration_bundles.c \
        dbm_quick.c \
        dbm_tokyocab.c \
        dir.h dir_priv.h \
        env_context.c env_context.h \
        evalfunction.c evalfunction.h \
        exec_tools.c exec_tools.h \
        expand.c expand.h \
        files_copy.c files_copy.h \
        files_hashes.c files_hashes.h \
        files_interfaces.c files_interfaces.h \
        files_lib.c files_lib.h \
        files_names.c files_names.h \
        fncall.c fncall.h \
        generic_agent.c generic_agent.h \
        granules.c granules.h \
        instrumentation.c instrumentation.h \
        item_lib.c item_lib.h \
        iteration.c iteration.h \
        keyring.c keyring.h\
        lastseen.c lastseen.h \
        locks.c locks.h \
        logging_old.c logging_old.h \
        logic_expressions.c logic_expressions.h \
        matching.c matching.h \
        mod_access.c mod_access.h \
        mod_common.c mod_common.h \
        mod_databases.c mod_databases.h \
        mod_environ.c mod_environ.h \
        mod_exec.c mod_exec.h \
        mod_files.c mod_files.h \
        mod_measurement.c mod_measurement.h \
        mod_methods.c mod_methods.h \
        mod_outputs.c mod_outputs.h \
        mod_packages.c mod_packages.h \
        mod_process.c mod_process.h \
        mod_report.c mod_report.h \
        mod_services.c mod_services.h \
        mod_storage.c mod_storage.h \
        mod_knowledge.c mod_knowledge.h \
        modes.c \
        mutex.c mutex.h \
        ornaments.c ornaments.h \
        policy.c policy.h \
        parser.c parser.h \
        parser_state.h \
        patches.c \
        pipes.h \
        processes_select.c processes_select.h \
        process_lib.h process_unix_priv.h \
        promise_logging.c promise_logging.h \
        promises.c promises.h \
        prototypes3.h \
        rlist.c rlist.h \
        scope.c scope.h \
        signals.c signals.h \
        sort.c sort.h \
        storage_tools.c \
        string_expressions.c string_expressions.h \
        syntax.c syntax.h \
        sysinfo.c sysinfo.h \
        syslog_client.c syslog_client.h \
        timeout.c \
        unix.c unix.h \
        var_expressions.c var_expressions.h \
        vars.c vars.h \
        verify_classes.c verify_classes.h \
        verify_reports.c \
        verify_vars.c verify_vars.h

if !HAVE_NOVA
libpromises_la_SOURCES += \
	enterprise_stubs.c
endif

if !NT

libpromises_la_SOURCES += \
        process_unix.c \
        pipes_unix.c

if LINUX
libpromises_la_SOURCES += \
        process_linux.c
endif

if AIX
libpromises_la_SOURCES += \
	process_aix.c
endif

if SOLARIS
libpromises_la_SOURCES += \
	process_solaris.c
endif

if !LINUX
if !AIX
if !SOLARIS
libpromises_la_SOURCES += \
        process_unix_stub.c
endif
endif
endif

endif # !NT

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
libpromises_la_DEPENDENCIES = ../libutils/libutils.la ../libcfnet/libcfnet.la $(NOVA_LDADD)

.PRECIOUS: ../libutils/libutils.la

../libutils/libutils.la:
	$(MAKE) -C ../libutils $(AM_MAKEFLAGS) libutils.la
