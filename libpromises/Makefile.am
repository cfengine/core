#
#  Copyright 2021 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#
if !BUILTIN_EXTENSIONS
projlib_LTLIBRARIES = libpromises.la
AM_LDFLAGS = -version-info 3:6:0 -no-undefined
else
noinst_LTLIBRARIES = libpromises.la
AM_LDFLAGS =
endif

AM_LDFLAGS += $(CORE_LDFLAGS) $(LMDB_LDFLAGS) $(TOKYOCABINET_LDFLAGS) $(QDBM_LDFLAGS) \
	$(PCRE2_LDFLAGS) $(OPENSSL_LDFLAGS) $(SQLITE3_LDFLAGS) $(LIBACL_LDFLAGS) $(LIBYAML_LDFLAGS) $(LIBCURL_LDFLAGS) $(LIBRSYNC_LDFLAGS)

AM_CPPFLAGS = \
	-I$(srcdir)/../libntech/libutils -I$(srcdir)/../libcfecompat \
	-I$(srcdir)/../libcfnet \
	-I$(srcdir)/../libenv \
	-I$(srcdir)/../cf-check \
	$(CORE_CPPFLAGS) $(ENTERPRISE_CPPFLAGS) \
	$(LMDB_CPPFLAGS) $(TOKYOCABINET_CPPFLAGS) $(QDBM_CPPFLAGS) \
	$(PCRE2_CPPFLAGS) $(OPENSSL_CPPFLAGS) $(SQLITE3_CPPFLAGS) $(LIBACL_CPPFLAGS) $(LIBYAML_CPPFLAGS) $(LIBCURL_CPPFLAGS) $(LIBRSYNC_CPPFLAGS)

AM_CFLAGS = $(CORE_CFLAGS) $(ENTERPRISE_CFLAGS) \
	$(LMDB_CFLAGS) $(TOKYOCABINET_CFLAGS) $(QDBM_CFLAGS) \
	$(PCRE2_CFLAGS) $(OPENSSL_CFLAGS) $(SQLITE3_CFLAGS) $(LIBACL_CFLAGS) $(LIBYAML_CFLAGS) $(LIBCURL_CFLAGS) $(LIBRSYNC_CFLAGS)

AM_YFLAGS = -d

LIBS = $(LMDB_LIBS) $(TOKYOCABINET_LIBS) $(QDBM_LIBS) \
	$(PCRE2_LIBS) $(OPENSSL_LIBS) $(SQLITE3_LIBS) $(LIBACL_LIBS) $(LIBYAML_LIBS) $(LIBCURL_LIBS) $(LIBRSYNC_LIBS)

# The lib providing sd_listen_fds() is not needed in libpromises, it's actually
# needed in libcfnet. But adding it here is an easy way to make sure it's
# available for all the binaries that might need it. Once libcfnet doesn't
# require libpromises, we can do this properly.
LIBS += $(SYSTEMD_SOCKET_LIBS)

libpromises_la_LIBADD = ../libntech/libutils/libutils.la ../libcfecompat/libcfecompat.la \
	../libcfnet/libcfnet.la \
	../libenv/libenv.la $(ENTERPRISE_LDADD)

