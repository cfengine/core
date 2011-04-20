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

/*****************************************************************************/
/*                                                                           */
/* File: mod_common.c                                                        */
/*                                                                           */
/* This is a root node in the syntax tree                                    */
/*                                                                           */
/*****************************************************************************/

#define CF3_MOD_COMMON

#include "cf3.defs.h"
#include "cf3.extern.h"

struct BodySyntax CF_TRANSACTION_BODY[] =
   {
   {"action_policy",cf_opts,"fix,warn,nop","Whether to repair or report about non-kept promises"},
   {"ifelapsed",cf_int,CF_VALRANGE,"Number of minutes before next allowed assessment of promise"},
   {"expireafter",cf_int,CF_VALRANGE,"Number of minutes before a repair action is interrupted and retried"},
   {"log_string",cf_str,"","A message to be written to the log when a promise verification leads to a repair"},
   {"log_level",cf_opts,"inform,verbose,error,log","The reporting level sent to syslog"},
   {"log_kept",cf_str,CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
   {"log_priority",cf_opts,"emergency,alert,critical,error,warning,notice,info,debug","The priority level of the log message, as interpreted by a syslog server"},
   {"log_repaired",cf_str,CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
   {"log_failed",cf_str,CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"},
   {"value_kept",cf_real,CF_REALRANGE,"A real number value attributed to keeping this promise"},
   {"value_repaired",cf_real,CF_REALRANGE,"A real number value attributed to reparing this promise"},
   {"value_notkept",cf_real,CF_REALRANGE,"A real number value (possibly negative) attributed to not keeping this promise"},
   {"audit",cf_opts,CF_BOOL,"true/false switch for detailed audit records of this promise"},
   {"background",cf_opts,CF_BOOL,"true/false switch for parallelizing the promise repair"},
   {"report_level",cf_opts,"inform,verbose,error,log","The reporting level for standard output for this promise"},
   {"measurement_class",cf_str,"","If set performance will be measured and recorded under this identifier"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/

struct BodySyntax CF_DEFINECLASS_BODY[] =
   {
   {"promise_repaired",cf_slist,CF_IDRANGE,"A list of classes to be defined globally"},
   {"repair_failed",cf_slist,CF_IDRANGE,"A list of classes to be defined globally"},
   {"repair_denied",cf_slist,CF_IDRANGE,"A list of classes to be defined globally"},
   {"repair_timeout",cf_slist,CF_IDRANGE,"A list of classes to be defined globally"},
   {"promise_kept",cf_slist,CF_IDRANGE,"A list of classes to be defined globally"},
   {"cancel_kept",cf_slist,CF_IDRANGE,"A list of classes to be cancelled if the promise is kept"},
   {"cancel_repaired",cf_slist,CF_IDRANGE,"A list of classes to be cancelled if the promise is repaired"},
   {"cancel_notkept",cf_slist,CF_IDRANGE,"A list of classes to be cancelled if the promise is not kept for any reason"},
   {"kept_returncodes",cf_slist,CF_INTLISTRANGE,"A list of return codes indicating a kept command-related promise"},
   {"repaired_returncodes",cf_slist,CF_INTLISTRANGE,"A list of return codes indicating a repaired command-related promise"},
   {"failed_returncodes",cf_slist,CF_INTLISTRANGE,"A list of return codes indicating a failed command-related promise"},
   {"persist_time",cf_int,CF_VALRANGE,"A number of minutes the specified classes should remain active"},
   {"timer_policy",cf_opts,"absolute,reset","Whether a persistent class restarts its counter when rediscovered"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/

struct BodySyntax CF_VARBODY[] =
   {
   {"string",cf_str,"","A scalar string"},
   {"int",cf_int,CF_INTRANGE,"A scalar integer"},
   {"real",cf_real,CF_REALRANGE,"A scalar real number"},
   {"slist",cf_slist,"","A list of scalar strings"},
   {"ilist",cf_ilist,CF_INTRANGE,"A list of integers"},
   {"rlist",cf_rlist,CF_REALRANGE,"A list of real numbers"},
   {"policy",cf_opts,"free,overridable,constant,ifdefined","The policy for (dis)allowing (re)definition of variables"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/

struct BodySyntax CF_CLASSBODY[] =
   {
   {"or",cf_clist,CF_CLASSRANGE,"Combine class sources with inclusive OR"}, 
   {"and",cf_clist,CF_CLASSRANGE,"Combine class sources with AND"},
   {"xor",cf_clist,CF_CLASSRANGE,"Combine class sources with XOR"},
   {"dist",cf_rlist,CF_REALRANGE,"Generate a probabilistic class distribution (from strategies in cfengine 2)"},
   {"expression",cf_class,CF_CLASSRANGE,"Evaluate string expression of classes in normal form"},
   {"not",cf_class,CF_CLASSRANGE,"Evaluate the negation of string expression in normal form"},
   {"select_class",cf_rlist,CF_CLASSRANGE,"Select one of the named list of classes to define based on host identity"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/
/* Control bodies                                        */
/*********************************************************/

struct BodySyntax CFG_CONTROLBODY[] =
   {
   {"bundlesequence",cf_slist,".*","List of promise bundles to verify in order"},
   {"ignore_missing_bundles",cf_opts,CF_BOOL,"If any bundles in the bundlesequence do not exist, ignore and continue"},
   {"ignore_missing_inputs",cf_opts,CF_BOOL,"If any input files do not exist, ignore and continue"},
   {"inputs",cf_slist,".*","List of additional filenames to parse for promises"},
   {"version",cf_str,"","Scalar version string for this configuration"},
   {"lastseenexpireafter",cf_int,CF_VALRANGE,"Number of minutes after which last-seen entries are purged"},
   {"output_prefix",cf_str,"","The string prefix for standard output"},
   {"domain",cf_str,".*","Specify the domain name for this host"},
   {"require_comments",cf_opts,CF_BOOL,"Warn about promises that do not have comment documentation"},
   {"host_licenses_paid",cf_int,CF_VALRANGE,"The number of licenses that you promise to have paid for by setting this value (legally binding for commercial license)"},
   {"syslog_host",cf_str,CF_IPRANGE,"The name or address of a host to which syslog messages should be sent directly by UDP"},
   {"syslog_port",cf_int,CF_VALRANGE,"The port number of a UDP syslog service"},
   {"fips_mode",cf_opts,CF_BOOL,"Activate full FIPS mode restrictions"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFA_CONTROLBODY[] =
   {
   {"abortclasses",cf_slist,".*","A list of classes which if defined lead to termination of cf-agent"},
   {"abortbundleclasses",cf_slist,".*","A list of classes which if defined lead to termination of current bundle"},
   {"addclasses",cf_slist,".*","A list of classes to be defined always in the current context"},
   {"agentaccess",cf_slist,".*","A list of user names allowed to execute cf-agent"},
   {"agentfacility",cf_opts,CF_FACILITY,"The syslog facility for cf-agent"},
   {"alwaysvalidate",cf_opts,CF_BOOL,"true/false flag to determine whether configurations will always be checked before executing, or only after updates"},
   {"auditing",cf_opts,CF_BOOL,"true/false flag to activate the cf-agent audit log"},
   {"binarypaddingchar",cf_str,"","Character used to pad unequal replacements in binary editing"},
   {"bindtointerface",cf_str,".*","Use this interface for outgoing connections"},
   {"hashupdates",cf_opts,CF_BOOL,"true/false whether stored hashes are updated when change is detected in source"},
   {"childlibpath",cf_str,".*","LD_LIBRARY_PATH for child processes"},
   {"checksum_alert_time",cf_int,"0,60","The persistence time for the checksum_alert class"},
   {"defaultcopytype",cf_opts,"mtime,atime,ctime,digest,hash,binary"},
   {"dryrun",cf_opts,CF_BOOL,"All talk and no action mode"},
   {"editbinaryfilesize",cf_int,CF_VALRANGE,"Integer limit on maximum binary file size to be edited"},
   {"editfilesize",cf_int,CF_VALRANGE,"Integer limit on maximum text file size to be edited"},
   {"environment",cf_slist,"[A-Za-z0-9_]+=.*","List of environment variables to be inherited by children"},
   {"exclamation",cf_opts,CF_BOOL,"true/false print exclamation marks during security warnings"},
   {"expireafter",cf_int,CF_VALRANGE,"Global default for time before on-going promise repairs are interrupted"},
   {"files_single_copy",cf_slist,"","List of filenames to be watched for multiple-source conflicts"},
   {"files_auto_define",cf_slist,"","List of filenames to define classes if copied"},
   {"hostnamekeys",cf_opts,CF_BOOL,"true/false label ppkeys by hostname not IP address"},
   {"ifelapsed",cf_int,CF_VALRANGE,"Global default for time that must elapse before promise will be rechecked"},
   {"inform",cf_opts,CF_BOOL,"true/false set inform level default"},
   {"intermittency",cf_opts,CF_BOOL,"true/false store detailed recordings of last observed time for all client-server connections for reliability assessment (false)"},
   {"max_children",cf_int,CF_VALRANGE,"Maximum number of background tasks that should be allowed concurrently"},
   {"maxconnections",cf_int,CF_VALRANGE,"Maximum number of outgoing connections to cf-serverd"},
   {"mountfilesystems",cf_opts,CF_BOOL,"true/false mount any filesystems promised"},
   {"nonalphanumfiles",cf_opts,CF_BOOL,"true/false warn about filenames with no alphanumeric content"},
   {"repchar",cf_str,".","The character used to canonize pathnames in the file repository"},
   {"refresh_processes",cf_slist,CF_IDRANGE,"Reload the process table before verifying the bundles named in this list (lazy evaluation)"},
   {"default_repository",cf_str,CF_PATHRANGE,"Path to the default file repository"},
   {"secureinput",cf_opts,CF_BOOL,"true/false check whether input files are writable by unauthorized users"},
   {"sensiblecount",cf_int,CF_VALRANGE,"Minimum number of files a mounted filesystem is expected to have"},
   {"sensiblesize",cf_int,CF_VALRANGE,"Minimum number of bytes a mounted filesystem is expected to have"},
   {"skipidentify",cf_opts,CF_BOOL,"Do not send IP/name during server connection because address resolution is broken"},
   {"suspiciousnames",cf_slist,"","List of names to warn about if found during any file search"},
   {"syslog",cf_opts,CF_BOOL,"true/false switches on output to syslog at the inform level"},
   {"track_value",cf_opts,CF_BOOL,"true/false switches on tracking of promise valuation"},
   {"timezone",cf_slist,"","List of allowed timezones this machine must comply with"},
   {"default_timeout",cf_int,CF_VALRANGE,"Maximum time a network connection should attempt to connect"},
   {"verbose",cf_opts,CF_BOOL,"true/false switches on verbose standard output"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFS_CONTROLBODY[] =
   {
   {"allowallconnects",cf_slist,"","List of IPs or hostnames that may have more than one connection to the server port"},
   {"allowconnects",cf_slist,"","List of IPs or hostnames that may connect to the server port"},
   {"allowusers",cf_slist,"","List of usernames who may execute requests from this server"},
   {"auditing",cf_opts,CF_BOOL,"true/false activate auditing of server connections"},
   {"bindtointerface",cf_str,"","IP of the interface to which the server should bind on multi-homed hosts"},
   {"cfruncommand",cf_str,CF_PATHRANGE,"Path to the cf-agent command or cf-execd wrapper for remote execution"},
   {"denybadclocks",cf_opts,CF_BOOL,"true/false accept connections from hosts with clocks that are out of sync"},
   {"denyconnects",cf_slist,"","List of IPs or hostnames that may NOT connect to the server port"},
   {"dynamicaddresses",cf_slist,"","List of IPs or hostnames for which the IP/name binding is expected to change"},
   {"hostnamekeys",cf_opts,CF_BOOL,"true/false store keys using hostname lookup instead of IP addresses"},
   {"keycacheTTL",cf_int,CF_VALRANGE,"Maximum number of hours to hold public keys in the cache"},
   {"logallconnections",cf_opts,CF_BOOL,"true/false causes the server to log all new connections to syslog"},
   {"logencryptedtransfers",cf_opts,CF_BOOL,"true/false log all successful transfers required to be encrypted"},
   {"maxconnections",cf_int,CF_VALRANGE,"Maximum number of connections that will be accepted by cf-serverd"},
   {"port",cf_int,"1024,99999","Default port for cfengine server"},
   {"serverfacility",cf_opts,CF_FACILITY,"Menu option for syslog facility level"},
   {"skipverify",cf_slist,"","List of IPs or hostnames for which we expect no DNS binding and cannot verify"},
   {"trustkeysfrom",cf_slist,"","List of IPs from whom we accept public keys on trust"},
   {NULL,cf_notype,NULL,NULL}
   };


struct BodySyntax CFM_CONTROLBODY[] =
   {
   {"forgetrate",cf_real,"0,1","Decimal fraction [0,1] weighting of new values over old in 2d-average computation"},
   {"monitorfacility",cf_opts,CF_FACILITY,"Menu option for syslog facility"},
   {"histograms",cf_opts,CF_BOOL,"Ignored, kept for backward compatibility"},
   {"tcpdump",cf_opts,CF_BOOL,"true/false use tcpdump if found"},
   {"tcpdumpcommand",cf_str,CF_PATHRANGE,"Path to the tcpdump command on this system"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFR_CONTROLBODY[] =
   {
   {"hosts",cf_slist,"","List of host or IP addresses to attempt connection with"},
   {"port",cf_int,"1024,99999","Default port for cfengine server"},
   {"force_ipv4",cf_opts,CF_BOOL,"true/false force use of ipv4 in connection"},
   {"trustkey",cf_opts,CF_BOOL,"true/false automatically accept all keys on trust from servers"},
   {"encrypt",cf_opts,CF_BOOL,"true/false encrypt connections with servers"},
   {"background_children",cf_opts,CF_BOOL,"true/false parallelize connections to servers"},
   {"max_children",cf_int,CF_VALRANGE,"Maximum number of simultaneous connections to attempt"},
   {"output_to_file",cf_opts,CF_BOOL,"true/false whether to send collected output to file(s)"},
   {"timeout",cf_int,"1,9999","Connection timeout, sec"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFEX_CONTROLBODY[] = /* enum cfexcontrol */
   {
   {"splaytime",cf_int,CF_VALRANGE,"Time in minutes to splay this host based on its name hash"},
   {"mailfrom",cf_str,".*@.*","Email-address cfengine mail appears to come from"},
   {"mailto",cf_str,".*@.*","Email-address cfengine mail is sent to"},
   {"smtpserver",cf_str,".*","Name or IP of a willing smtp server for sending email"},
   {"mailmaxlines",cf_int,"0,1000","Maximum number of lines of output to send by email"},
   {"schedule",cf_slist,"","The class schedule used by cf-execd for activating cf-agent"},
   {"executorfacility",cf_opts,CF_FACILITY,"Menu option for syslog facility level"},
   {"exec_command",cf_str,CF_PATHRANGE,"The full path and command to the executable run by default (overriding builtin)"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFK_CONTROLBODY[] =
   {
   {"build_directory",cf_str,".*","The directory in which to generate output files"},
   {"document_root",cf_str,".*","The directory in which the web root resides"},
   {"generate_manual",cf_opts,CF_BOOL,"true/false generate texinfo manual page skeleton for this version"},
   {"graph_directory",cf_str,CF_PATHRANGE,"Path to directory where rendered .png files will be created"},
   {"graph_output",cf_opts,CF_BOOL,"true/false generate png visualization of topic map if possible (requires lib)"},
   {"goal_categories",cf_slist,"","A list of context names that represent parent categories for goals (goal patterns)"},
   {"goal_patterns",cf_slist,"","A list of regular expressions that match promisees/topics considered to be organizational goals"},
   {"html_banner",cf_str,"","HTML code for a banner to be added to rendered in html after the header"},
   {"html_footer",cf_str,"","HTML code for a page footer to be added to rendered in html before the end body tag"},
   {"id_prefix",cf_str,".*","The LTM identifier prefix used to label topic maps (used for disambiguation in merging)"},
   {"manual_source_directory",cf_str,CF_PATHRANGE,"Path to directory where raw text about manual topics is found (defaults to build_directory)"},
   {"query_engine",cf_str,"","Name of a dynamic web-page used to accept and drive queries in a browser"},
   {"query_output",cf_opts,"html,text","Menu option for generated output format"},
   {"sql_type",cf_opts,"mysql,postgres","Menu option for supported database type"},
   {"sql_database",cf_str,"","Name of database used for the topic map"},
   {"sql_owner",cf_str,"","User id of sql database user"},
   {"sql_passwd",cf_str,"","Embedded password for accessing sql database"},
   {"sql_server",cf_str,"","Name or IP of database server (or localhost)"},
   {"sql_connection_db",cf_str,"","The name of an existing database to connect to in order to create/manage other databases"},
   {"style_sheet",cf_str,"","Name of a style-sheet to be used in rendering html output (added to headers)"},
   {"view_projections",cf_opts,CF_BOOL,"Perform view-projection analytics in graph generation"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFRE_CONTROLBODY[] = /* enum cfrecontrol */
   {
   {"aggregation_point",cf_str,CF_PATHRANGE,"The root directory of the data cache for CMDB aggregation"},       
   {"auto_scaling",cf_opts,CF_BOOL,"true/false whether to auto-scale graph output to optimize use of space"},
   {"build_directory",cf_str,".*","The directory in which to generate output files"},
   {"csv2xml",cf_slist,"","A list of csv formatted files in the build directory to convert to simple xml"},
   {"error_bars",cf_opts,CF_BOOL,"true/false whether to generate error bars on graph output"},
   {"html_banner",cf_str,"","HTML code for a banner to be added to rendered in html after the header"},
   {"html_embed",cf_opts,CF_BOOL,"If true, no header and footer tags will be added to html output"},
   {"html_footer",cf_str,"","HTML code for a page footer to be added to rendered in html before the end body tag"},
   {"query_engine",cf_str,"","Name of a dynamic web-page used to accept and drive queries in a browser"},
   {"reports",cf_olist,"all,audit,performance,all_locks,active_locks,hashes,classes,last_seen,monitor_now,monitor_history,monitor_summary,compliance,setuid,file_changes,installed_software,software_patches,value,variables","A list of reports that may be generated"},
   {"report_output",cf_opts,"csv,html,text,xml","Menu option for generated output format. Applies only to text reports, graph data remain in xydy format."},
   {"style_sheet",cf_str,"","Name of a style-sheet to be used in rendering html output (added to headers)"},
   {"time_stamps",cf_opts,CF_BOOL,"true/false whether to generate timestamps in the output directory name"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFH_CONTROLBODY[] = /* enum cfh_control */
   {
   {"export_zenoss",cf_opts,CF_BOOL,"Make data available for Zenoss integration in docroot/reports/summary.z"},
   {"federation",cf_slist,"","The list of cfengine servers supporting constellation integration with this hub"},
   {"hub_schedule",cf_slist,"","The class schedule used by cf-hub for report collation"},
   {"port",cf_int,"1024,99999","Default port for contacting hub nodes"},
   {NULL,cf_notype,NULL,NULL}
   };


/*********************************************************/

/* This list is for checking free standing body lval => rval bindings */
    
struct SubTypeSyntax CF_ALL_BODIES[] =
   {
   {CF_COMMONC,"control",CFG_CONTROLBODY},
   {CF_AGENTC,"control",CFA_CONTROLBODY},
   {CF_SERVERC,"control",CFS_CONTROLBODY},
   {CF_MONITORC,"control",CFM_CONTROLBODY},
   {CF_RUNC,"control",CFR_CONTROLBODY},
   {CF_EXECC,"control",CFEX_CONTROLBODY},
   {CF_KNOWC,"control",CFK_CONTROLBODY},
   {CF_REPORTC,"control",CFRE_CONTROLBODY},
   {CF_HUBC,"control",CFH_CONTROLBODY},

   //  get others from modules e.g. "agent","files",CF_FILES_BODIES,

   {NULL,NULL,NULL}
   };



/*********************************************************/
/*                                                       */
/* Constraint values/types                               */
/*                                                       */
/*********************************************************/

 /* This is where we place lval => rval bindings that
    apply to more than one subtype, e.g. generic
    processing behavioural details */

struct BodySyntax CF_COMMON_BODIES[] =
   {
   {CF_TRANSACTION,cf_body,CF_TRANSACTION_BODY,"Output behaviour"},
   {CF_DEFINECLASSES,cf_body,CF_DEFINECLASS_BODY,"Signalling behaviour"},
   {"ifvarclass",cf_str,"","Extended classes ANDed with context"},
   {"handle",cf_str,CF_IDRANGE,"A unique id-tag string for referring to this as a promisee elsewhere"},
   {"depends_on",cf_slist,"","A list of promise handles that this promise builds on or depends on somehow (for knowledge management)"},
   {"comment",cf_str,"","A comment about this promise's real intention that follows through the program"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/

 /* This is where we place promise subtypes that apply
    to more than one type of bundle, e.g. agent,server.. */

struct SubTypeSyntax CF_COMMON_SUBTYPES[] =
     {
     {"*","vars",CF_VARBODY},
     {"*","classes",CF_CLASSBODY},
     {"*","reports",CF_REPORT_BODIES},
     {"*","*",CF_COMMON_BODIES},
     {NULL,NULL,NULL}
     };

/*********************************************************/
/* THIS IS WHERE TO ATTACH SYNTAX MODULES                */
/*********************************************************/

/* Read in all parsable Bundle definitions */
/* REMEMBER TO REGISTER THESE IN cf3.extern.h */

struct SubTypeSyntax *CF_ALL_SUBTYPES[CF3_MODULES] =
   {
   CF_COMMON_SUBTYPES,       /* Add modules after this, mod_report.c is here */
   CF_EXEC_SUBTYPES,         /* mod_exec.c */
   CF_DATABASES_SUBTYPES,    /* mod_databases.c */
   CF_ENVIRONMENT_SUBTYPES,  /* mod_environ.c */
   CF_FILES_SUBTYPES,        /* mod_files.c */
   CF_INTERFACES_SUBTYPES,   /* mod_interfaces.c */
   CF_METHOD_SUBTYPES,       /* mod_methods.c */
   CF_OUTPUTS_SUBTYPES,      /* mod_outputs.c */
   CF_PACKAGES_SUBTYPES,     /* mod_packages.c */
   CF_PROCESS_SUBTYPES,      /* mod_process.c */
   CF_SERVICES_SUBTYPES,     /* mod_services.c */
   CF_STORAGE_SUBTYPES,      /* mod_storage.c */
   CF_REMACCESS_SUBTYPES,    /* mod_access.c */
   CF_KNOWLEDGE_SUBTYPES,    /* mod_knowledge.c */
   CF_MEASUREMENT_SUBTYPES,  /* mod_measurement.c */
   
   /* update CF3_MODULES in cf3.defs.h */
   };
