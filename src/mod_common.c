/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/* This is a root node in the syntax tree */

#include "cf3.defs.h"

#include "mod_environ.h"
#include "mod_outputs.h"
#include "mod_access.h"
#include "mod_interfaces.h"
#include "mod_storage.h"
#include "mod_databases.h"
#include "mod_knowledge.h"
#include "mod_packages.h"
#include "mod_report.h"
#include "mod_files.h"
#include "mod_exec.h"
#include "mod_methods.h"
#include "mod_process.h"
#include "mod_services.h"
#include "mod_measurement.h"

static const BodySyntax CF_TRANSACTION_BODY[] =
{
    {"action_policy", cf_opts, "fix,warn,nop", "Whether to repair or report about non-kept promises"},
    {"ifelapsed", cf_int, CF_VALRANGE, "Number of minutes before next allowed assessment of promise",
     "control body value"},
    {"expireafter", cf_int, CF_VALRANGE, "Number of minutes before a repair action is interrupted and retried",
     "control body value"},
    {"log_string", cf_str, "", "A message to be written to the log when a promise verification leads to a repair"},
    {"log_level", cf_opts, "inform,verbose,error,log", "The reporting level sent to syslog"},
    {"log_kept", cf_str, CF_LOGRANGE,
     "This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
    {"log_priority", cf_opts, "emergency,alert,critical,error,warning,notice,info,debug",
     "The priority level of the log message, as interpreted by a syslog server"},
    {"log_repaired", cf_str, CF_LOGRANGE,
     "This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
    {"log_failed", cf_str, CF_LOGRANGE,
     "This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
    {"value_kept", cf_real, CF_REALRANGE, "A real number value attributed to keeping this promise"},
    {"value_repaired", cf_real, CF_REALRANGE, "A real number value attributed to reparing this promise"},
    {"value_notkept", cf_real, CF_REALRANGE,
     "A real number value (possibly negative) attributed to not keeping this promise"},
    {"audit", cf_opts, CF_BOOL, "true/false switch for detailed audit records of this promise", "false"},
    {"background", cf_opts, CF_BOOL, "true/false switch for parallelizing the promise repair", "false"},
    {"report_level", cf_opts, "inform,verbose,error,log", "The reporting level for standard output for this promise",
     "none"},
    {"measurement_class", cf_str, "", "If set performance will be measured and recorded under this identifier"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_DEFINECLASS_BODY[] =
{
    {"promise_repaired", cf_slist, CF_IDRANGE, "A list of classes to be defined globally"},
    {"repair_failed", cf_slist, CF_IDRANGE, "A list of classes to be defined globally"},
    {"repair_denied", cf_slist, CF_IDRANGE, "A list of classes to be defined globally"},
    {"repair_timeout", cf_slist, CF_IDRANGE, "A list of classes to be defined globally"},
    {"promise_kept", cf_slist, CF_IDRANGE, "A list of classes to be defined globally"},
    {"cancel_kept", cf_slist, CF_IDRANGE, "A list of classes to be cancelled if the promise is kept"},
    {"cancel_repaired", cf_slist, CF_IDRANGE, "A list of classes to be cancelled if the promise is repaired"},
    {"cancel_notkept", cf_slist, CF_IDRANGE,
     "A list of classes to be cancelled if the promise is not kept for any reason"},
    {"kept_returncodes", cf_slist, CF_INTLISTRANGE, "A list of return codes indicating a kept command-related promise"},
    {"repaired_returncodes", cf_slist, CF_INTLISTRANGE,
     "A list of return codes indicating a repaired command-related promise"},
    {"failed_returncodes", cf_slist, CF_INTLISTRANGE,
     "A list of return codes indicating a failed command-related promise"},
    {"persist_time", cf_int, CF_VALRANGE, "A number of minutes the specified classes should remain active"},
    {"timer_policy", cf_opts, "absolute,reset", "Whether a persistent class restarts its counter when rediscovered",
     "reset"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CF_VARBODY[] =
{
    {"string", cf_str, "", "A scalar string"},
    {"int", cf_int, CF_INTRANGE, "A scalar integer"},
    {"real", cf_real, CF_REALRANGE, "A scalar real number"},
    {"slist", cf_slist, "", "A list of scalar strings"},
    {"ilist", cf_ilist, CF_INTRANGE, "A list of integers"},
    {"rlist", cf_rlist, CF_REALRANGE, "A list of real numbers"},
    {"policy", cf_opts, "free,overridable,constant,ifdefined",
     "The policy for (dis)allowing (re)definition of variables"},
    {NULL, cf_notype, NULL, NULL}
};


const BodySyntax CF_METABODY[] =
{
    {"string", cf_str, "", "A scalar string"},
    {"slist", cf_slist, "", "A list of scalar strings"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CF_DEFAULTSBODY[] =
{
    {"if_match_regex", cf_str, "", "If this regular expression matches the current value of the variable, replace it with default"},
    {"string", cf_str, "", "A scalar string"},
    {"slist", cf_slist, "", "A list of scalar strings"},
    {NULL, cf_notype, NULL, NULL}
};


const BodySyntax CF_CLASSBODY[] =
{
    {"and", cf_clist, CF_CLASSRANGE, "Combine class sources with AND"},
    {"dist", cf_rlist, CF_REALRANGE, "Generate a probabilistic class distribution (from strategies in cfengine 2)"},
    {"expression", cf_class, CF_CLASSRANGE, "Evaluate string expression of classes in normal form"},
    {"or", cf_clist, CF_CLASSRANGE, "Combine class sources with inclusive OR"},
    {"persistence", cf_int, CF_VALRANGE, "Make the class persistent (cached) to avoid reevaluation, time in minutes"},
    {"not", cf_class, CF_CLASSRANGE, "Evaluate the negation of string expression in normal form"},
    {"select_class", cf_clist, CF_CLASSRANGE,
     "Select one of the named list of classes to define based on host identity", "random_selection"},
    {"xor", cf_clist, CF_CLASSRANGE, "Combine class sources with XOR"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFG_CONTROLBODY[] =
{
    {"bundlesequence", cf_slist, ".*", "List of promise bundles to verify in order"},
    {"goal_patterns", cf_slist, "",
     "A list of regular expressions that match promisees/topics considered to be organizational goals"},
    {"ignore_missing_bundles", cf_opts, CF_BOOL,
     "If any bundles in the bundlesequence do not exist, ignore and continue", "false"},
    {"ignore_missing_inputs", cf_opts, CF_BOOL, "If any input files do not exist, ignore and continue", "false"},
    {"inputs", cf_slist, ".*", "List of additional filenames to parse for promises"},
    {"version", cf_str, "", "Scalar version string for this configuration"},
    {"lastseenexpireafter", cf_int, CF_VALRANGE, "Number of minutes after which last-seen entries are purged",
     "One week"},
    {"output_prefix", cf_str, "", "The string prefix for standard output"},
    {"domain", cf_str, ".*", "Specify the domain name for this host"},
    {"require_comments", cf_opts, CF_BOOL, "Warn about promises that do not have comment documentation", "false"},
    {"host_licenses_paid", cf_int, CF_VALRANGE,
     "The number of licenses that you promise to have paid for by setting this value (legally binding for commercial license)",
     "0"},
    {"site_classes", cf_clist, CF_CLASSRANGE,
     "A list of classes that will represent geographical site locations for hosts. These should be defined elsewhere in the configuration in a classes promise."},
    {"syslog_host", cf_str, CF_IPRANGE,
     "The name or address of a host to which syslog messages should be sent directly by UDP", "514"},
    {"syslog_port", cf_int, CF_VALRANGE, "The port number of a UDP syslog service"},
    {"fips_mode", cf_opts, CF_BOOL, "Activate full FIPS mode restrictions", "false"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFA_CONTROLBODY[] =
{
    {"abortclasses", cf_slist, ".*", "A list of classes which if defined lead to termination of cf-agent"},
    {"abortbundleclasses", cf_slist, ".*", "A list of classes which if defined lead to termination of current bundle"},
    {"addclasses", cf_slist, ".*", "A list of classes to be defined always in the current context"},
    {"agentaccess", cf_slist, ".*", "A list of user names allowed to execute cf-agent"},
    {"agentfacility", cf_opts, CF_FACILITY, "The syslog facility for cf-agent", "LOG_USER"},
    {"allclassesreport", cf_opts, CF_BOOL, "Generate allclasses.txt report"},
    {"alwaysvalidate", cf_opts, CF_BOOL,
     "true/false flag to determine whether configurations will always be checked before executing, or only after updates"},
    {"auditing", cf_opts, CF_BOOL, "true/false flag to activate the cf-agent audit log", "false"},
    {"binarypaddingchar", cf_str, "", "Character used to pad unequal replacements in binary editing", "space (ASC=32)"},
    {"bindtointerface", cf_str, ".*", "Use this interface for outgoing connections"},
    {"hashupdates", cf_opts, CF_BOOL, "true/false whether stored hashes are updated when change is detected in source",
     "false"},
    {"childlibpath", cf_str, ".*", "LD_LIBRARY_PATH for child processes"},
    {"checksum_alert_time", cf_int, "0,60", "The persistence time for the checksum_alert class", "10 mins"},
    {"defaultcopytype", cf_opts, "mtime,atime,ctime,digest,hash,binary", "ctime or mtime differ"},
    {"dryrun", cf_opts, CF_BOOL, "All talk and no action mode", "false"},
    {"editbinaryfilesize", cf_int, CF_VALRANGE, "Integer limit on maximum binary file size to be edited", "100000"},
    {"editfilesize", cf_int, CF_VALRANGE, "Integer limit on maximum text file size to be edited", "100000"},
    {"environment", cf_slist, "[A-Za-z0-9_]+=.*", "List of environment variables to be inherited by children"},
    {"exclamation", cf_opts, CF_BOOL, "true/false print exclamation marks during security warnings", "true"},
    {"expireafter", cf_int, CF_VALRANGE, "Global default for time before on-going promise repairs are interrupted",
     "1 min"},
    {"files_single_copy", cf_slist, "", "List of filenames to be watched for multiple-source conflicts"},
    {"files_auto_define", cf_slist, "", "List of filenames to define classes if copied"},
    {"hostnamekeys", cf_opts, CF_BOOL, "true/false label ppkeys by hostname not IP address", "false"},
    {"ifelapsed", cf_int, CF_VALRANGE, "Global default for time that must elapse before promise will be rechecked",
     "1"},
    {"inform", cf_opts, CF_BOOL, "true/false set inform level default", "false"},
    {"intermittency", cf_opts, CF_BOOL,
     "This option is deprecated, does nothing and is kept for backward compatibility",
     "false"},
    {"max_children", cf_int, CF_VALRANGE, "Maximum number of background tasks that should be allowed concurrently",
     "1 concurrent agent promise"},
    {"maxconnections", cf_int, CF_VALRANGE, "Maximum number of outgoing connections to cf-serverd",
     "30 remote queries"},
    {"mountfilesystems", cf_opts, CF_BOOL, "true/false mount any filesystems promised", "false"},
    {"nonalphanumfiles", cf_opts, CF_BOOL, "true/false warn about filenames with no alphanumeric content", "false"},
    {"repchar", cf_str, ".", "The character used to canonize pathnames in the file repository", "_"},
    {"refresh_processes", cf_slist, CF_IDRANGE,
     "Reload the process table before verifying the bundles named in this list (lazy evaluation)"},
    {"default_repository", cf_str, CF_ABSPATHRANGE, "Path to the default file repository", "in situ"},
    {"secureinput", cf_opts, CF_BOOL, "true/false check whether input files are writable by unauthorized users",
     "false"},
    {"sensiblecount", cf_int, CF_VALRANGE, "Minimum number of files a mounted filesystem is expected to have",
     "2 files"},
    {"sensiblesize", cf_int, CF_VALRANGE, "Minimum number of bytes a mounted filesystem is expected to have",
     "1000 bytes"},
    {"skipidentify", cf_opts, CF_BOOL,
     "Do not send IP/name during server connection because address resolution is broken", "false"},
    {"suspiciousnames", cf_slist, "", "List of names to warn about if found during any file search"},
    {"syslog", cf_opts, CF_BOOL, "true/false switches on output to syslog at the inform level", "false"},
    {"track_value", cf_opts, CF_BOOL, "true/false switches on tracking of promise valuation", "false"},
    {"timezone", cf_slist, "", "List of allowed timezones this machine must comply with"},
    {"default_timeout", cf_int, CF_VALRANGE, "Maximum time a network connection should attempt to connect",
     "10 seconds"},
    {"verbose", cf_opts, CF_BOOL, "true/false switches on verbose standard output", "false"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFS_CONTROLBODY[] =
{
    {"allowallconnects", cf_slist, "",
     "List of IPs or hostnames that may have more than one connection to the server port"},
    {"allowconnects", cf_slist, "", "List of IPs or hostnames that may connect to the server port"},
    {"allowusers", cf_slist, "", "List of usernames who may execute requests from this server"},
    {"auditing", cf_opts, CF_BOOL, "true/false activate auditing of server connections", "false"},
    {"bindtointerface", cf_str, "", "IP of the interface to which the server should bind on multi-homed hosts"},
    {"cfruncommand", cf_str, CF_PATHRANGE, "Path to the cf-agent command or cf-execd wrapper for remote execution"},
    {"call_collect_interval", cf_int, CF_VALRANGE, "The interval in minutes in between collect calls to the policy hub offering a tunnel for report collection (Enterprise)"},
    {"collect_window", cf_int, CF_VALRANGE, "A time in seconds that a collect-call tunnel remains open to a hub to attempt a report transfer before it is closed (Enterprise)"},
    {"denybadclocks", cf_opts, CF_BOOL, "true/false accept connections from hosts with clocks that are out of sync",
     "true"},
    {"denyconnects", cf_slist, "", "List of IPs or hostnames that may NOT connect to the server port"},
    {"dynamicaddresses", cf_slist, "", "List of IPs or hostnames for which the IP/name binding is expected to change"},
    {"hostnamekeys", cf_opts, CF_BOOL, "true/false store keys using hostname lookup instead of IP addresses", "false"},
    {"keycacheTTL", cf_int, CF_VALRANGE, "Maximum number of hours to hold public keys in the cache", "24"},
    {"logallconnections", cf_opts, CF_BOOL, "true/false causes the server to log all new connections to syslog",
     "false"},
    {"logencryptedtransfers", cf_opts, CF_BOOL, "true/false log all successful transfers required to be encrypted",
     "false"},
    {"maxconnections", cf_int, CF_VALRANGE, "Maximum number of connections that will be accepted by cf-serverd",
     "30 remote queries"},
    {"port", cf_int, "1024,99999", "Default port for cfengine server", "5308"},
    {"serverfacility", cf_opts, CF_FACILITY, "Menu option for syslog facility level", "LOG_USER"},
    {"skipverify", cf_slist, "", "List of IPs or hostnames for which we expect no DNS binding and cannot verify"},
    {"trustkeysfrom", cf_slist, "", "List of IPs from whom we accept public keys on trust"},
    {"listen", cf_opts, CF_BOOL, "true/false enable server deamon to listen on defined port", "true"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFM_CONTROLBODY[] =
{
    {"forgetrate", cf_real, "0,1", "Decimal fraction [0,1] weighting of new values over old in 2d-average computation",
     "0.6"},
    {"monitorfacility", cf_opts, CF_FACILITY, "Menu option for syslog facility", "LOG_USER"},
    {"histograms", cf_opts, CF_BOOL, "Ignored, kept for backward compatibility", "true"},
    {"tcpdump", cf_opts, CF_BOOL, "true/false use tcpdump if found", "false"},
    {"tcpdumpcommand", cf_str, CF_ABSPATHRANGE, "Path to the tcpdump command on this system"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFR_CONTROLBODY[] =
{
    {"hosts", cf_slist, "", "List of host or IP addresses to attempt connection with"},
    {"port", cf_int, "1024,99999", "Default port for cfengine server", "5308"},
    {"force_ipv4", cf_opts, CF_BOOL, "true/false force use of ipv4 in connection", "false"},
    {"trustkey", cf_opts, CF_BOOL, "true/false automatically accept all keys on trust from servers", "false"},
    {"encrypt", cf_opts, CF_BOOL, "true/false encrypt connections with servers", "false"},
    {"background_children", cf_opts, CF_BOOL, "true/false parallelize connections to servers", "false"},
    {"max_children", cf_int, CF_VALRANGE, "Maximum number of simultaneous connections to attempt", "50 runagents"},
    {"output_to_file", cf_opts, CF_BOOL, "true/false whether to send collected output to file(s)", "false"},
    {"output_directory", cf_str, CF_ABSPATHRANGE, "Directory where the output is stored"},
    {"timeout", cf_int, "1,9999", "Connection timeout, sec"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFEX_CONTROLBODY[] = /* enum cfexcontrol */
{
    {"splaytime", cf_int, CF_VALRANGE, "Time in minutes to splay this host based on its name hash", "0"},
    {"mailfrom", cf_str, ".*@.*", "Email-address cfengine mail appears to come from"},
    {"mailto", cf_str, ".*@.*", "Email-address cfengine mail is sent to"},
    {"smtpserver", cf_str, ".*", "Name or IP of a willing smtp server for sending email"},
    {"mailmaxlines", cf_int, "0,1000", "Maximum number of lines of output to send by email", "30"},
    {"schedule", cf_slist, "", "The class schedule used by cf-execd for activating cf-agent"},
    {"executorfacility", cf_opts, CF_FACILITY, "Menu option for syslog facility level", "LOG_USER"},
    {"exec_command", cf_str, CF_ABSPATHRANGE,
     "The full path and command to the executable run by default (overriding builtin)"},
    {"agent_expireafter", cf_int, "0,10080", "Maximum agent runtime (in minutes)", "10080"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFK_CONTROLBODY[] =
{
    {"build_directory", cf_str, ".*", "The directory in which to generate output files", "Current working directory"},
    {"document_root", cf_str, ".*", "The directory in which the web root resides"},
    {"generate_manual", cf_opts, CF_BOOL, "true/false generate texinfo manual page skeleton for this version", "false"},
    {"graph_directory", cf_str, CF_ABSPATHRANGE, "Path to directory where rendered .png files will be created"},
    {"graph_output", cf_opts, CF_BOOL, "true/false generate png visualization of topic map if possible (requires lib)"},
    {"html_banner", cf_str, "", "HTML code for a banner to be added to rendered in html after the header"},
    {"html_footer", cf_str, "", "HTML code for a page footer to be added to rendered in html before the end body tag"},
    {"id_prefix", cf_str, ".*",
     "The LTM identifier prefix used to label topic maps (used for disambiguation in merging)"},
    {"manual_source_directory", cf_str, CF_ABSPATHRANGE,
     "Path to directory where raw text about manual topics is found (defaults to build_directory)"},
    {"query_engine", cf_str, "", "Name of a dynamic web-page used to accept and drive queries in a browser"},
    {"query_output", cf_opts, "html,text", "Menu option for generated output format"},
    {"sql_type", cf_opts, "mysql,postgres", "Menu option for supported database type"},
    {"sql_database", cf_str, "", "Name of database used for the topic map"},
    {"sql_owner", cf_str, "", "User id of sql database user"},
    {"sql_passwd", cf_str, "", "Embedded password for accessing sql database"},
    {"sql_server", cf_str, "", "Name or IP of database server (or localhost)"},
    {"sql_connection_db", cf_str, "",
     "The name of an existing database to connect to in order to create/manage other databases"},
    {"style_sheet", cf_str, "", "Name of a style-sheet to be used in rendering html output (added to headers)"},
    {"view_projections", cf_opts, CF_BOOL, "Perform view-projection analytics in graph generation", "false"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFRE_CONTROLBODY[] = /* enum cfrecontrol */
{
    {"aggregation_point", cf_str, CF_ABSPATHRANGE, "The root directory of the data cache for CMDB aggregation"},
    {"auto_scaling", cf_opts, CF_BOOL, "true/false whether to auto-scale graph output to optimize use of space",
     "true"},
    {"build_directory", cf_str, ".*", "The directory in which to generate output files", "Current working directory"},
    {"csv2xml", cf_slist, "", "A list of csv formatted files in the build directory to convert to simple xml"},
    {"error_bars", cf_opts, CF_BOOL, "true/false whether to generate error bars on graph output", "true"},
    {"html_banner", cf_str, "", "HTML code for a banner to be added to rendered in html after the header"},
    {"html_embed", cf_opts, CF_BOOL, "If true, no header and footer tags will be added to html output"},
    {"html_footer", cf_str, "", "HTML code for a page footer to be added to rendered in html before the end body tag"},
    {"query_engine", cf_str, "", "Name of a dynamic web-page used to accept and drive queries in a browser"},
    {"reports", cf_olist,
     "all,audit,performance,all_locks,active_locks,hashes,classes,last_seen,monitor_now,monitor_history,monitor_summary,compliance,setuid,file_changes,installed_software,software_patches,value,variables",
     "A list of reports that may be generated", "none"},
    {"report_output", cf_opts, "csv,html,text,xml",
     "Menu option for generated output format. Applies only to text reports, graph data remain in xydy format.",
     "none"},
    {"style_sheet", cf_str, "", "Name of a style-sheet to be used in rendering html output (added to headers)"},
    {"time_stamps", cf_opts, CF_BOOL, "true/false whether to generate timestamps in the output directory name",
     "false"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFH_CONTROLBODY[] =  /* enum cfh_control */
{
    {"export_zenoss", cf_str, CF_PATHRANGE, "Generate report for Zenoss integration"},
    {"exclude_hosts", cf_slist, "", "A list of IP addresses of hosts to exclude from report collection"},
    {"hub_schedule", cf_slist, "", "The class schedule used by cf-hub for report collation"},
    {"port", cf_int, "1024,99999", "Default port for contacting hub nodes", "5308"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CFFILE_CONTROLBODY[] =  /* enum cfh_control */
{
    {"namespace", cf_str, CF_IDRANGE, "Switch to a private namespace to protect current file from duplicate definitions"},
    {NULL, cf_notype, NULL, NULL}
};

/* This list is for checking free standing body lval => rval bindings */

const SubTypeSyntax CF_ALL_BODIES[] =
{
    {CF_COMMONC, "control", CFG_CONTROLBODY},
    {CF_AGENTC, "control", CFA_CONTROLBODY},
    {CF_SERVERC, "control", CFS_CONTROLBODY},
    {CF_MONITORC, "control", CFM_CONTROLBODY},
    {CF_RUNC, "control", CFR_CONTROLBODY},
    {CF_EXECC, "control", CFEX_CONTROLBODY},
    {CF_KNOWC, "control", CFK_CONTROLBODY},
    {CF_REPORTC, "control", CFRE_CONTROLBODY},
    {CF_HUBC, "control", CFH_CONTROLBODY},
    {"file", "control", CFFILE_CONTROLBODY},

    //  get others from modules e.g. "agent","files",CF_FILES_BODIES,

    {NULL, NULL, NULL}
};

/*********************************************************/
/*                                                       */
/* Constraint values/types                               */
/*                                                       */
/*********************************************************/

 /* This is where we place lval => rval bindings that
    apply to more than one subtype, e.g. generic
    processing behavioural details */

const BodySyntax CF_COMMON_BODIES[] =
{
    {CF_TRANSACTION, cf_body, CF_TRANSACTION_BODY, "Output behaviour"},
    {CF_DEFINECLASSES, cf_body, CF_DEFINECLASS_BODY, "Signalling behaviour"},
    {"comment", cf_str, "", "A comment about this promise's real intention that follows through the program"},
    {"depends_on", cf_slist, "","A list of promise handles that this promise builds on or depends on somehow (for knowledge management)"},
    {"handle", cf_str, "", "A unique id-tag string for referring to this as a promisee elsewhere"},
    {"ifvarclass", cf_str, "", "Extended classes ANDed with context"},
    {"meta", cf_slist, "", "User-data associated with policy, e.g. key=value strings"},
    {NULL, cf_notype, NULL, NULL}
};

 /* This is where we place promise subtypes that apply
    to more than one type of bundle, e.g. agent,server.. */

const SubTypeSyntax CF_COMMON_SUBTYPES[] =
{

    {"*", "classes", CF_CLASSBODY},
    {"*", "defaults", CF_DEFAULTSBODY},
    {"*", "meta", CF_METABODY},
    {"*", "reports", CF_REPORT_BODIES},
    {"*", "vars", CF_VARBODY},
    {"*", "*", CF_COMMON_BODIES},
    {NULL, NULL, NULL}
};

/*********************************************************/
/* THIS IS WHERE TO ATTACH SYNTAX MODULES                */
/*********************************************************/

/* Read in all parsable Bundle definitions */
/* REMEMBER TO REGISTER THESE IN cf3.extern.h */

const SubTypeSyntax *CF_ALL_SUBTYPES[] =
{
    CF_COMMON_SUBTYPES,         /* Add modules after this, mod_report.c is here */
    CF_EXEC_SUBTYPES,           /* mod_exec.c */
    CF_DATABASES_SUBTYPES,      /* mod_databases.c */
    CF_ENVIRONMENT_SUBTYPES,    /* mod_environ.c */
    CF_FILES_SUBTYPES,          /* mod_files.c */
    CF_INTERFACES_SUBTYPES,     /* mod_interfaces.c */
    CF_METHOD_SUBTYPES,         /* mod_methods.c */
    CF_OUTPUTS_SUBTYPES,        /* mod_outputs.c */
    CF_PACKAGES_SUBTYPES,       /* mod_packages.c */
    CF_PROCESS_SUBTYPES,        /* mod_process.c */
    CF_SERVICES_SUBTYPES,       /* mod_services.c */
    CF_STORAGE_SUBTYPES,        /* mod_storage.c */
    CF_REMACCESS_SUBTYPES,      /* mod_access.c */
    CF_KNOWLEDGE_SUBTYPES,      /* mod_knowledge.c */
    CF_MEASUREMENT_SUBTYPES,    /* mod_measurement.c */
};

const int CF3_MODULES = (sizeof(CF_ALL_SUBTYPES) / sizeof(CF_ALL_SUBTYPES[0]));
