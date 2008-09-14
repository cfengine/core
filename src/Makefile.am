AM_CFLAGS = $(GCC_CFLAG) @CFLAGS@  
AM_YFLAGS = -d
LEXLIB = @LEXLIB@
LDADD = $(BERKELEY_DB_LDFLAGSD) $(BERKELEY_DB_LIB) $(OPENSSL_LIB_LDFLAGS) $(OPENSSL_LIB_LIB) libpromises.la 
INCLUDES = $(BERKELEY_DB_CFLAGS) $(OPENSSL_LIB_CPPFLAGS)
AM_LDFLAGS = $(BERKELEY_DB_LDFLAGS) $(OPENSSL_LIB_LDFLAGS) @LDFLAGS@

lib_LTLIBRARIES = libpromises.la

libpromises_la_LDFLAGS = -version-info 1:0:0 -no-undefined

# Separate out a library

libpromises_la_SOURCES =						\
	cf3parse.y							\
	cf3lex.l							\
	attributes.c							\
	install.c							\
	generic_agent.c							\
	env_context.c							\
	files_interfaces.c						\
	files_properties.c						\
	files_select.c							\
	files_operators.c						\
	files_repository.c						\
	files_copy.c							\
	files_editline.c						\
	files_edit.c							\
	files_links.c							\
	files_hashes.c							\
	item-lib.c							\
	cf_sql.c							\
	client_protocol.c						\
	client_code.c							\
	recursion.c							\
	processes_select.c						\
	fncall.c							\
	cf3globals.c							\
	report.c							\
	evalfunction.c							\
	sysinfo.c							\
	conversion.c							\
	scope.c								\
	assoc.c								\
	vars.c								\
	args.c								\
	hashes.c							\
	expand.c							\
	matching.c							\
	selfdiagnostic.c						\
	instrumentation.c						\
	promises.c							\
	constraints.c							\
	iteration.c							\
	rlist.c								\
	syntax.c							\
	logging.c							\
	signals.c							\
	transaction.c							\
	cfstream.c							\
	pipes.c								\
	html.c								\
	mod_common.c							\
	mod_access.c							\
	mod_exec.c							\
	mod_files.c							\
	mod_report.c							\
	mod_storage.c							\
	mod_knowledge.c							\
	mod_process.c


sbin_PROGRAMS = cf-know cf-promises cf-agent cf-monitord cf-serverd cf-execd cf-runagent

cf_promises_SOURCES =							\
	cfpromises.c

cf_agent_SOURCES =							\
	agent.c								\
	verify_files.c							\
	verify_processes.c						\
	verify_exec.c							\
	verify_reports.c						\
	agentdiagnostic.c

cf_serverd_SOURCES =							\
	server.c							\
	server_transform.c

cf_monitord_SOURCES =							\
	env_monitor.c							\
	monitor.c

cf_execd_SOURCES =							\
	exec.c

cf_runagent_SOURCES =							\
	runagent.c

cf_know_SOURCES =							\
	verify_reports.c						\
	cfknow.c							\
	ontology.c

EXTRA_DIST = cf3lex.c cf3parse.c

#
# Pickup header files here so Automake knows about them
#
noinst_HEADERS =							\
	cf.defs.h							\
	prototypes.h							\
	cf.extern.h							\
	prototypes3.h							\
	cf3.defs.h							\
	cf3.server.h							\
	cf3parse.h							\
	cf3.extern.h

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
