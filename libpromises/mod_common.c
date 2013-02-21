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
    {"action_policy", DATA_TYPE_OPTION, "fix,warn,nop", "Whether to repair or report about non-kept promises"},
    {"ifelapsed", DATA_TYPE_INT, CF_VALRANGE, "Number of minutes before next allowed assessment of promise",
     "control body value"},
    {"expireafter", DATA_TYPE_INT, CF_VALRANGE, "Number of minutes before a repair action is interrupted and retried",
     "control body value"},
    {"log_string", DATA_TYPE_STRING, "", "A message to be written to the log when a promise verification leads to a repair"},
    {"log_level", DATA_TYPE_OPTION, "inform,verbose,error,log", "The reporting level sent to syslog"},
    {"log_kept", DATA_TYPE_STRING, CF_LOGRANGE,
     "This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
    {"log_priority", DATA_TYPE_OPTION, "emergency,alert,critical,error,warning,notice,info,debug",
     "The priority level of the log message, as interpreted by a syslog server"},
    {"log_repaired", DATA_TYPE_STRING, CF_LOGRANGE,
     "This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
    {"log_failed", DATA_TYPE_STRING, CF_LOGRANGE,
     "This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
    {"value_kept", DATA_TYPE_REAL, CF_REALRANGE, "A real number value attributed to keeping this promise"},
    {"value_repaired", DATA_TYPE_REAL, CF_REALRANGE, "A real number value attributed to reparing this promise"},
    {"value_notkept", DATA_TYPE_REAL, CF_REALRANGE,
     "A real number value (possibly negative) attributed to not keeping this promise"},
    {"audit", DATA_TYPE_OPTION, CF_BOOL, "true/false switch for detailed audit records of this promise", "false"},
    {"background", DATA_TYPE_OPTION, CF_BOOL, "true/false switch for parallelizing the promise repair", "false"},
    {"report_level", DATA_TYPE_OPTION, "inform,verbose,error,log", "The reporting level for standard output for this promise",
     "none"},
    {"measurement_class", DATA_TYPE_STRING, "", "If set performance will be measured and recorded under this identifier"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_DEFINECLASS_BODY[] =
{
    {"promise_repaired", DATA_TYPE_STRING_LIST, CF_IDRANGE, "A list of classes to be defined globally"},
    {"repair_failed", DATA_TYPE_STRING_LIST, CF_IDRANGE, "A list of classes to be defined globally"},
    {"repair_denied", DATA_TYPE_STRING_LIST, CF_IDRANGE, "A list of classes to be defined globally"},
    {"repair_timeout", DATA_TYPE_STRING_LIST, CF_IDRANGE, "A list of classes to be defined globally"},
    {"promise_kept", DATA_TYPE_STRING_LIST, CF_IDRANGE, "A list of classes to be defined globally"},
    {"cancel_kept", DATA_TYPE_STRING_LIST, CF_IDRANGE, "A list of classes to be cancelled if the promise is kept"},
    {"cancel_repaired", DATA_TYPE_STRING_LIST, CF_IDRANGE, "A list of classes to be cancelled if the promise is repaired"},
    {"cancel_notkept", DATA_TYPE_STRING_LIST, CF_IDRANGE,
     "A list of classes to be cancelled if the promise is not kept for any reason"},
    {"kept_returncodes", DATA_TYPE_STRING_LIST, CF_INTLISTRANGE, "A list of return codes indicating a kept command-related promise"},
    {"repaired_returncodes", DATA_TYPE_STRING_LIST, CF_INTLISTRANGE,
     "A list of return codes indicating a repaired command-related promise"},
    {"failed_returncodes", DATA_TYPE_STRING_LIST, CF_INTLISTRANGE,
     "A list of return codes indicating a failed command-related promise"},
    {"persist_time", DATA_TYPE_INT, CF_VALRANGE, "A number of minutes the specified classes should remain active"},
    {"timer_policy", DATA_TYPE_OPTION, "absolute,reset", "Whether a persistent class restarts its counter when rediscovered",
     "reset"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CF_VARBODY[] =
{
    {"string", DATA_TYPE_STRING, "", "A scalar string"},
    {"int", DATA_TYPE_INT, CF_INTRANGE, "A scalar integer"},
    {"real", DATA_TYPE_REAL, CF_REALRANGE, "A scalar real number"},
    {"slist", DATA_TYPE_STRING_LIST, "", "A list of scalar strings"},
    {"ilist", DATA_TYPE_INT_LIST, CF_INTRANGE, "A list of integers"},
    {"rlist", DATA_TYPE_REAL_LIST, CF_REALRANGE, "A list of real numbers"},
    {"policy", DATA_TYPE_OPTION, "free,overridable,constant,ifdefined",
     "The policy for (dis)allowing (re)definition of variables"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};


const BodySyntax CF_METABODY[] =
{
    {"string", DATA_TYPE_STRING, "", "A scalar string"},
    {"slist", DATA_TYPE_STRING_LIST, "", "A list of scalar strings"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CF_DEFAULTSBODY[] =
{
    {"if_match_regex", DATA_TYPE_STRING, "", "If this regular expression matches the current value of the variable, replace it with default"},
    {"string", DATA_TYPE_STRING, "", "A scalar string"},
    {"slist", DATA_TYPE_STRING_LIST, "", "A list of scalar strings"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};


const BodySyntax CF_CLASSBODY[] =
{
    {"and", DATA_TYPE_CONTEXT_LIST, CF_CLASSRANGE, "Combine class sources with AND"},
    {"dist", DATA_TYPE_REAL_LIST, CF_REALRANGE, "Generate a probabilistic class distribution (from strategies in cfengine 2)"},
    {"expression", DATA_TYPE_CONTEXT, CF_CLASSRANGE, "Evaluate string expression of classes in normal form"},
    {"or", DATA_TYPE_CONTEXT_LIST, CF_CLASSRANGE, "Combine class sources with inclusive OR"},
    {"persistence", DATA_TYPE_INT, CF_VALRANGE, "Make the class persistent (cached) to avoid reevaluation, time in minutes"},
    {"not", DATA_TYPE_CONTEXT, CF_CLASSRANGE, "Evaluate the negation of string expression in normal form"},
    {"select_class", DATA_TYPE_CONTEXT_LIST, CF_CLASSRANGE,
     "Select one of the named list of classes to define based on host identity", "random_selection"},
    {"xor", DATA_TYPE_CONTEXT_LIST, CF_CLASSRANGE, "Combine class sources with XOR"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFG_CONTROLBODY[] =
{
    {"bundlesequence", DATA_TYPE_STRING_LIST, ".*", "List of promise bundles to verify in order"},
    {"goal_patterns", DATA_TYPE_STRING_LIST, "",
     "A list of regular expressions that match promisees/topics considered to be organizational goals"},
    {"ignore_missing_bundles", DATA_TYPE_OPTION, CF_BOOL,
     "If any bundles in the bundlesequence do not exist, ignore and continue", "false"},
    {"ignore_missing_inputs", DATA_TYPE_OPTION, CF_BOOL, "If any input files do not exist, ignore and continue", "false"},
    {"inputs", DATA_TYPE_STRING_LIST, ".*", "List of additional filenames to parse for promises"},
    {"version", DATA_TYPE_STRING, "", "Scalar version string for this configuration"},
    {"lastseenexpireafter", DATA_TYPE_INT, CF_VALRANGE, "Number of minutes after which last-seen entries are purged",
     "One week"},
    {"output_prefix", DATA_TYPE_STRING, "", "The string prefix for standard output"},
    {"domain", DATA_TYPE_STRING, ".*", "Specify the domain name for this host"},
    {"require_comments", DATA_TYPE_OPTION, CF_BOOL, "Warn about promises that do not have comment documentation", "false"},
    {"host_licenses_paid", DATA_TYPE_INT, CF_VALRANGE,
     "The number of licenses that you promise to have paid for by setting this value (legally binding for commercial license)",
     "25"},
    {"site_classes", DATA_TYPE_CONTEXT_LIST, CF_CLASSRANGE,
     "A list of classes that will represent geographical site locations for hosts. These should be defined elsewhere in the configuration in a classes promise."},
    {"syslog_host", DATA_TYPE_STRING, CF_IPRANGE,
     "The name or address of a host to which syslog messages should be sent directly by UDP", "514"},
    {"syslog_port", DATA_TYPE_INT, CF_VALRANGE, "The port number of a UDP syslog service"},
    {"fips_mode", DATA_TYPE_OPTION, CF_BOOL, "Activate full FIPS mode restrictions", "false"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFA_CONTROLBODY[] =
{
    {"abortclasses", DATA_TYPE_STRING_LIST, ".*", "A list of classes which if defined lead to termination of cf-agent"},
    {"abortbundleclasses", DATA_TYPE_STRING_LIST, ".*", "A list of classes which if defined lead to termination of current bundle"},
    {"addclasses", DATA_TYPE_STRING_LIST, ".*", "A list of classes to be defined always in the current context"},
    {"agentaccess", DATA_TYPE_STRING_LIST, ".*", "A list of user names allowed to execute cf-agent"},
    {"agentfacility", DATA_TYPE_OPTION, CF_FACILITY, "The syslog facility for cf-agent", "LOG_USER"},
    {"allclassesreport", DATA_TYPE_OPTION, CF_BOOL, "Generate allclasses.txt report"},
    {"alwaysvalidate", DATA_TYPE_OPTION, CF_BOOL,
     "true/false flag to determine whether configurations will always be checked before executing, or only after updates"},
    {"auditing", DATA_TYPE_OPTION, CF_BOOL, "This option is deprecated, does nothing and is kept for backward compatibility", "false"},
    {"binarypaddingchar", DATA_TYPE_STRING, "", "Character used to pad unequal replacements in binary editing", "space (ASC=32)"},
    {"bindtointerface", DATA_TYPE_STRING, ".*", "Use this interface for outgoing connections"},
    {"hashupdates", DATA_TYPE_OPTION, CF_BOOL, "true/false whether stored hashes are updated when change is detected in source",
     "false"},
    {"childlibpath", DATA_TYPE_STRING, ".*", "LD_LIBRARY_PATH for child processes"},
    {"checksum_alert_time", DATA_TYPE_INT, "0,60", "The persistence time for the checksum_alert class", "10 mins"},
    {"defaultcopytype", DATA_TYPE_OPTION, "mtime,atime,ctime,digest,hash,binary", "ctime or mtime differ"},
    {"dryrun", DATA_TYPE_OPTION, CF_BOOL, "All talk and no action mode", "false"},
    {"editbinaryfilesize", DATA_TYPE_INT, CF_VALRANGE, "Integer limit on maximum binary file size to be edited", "100000"},
    {"editfilesize", DATA_TYPE_INT, CF_VALRANGE, "Integer limit on maximum text file size to be edited", "100000"},
    {"environment", DATA_TYPE_STRING_LIST, "[A-Za-z0-9_]+=.*", "List of environment variables to be inherited by children"},
    {"exclamation", DATA_TYPE_OPTION, CF_BOOL, "true/false print exclamation marks during security warnings", "true"},
    {"expireafter", DATA_TYPE_INT, CF_VALRANGE, "Global default for time before on-going promise repairs are interrupted",
     "1 min"},
    {"files_single_copy", DATA_TYPE_STRING_LIST, "", "List of filenames to be watched for multiple-source conflicts"},
    {"files_auto_define", DATA_TYPE_STRING_LIST, "", "List of filenames to define classes if copied"},
    {"hostnamekeys", DATA_TYPE_OPTION, CF_BOOL, "true/false label ppkeys by hostname not IP address", "false"},
    {"ifelapsed", DATA_TYPE_INT, CF_VALRANGE, "Global default for time that must elapse before promise will be rechecked",
     "1"},
    {"inform", DATA_TYPE_OPTION, CF_BOOL, "true/false set inform level default", "false"},
    {"intermittency", DATA_TYPE_OPTION, CF_BOOL,
     "This option is deprecated, does nothing and is kept for backward compatibility",
     "false"},
    {"max_children", DATA_TYPE_INT, CF_VALRANGE, "Maximum number of background tasks that should be allowed concurrently",
     "1 concurrent agent promise"},
    {"maxconnections", DATA_TYPE_INT, CF_VALRANGE, "Maximum number of outgoing connections to cf-serverd",
     "30 remote queries"},
    {"mountfilesystems", DATA_TYPE_OPTION, CF_BOOL, "true/false mount any filesystems promised", "false"},
    {"nonalphanumfiles", DATA_TYPE_OPTION, CF_BOOL, "true/false warn about filenames with no alphanumeric content", "false"},
    {"repchar", DATA_TYPE_STRING, ".", "The character used to canonize pathnames in the file repository", "_"},
    {"refresh_processes", DATA_TYPE_STRING_LIST, CF_IDRANGE,
     "Reload the process table before verifying the bundles named in this list (lazy evaluation)"},
    {"default_repository", DATA_TYPE_STRING, CF_ABSPATHRANGE, "Path to the default file repository", "in situ"},
    {"secureinput", DATA_TYPE_OPTION, CF_BOOL, "true/false check whether input files are writable by unauthorized users",
     "false"},
    {"sensiblecount", DATA_TYPE_INT, CF_VALRANGE, "Minimum number of files a mounted filesystem is expected to have",
     "2 files"},
    {"sensiblesize", DATA_TYPE_INT, CF_VALRANGE, "Minimum number of bytes a mounted filesystem is expected to have",
     "1000 bytes"},
    {"skipidentify", DATA_TYPE_OPTION, CF_BOOL,
     "Do not send IP/name during server connection because address resolution is broken", "false"},
    {"suspiciousnames", DATA_TYPE_STRING_LIST, "", "List of names to warn about if found during any file search"},
    {"syslog", DATA_TYPE_OPTION, CF_BOOL, "true/false switches on output to syslog at the inform level", "false"},
    {"track_value", DATA_TYPE_OPTION, CF_BOOL, "true/false switches on tracking of promise valuation", "false"},
    {"timezone", DATA_TYPE_STRING_LIST, "", "List of allowed timezones this machine must comply with"},
    {"default_timeout", DATA_TYPE_INT, CF_VALRANGE, "Maximum time a network connection should attempt to connect",
     "10 seconds"},
    {"verbose", DATA_TYPE_OPTION, CF_BOOL, "true/false switches on verbose standard output", "false"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFS_CONTROLBODY[] =
{
    {"allowallconnects", DATA_TYPE_STRING_LIST, "",
     "List of IPs or hostnames that may have more than one connection to the server port"},
    {"allowconnects", DATA_TYPE_STRING_LIST, "", "List of IPs or hostnames that may connect to the server port"},
    {"allowusers", DATA_TYPE_STRING_LIST, "", "List of usernames who may execute requests from this server"},
    {"auditing", DATA_TYPE_OPTION, CF_BOOL, "true/false activate auditing of server connections", "false"},
    {"bindtointerface", DATA_TYPE_STRING, "", "IP of the interface to which the server should bind on multi-homed hosts"},
    {"cfruncommand", DATA_TYPE_STRING, CF_PATHRANGE, "Path to the cf-agent command or cf-execd wrapper for remote execution"},
    {"call_collect_interval", DATA_TYPE_INT, CF_VALRANGE, "The interval in minutes in between collect calls to the policy hub offering a tunnel for report collection (Enterprise)"},
    {"collect_window", DATA_TYPE_INT, CF_VALRANGE, "A time in seconds that a collect-call tunnel remains open to a hub to attempt a report transfer before it is closed (Enterprise)"},
    {"denybadclocks", DATA_TYPE_OPTION, CF_BOOL, "true/false accept connections from hosts with clocks that are out of sync",
     "true"},
    {"denyconnects", DATA_TYPE_STRING_LIST, "", "List of IPs or hostnames that may NOT connect to the server port"},
    {"dynamicaddresses", DATA_TYPE_STRING_LIST, "", "List of IPs or hostnames for which the IP/name binding is expected to change"},
    {"hostnamekeys", DATA_TYPE_OPTION, CF_BOOL, "true/false store keys using hostname lookup instead of IP addresses", "false"},
    {"keycacheTTL", DATA_TYPE_INT, CF_VALRANGE, "Maximum number of hours to hold public keys in the cache", "24"},
    {"logallconnections", DATA_TYPE_OPTION, CF_BOOL, "true/false causes the server to log all new connections to syslog",
     "false"},
    {"logencryptedtransfers", DATA_TYPE_OPTION, CF_BOOL, "true/false log all successful transfers required to be encrypted",
     "false"},
    {"maxconnections", DATA_TYPE_INT, CF_VALRANGE, "Maximum number of connections that will be accepted by cf-serverd",
     "30 remote queries"},
    {"port", DATA_TYPE_INT, "1024,99999", "Default port for cfengine server", "5308"},
    {"serverfacility", DATA_TYPE_OPTION, CF_FACILITY, "Menu option for syslog facility level", "LOG_USER"},
    {"skipverify", DATA_TYPE_STRING_LIST, "", "List of IPs or hostnames for which we expect no DNS binding and cannot verify"},
    {"trustkeysfrom", DATA_TYPE_STRING_LIST, "", "List of IPs from whom we accept public keys on trust"},
    {"listen", DATA_TYPE_OPTION, CF_BOOL, "true/false enable server deamon to listen on defined port", "true"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFM_CONTROLBODY[] =
{
    {"forgetrate", DATA_TYPE_REAL, "0,1", "Decimal fraction [0,1] weighting of new values over old in 2d-average computation",
     "0.6"},
    {"monitorfacility", DATA_TYPE_OPTION, CF_FACILITY, "Menu option for syslog facility", "LOG_USER"},
    {"histograms", DATA_TYPE_OPTION, CF_BOOL, "Ignored, kept for backward compatibility", "true"},
    {"tcpdump", DATA_TYPE_OPTION, CF_BOOL, "true/false use tcpdump if found", "false"},
    {"tcpdumpcommand", DATA_TYPE_STRING, CF_ABSPATHRANGE, "Path to the tcpdump command on this system"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFR_CONTROLBODY[] =
{
    {"hosts", DATA_TYPE_STRING_LIST, "", "List of host or IP addresses to attempt connection with"},
    {"port", DATA_TYPE_INT, "1024,99999", "Default port for cfengine server", "5308"},
    {"force_ipv4", DATA_TYPE_OPTION, CF_BOOL, "true/false force use of ipv4 in connection", "false"},
    {"trustkey", DATA_TYPE_OPTION, CF_BOOL, "true/false automatically accept all keys on trust from servers", "false"},
    {"encrypt", DATA_TYPE_OPTION, CF_BOOL, "true/false encrypt connections with servers", "false"},
    {"background_children", DATA_TYPE_OPTION, CF_BOOL, "true/false parallelize connections to servers", "false"},
    {"max_children", DATA_TYPE_INT, CF_VALRANGE, "Maximum number of simultaneous connections to attempt", "50 runagents"},
    {"output_to_file", DATA_TYPE_OPTION, CF_BOOL, "true/false whether to send collected output to file(s)", "false"},
    {"output_directory", DATA_TYPE_STRING, CF_ABSPATHRANGE, "Directory where the output is stored"},
    {"timeout", DATA_TYPE_INT, "1,9999", "Connection timeout, sec"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFEX_CONTROLBODY[] = /* enum cfexcontrol */
{
    {"splaytime", DATA_TYPE_INT, CF_VALRANGE, "Time in minutes to splay this host based on its name hash", "0"},
    {"mailfrom", DATA_TYPE_STRING, ".*@.*", "Email-address cfengine mail appears to come from"},
    {"mailto", DATA_TYPE_STRING, ".*@.*", "Email-address cfengine mail is sent to"},
    {"smtpserver", DATA_TYPE_STRING, ".*", "Name or IP of a willing smtp server for sending email"},
    {"mailmaxlines", DATA_TYPE_INT, "0,1000", "Maximum number of lines of output to send by email", "30"},
    {"schedule", DATA_TYPE_STRING_LIST, "", "The class schedule used by cf-execd for activating cf-agent"},
    {"executorfacility", DATA_TYPE_OPTION, CF_FACILITY, "Menu option for syslog facility level", "LOG_USER"},
    {"exec_command", DATA_TYPE_STRING, CF_ABSPATHRANGE,
     "The full path and command to the executable run by default (overriding builtin)"},
    {"agent_expireafter", DATA_TYPE_INT, "0,10080", "Maximum agent runtime (in minutes)", "10080"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFH_CONTROLBODY[] =  /* enum cfh_control */
{
    {"export_zenoss", DATA_TYPE_STRING, CF_PATHRANGE, "Generate report for Zenoss integration"},
    {"exclude_hosts", DATA_TYPE_STRING_LIST, "", "A list of IP addresses of hosts to exclude from report collection"},
    {"hub_schedule", DATA_TYPE_STRING_LIST, "", "The class schedule used by cf-hub for report collation"},
    {"port", DATA_TYPE_INT, "1024,99999", "Default port for contacting hub nodes", "5308"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const BodySyntax CFFILE_CONTROLBODY[] =  /* enum cfh_control */
{
    {"namespace", DATA_TYPE_STRING, CF_IDRANGE, "Switch to a private namespace to protect current file from duplicate definitions"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
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
    {CF_TRANSACTION, DATA_TYPE_BODY, CF_TRANSACTION_BODY, "Output behaviour"},
    {CF_DEFINECLASSES, DATA_TYPE_BODY, CF_DEFINECLASS_BODY, "Signalling behaviour"},
    {"comment", DATA_TYPE_STRING, "", "A comment about this promise's real intention that follows through the program"},
    {"depends_on", DATA_TYPE_STRING_LIST, "","A list of promise handles that this promise builds on or depends on somehow (for knowledge management)"},
    {"handle", DATA_TYPE_STRING, "", "A unique id-tag string for referring to this as a promisee elsewhere"},
    {"ifvarclass", DATA_TYPE_STRING, "", "Extended classes ANDed with context"},
    {"meta", DATA_TYPE_STRING_LIST, "", "User-data associated with policy, e.g. key=value strings"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
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
    CF_MEASUREMENT_SUBTYPES,    /* mod_measurement.c */
};

const int CF3_MODULES = (sizeof(CF_ALL_SUBTYPES) / sizeof(CF_ALL_SUBTYPES[0]));