libpromises_la_SOURCES = \
	acl_tools.h acl_tools_posix.c \
	actuator.c actuator.h \
	assoc.c assoc.h \
	attributes.c attributes.h \
	audit.c audit.h \
	bootstrap.c bootstrap.h bootstrap.inc failsafe.cf \
	cf-windows-functions.h \
	cf3.defs.h \
	cf3.extern.h \
	cf3globals.c \
	cf3lex.l \
	cf3parse.y cf3parse.h \
	cf3parse_logic.h \
	changes_chroot.c changes_chroot.h \
	chflags.c chflags.h \
	class.c class.h \
	cmdb.c cmdb.h \
	constants.c \
	conversion.c conversion.h \
	crypto.c crypto.h \
	dbm_api.c dbm_api.h dbm_api_types.h dbm_priv.h \
	dbm_migration.c dbm_migration.h \
	dbm_migration_lastseen.c \
	dbm_lmdb.c \
	dbm_quick.c \
	dbm_tokyocab.c \
	enterprise_stubs.c enterprise_extension.c enterprise_extension.h \
	eval_context.c eval_context.h \
	evalfunction.c evalfunction.h \
	exec_tools.c exec_tools.h \
	expand.c expand.h \
	extensions.c extensions.h \
	extensions_template.c.pre extensions_template.h.pre \
	feature.c feature.h \
	files_copy.c files_copy.h \
	files_interfaces.c files_interfaces.h \
	files_lib.c files_lib.h \
	files_links.c files_links.h \
	files_names.c files_names.h \
	files_operators.c files_operators.h \
	files_repository.c files_repository.h \
	fncall.c fncall.h \
	generic_agent.c generic_agent.h \
	global_mutex.c global_mutex.h \
	granules.c granules.h \
	instrumentation.c instrumentation.h \
	item_lib.c item_lib.h \
	iteration.c iteration.h \
	keyring.c keyring.h \
	lastseen.c lastseen.h \
	loading.c loading.h \
	locks.c locks.h \
	logic_expressions.c logic_expressions.h \
	matching.c matching.h \
	match_scope.c match_scope.h \
	math_eval.c math_eval.h math.pc \
	mod_access.c mod_access.h \
	mod_common.c mod_common.h \
	mod_custom.c mod_custom.h \
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
	mod_users.c mod_users.h \
	modes.c \
	monitoring_read.c monitoring_read.h \
	ornaments.c ornaments.h \
	override_fsattrs.c override_fsattrs.h \
	policy.c policy.h \
	parser.c parser.h \
	parser_helpers.h \
	parser_state.h \
	patches.c \
	pipes.h pipes.c \
	processes_select.c processes_select.h \
	process_lib.h process_unix_priv.h \
	promises.c promises.h \
	prototypes3.h \
	rlist.c rlist.h \
	scope.c scope.h \
	shared_lib.c shared_lib.h \
	signals.c signals.h \
	sort.c sort.h \
	storage_tools.c \
	string_expressions.c string_expressions.h \
	syntax.c syntax.h \
	syslog_client.c syslog_client.h \
	systype.c systype.h \
	timeout.c timeout.h \
	unix.c unix.h \
	var_expressions.c var_expressions.h \
	variable.c variable.h \
	vars.c vars.h \
	verify_classes.c verify_classes.h \
	verify_reports.c \
	verify_vars.c verify_vars.h \
	../cf-check/backup.c ../cf-check/backup.h \
	../cf-check/diagnose.c ../cf-check/diagnose.h \
	../cf-check/lmdump.c ../cf-check/lmdump.h \
	../cf-check/repair.c ../cf-check/repair.h \
	../cf-check/replicate_lmdb.c ../cf-check/replicate_lmdb.h \
	../cf-check/utilities.c ../cf-check/utilities.h \
	../cf-check/validate.c ../cf-check/validate.h

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

if HPUX
libpromises_la_SOURCES += \
	process_hpux.c
endif

if SOLARIS
libpromises_la_SOURCES += \
	process_solaris.c
endif

if FREEBSD
libpromises_la_SOURCES += \
	process_freebsd.c
endif

if !LINUX
if !AIX
if !HPUX
if !SOLARIS
if !FREEBSD
libpromises_la_SOURCES += \
	process_unix_stub.c
endif
endif
endif
endif
endif

endif # !NT

if !NDEBUG
if LINUX
libpromises_la_SOURCES += \
	dbm_test_api.c dbm_test_api.h
endif
endif


########## Sources pre-generated and distributed in the tarball ##########
# Make sure you include the generated file as a dependency to a target
# (e.g. libpromises_la) so that it's generated during "make dist"

BUILT_SOURCES = cf3lex.c cf3parse.h cf3parse.c \
	enterprise_extension.c enterprise_extension.h \
	bootstrap.inc
EXTRA_SCRIPTS = text2cstring.pl enterprise_extension.sed
EXTRA_DIST = $(EXTRA_SCRIPTS)

enterprise_extension.c: extensions_template.c.pre enterprise_extension.sed
	$(V_SED) $(SED) -f $(srcdir)/enterprise_extension.sed $< > $@
enterprise_extension.h: extensions_template.h.pre enterprise_extension.sed
	$(V_SED) $(SED) -f $(srcdir)/enterprise_extension.sed $< > $@

bootstrap.inc: failsafe.cf text2cstring.pl
	$(V_PERL) $(PERL) $(srcdir)/text2cstring.pl $< > $@


#
# Proper compilation progress printing
#
V_PERL = $(cf__v_PERL_$(V))
cf__v_PERL_ = $(cf__v_PERL_$(AM_DEFAULT_VERBOSITY))
cf__v_PERL_0 = @echo "  PERL    " "$@";
cf__v_PERL_1 =
V_SED = $(cf__v_SED_$(V))
cf__v_SED_ = $(cf__v_SED_$(AM_DEFAULT_VERBOSITY))
cf__v_SED_0 = @echo "  SED     " "$@";
cf__v_SED_1 =


#
# Some basic clean ups
#
# Use "make mostlyclean" if you want to keep the BUILT_SOURCES
#
CLEANFILES = $(BUILT_SOURCES)
MOSTLYCLEANFILES =  *.gcno *.gcda *~ *.orig *.rej
