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


/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

struct FnCallType CF_FNCALL_TYPES[] = 
   {
   {"accessedbefore",cf_class,2,"True if arg1 was accessed before arg2 (atime)"},
   {"accumulated",cf_int,6,"Convert an accumulated amount of time into a system representation"},
   {"ago",cf_int,6,"Convert a time relative to now to an integer system representation"},
   {"canonify",cf_str,1,"Convert an abitrary string into a legal class name"},
   {"changedbefore",cf_class,2,"True if arg1 was changed before arg2 (ctime)"},
   {"classify",cf_class,1,"True if the canonicalization of the argument is a currently defined class"},
   {"classmatch",cf_class,1,"True if the regular expression matches any currently defined class"},
   {"execresult",cf_str,2,"Execute named command and assign output to variable"},
   {"fileexists",cf_class,1,"True if the named file can be accessed"},
   {"filesexist",cf_class,1,"True if the named list of files can ALL be accessed"},
   {"getindices",cf_slist,1,"Get a list of keys to the array whose id is the argument and assign to variable"},
   {"getgid",cf_int,1,"Return the integer group id of the named group on this host"},
   {"getuid",cf_int,1,"Return the integer user id of the named user on this host"},
   {"groupexists",cf_class,1,"True if group or numerical id exists on this host"},
   {"hash",cf_str,2,"Return the hash of arg1, type arg2 and assign to a variable"},
   {"hashmatch",cf_class,3,"Compute the hash of arg1, of type arg2 and test if it matches the value in arg 3"},
   {"hostrange",cf_class,2,"True if the current host lies in the range of enumerated hostnames specified"},
   {"hostinnetgroup",cf_class,1,"True if the current host is in the named netgroup"},
   {"iprange",cf_class,1,"True if the current host lies in the range of IP addresses specified"},
   {"irange",cf_irange,2,"Define a range of integer values for cfengine internal use"},
   {"isdir",cf_class,1,"True if the named object is a directory"},
   {"isgreaterthan",cf_class,2,"True if arg1 is numerically greater than arg2, else compare strings like strcmp"},
   {"islessthan",cf_class,2,"True if arg1 is numerically less than arg2, else compare strings like NOT strcmp"},
   {"islink",cf_class,1,"True if the named object is a symbolic link"},
   {"isnewerthan",cf_class,2,"True if arg1 is newer (modified later) than arg2 (mtime)"},
   {"isplain",cf_class,1,"True if the named object is a plain/regular file"},
   {"isvariable",cf_class,1,"True if the named variable is defined"},
   {"lastnode",cf_str,2,"Extract the last of a separated string, e.g. filename from a path"},
   {"ldaparray",cf_class,6,"Extract all values from an ldap record"},
   {"ldaplist",cf_slist,6,"Extract all named values from multiple ldap records"},
   {"ldapvalue",cf_str,6,"Extract the first matching named value from ldap"},
   {"now",cf_int,0,"Convert the current time into system representation"},
   {"on",cf_int,6,"Convert an exact date/time to an integer system representation"},
   {"peers",cf_slist,3,"Get a list of peers (not including ourself) from the partition to which we belong"},
   {"peerleader",cf_str,3,"Get the assigned peer-leader of the partition to which we belong"},
   {"peerleaders",cf_slist,3,"Get a list of peer leaders from the named partitioning"},
   {"randomint",cf_int,2,"Generate a random integer between the given limits"},
   {"readfile",cf_str,2,"Read max number of bytes from named file and assign to variable"},
   {"readintarray",cf_int,6,"Read an array of integers from a file and assign the dimension to a variable"},
   {"readintlist",cf_ilist,5,"Read and assign a list variable from a file of separated ints"},
   {"readrealarray",cf_int,6,"Read an array of real numbers from a file and assign the dimension to a variable"},
   {"readreallist",cf_rlist,5,"Read and assign a list variable from a file of separated real numbers"},
   {"readstringarray",cf_int,6,"Read an array of strings from a file and assign the dimension to a variable"},
   {"readstringlist",cf_slist,5,"Read and assign a list variable from a file of separated strings"},
   {"readtcp",cf_str,4,"Connect to tcp port, send string and assign result to variable"},
   {"regarray",cf_class,2,"True if arg1 matches any item in the associative array with id=arg2"},
   {"regcmp",cf_class,2,"True if arg2 is a regular expression matching arg1"},
   {"registryvalue",cf_str,2,"Returns a value for an MS-Win registry key,value pair"},
   {"regline",cf_class,2,"True if arg2 is a regular expression matching a line in file arg1"},
   {"reglist",cf_class,2,"True if arg2 matches any item in the list with id=arg1"},
   {"regldap",cf_class,7,"True if arg6 is a regular expression matching a value item in an ldap search"},
   {"remotescalar",cf_str,3,"Read a scalar value from a remote cfengine server"},
   {"returnszero",cf_class,2,"True if named shell command has exit status zero"},
   {"rrange",cf_rrange,2,"Define a range of real numbers for cfengine internal use"},
   {"selectservers",cf_int,6,"Select tcp servers which respond correctly to a query and return their number, set array of names"},
   {"splayclass",cf_class,2,"True if the first argument's time-slot has arrived, according to a policy in arg2"},
   {"splitstring",cf_slist,3,"Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2"},
   {"strcmp",cf_class,2,"True if the two strings match exactly"},
   {"usemodule",cf_class,2,"Execute cfengine module script and set class if successful"},
   {"userexists",cf_class,1,"True if user name or numerical id exists on this host"},
   {NULL,cf_notype,0,NULL}
   };

/*********************************************************/

struct BodySyntax CF_TRANSACTION_BODY[] =
   {
   {"action_policy",cf_opts,"fix,warn,nop","Whether to repair or report about non-kept promises"},
   {"ifelapsed",cf_int,CF_VALRANGE,"Number of minutes before next allowed assessment of promise"},
   {"expireafter",cf_int,CF_VALRANGE,"Number of minutes before a repair action is interrupted and retried"},
   {"log_string",cf_str,"","A message to be written to the log when a promise verification leads to a repair"},
   {"log_level",cf_opts,"inform,verbose,error,log","The reporting level sent to syslog"},
   {"log_kept",cf_str,"","This should be filename of a file to which log_string will be saved, if undefined it goes to syslog"},
   {"log_repaired",cf_str,"","This should be filename of a file to which log_string will be saved, if undefined it goes to syslog"},
   {"log_failed",cf_str,"","This should be filename of a file to which log_string will be saved, if undefined it goes to syslog"},
   {"audit",cf_opts,CF_BOOL,"true/false switch for detailed audit records of this promise"},
   {"background",cf_opts,CF_BOOL,"true/false switch for parallelizing the promise repair"},
   {"report_level",cf_opts,"inform,verbose,error,log","The reporting level for standard output"},
   {"measurement_class",cf_str,"","If set performance will be measured and recorded under this identifier"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/

struct BodySyntax CF_DEFINECLASS_BODY[] =
   {
   {"promise_repaired",cf_slist,CF_IDRANGE,"A list of classes to be defined"},
   {"repair_failed",cf_slist,CF_IDRANGE,"A list of classes to be defined"},
   {"repair_denied",cf_slist,CF_IDRANGE,"A list of classes to be defined"},
   {"repair_timeout",cf_slist,CF_IDRANGE,"A list of classes to be defined"},
   {"promise_kept",cf_slist,CF_IDRANGE,"A list of classes to be defined"},
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
   {"policy",cf_opts,"free,overridable,constant","The policy for (dis)allowing redefinition of variables"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/

struct BodySyntax CF_CLASSBODY[] =
   {
   {"or",cf_clist,CF_CLASSRANGE,"Combine class sources with inclusive OR"}, 
   {"and",cf_clist,CF_CLASSRANGE,"Combine class sources with AND"},
   {"xor",cf_clist,CF_CLASSRANGE,"Combine class sources with XOR"},
   {"dist",cf_rlist,CF_REALRANGE,"Generate a probabilistic class distribution (strategy in cfengine 2)"},
   {"expression",cf_class,CF_CLASSRANGE,"Evaluate string expression of classes in normal form"},
   {"not",cf_class,CF_CLASSRANGE,"Evaluate the negation of string expression in normal form"},
   {NULL,cf_notype,NULL,NULL}
   };

/*********************************************************/
/* Control bodies                                        */
/*********************************************************/

struct BodySyntax CFG_CONTROLBODY[] =
   {
   {"bundlesequence",cf_slist,".*","List of promise bundles to verify in order"},
   {"inputs",cf_slist,".*","List of filenames to parse for promises"},
   {"version",cf_str,"","Scalar version string for this configuration"},
   {"lastseenexpireafter",cf_int,CF_VALRANGE,"Number of minutes after which last-seen entries are purged"},
   {"output_prefix",cf_str,"","The string prefix for standard output"},
   {"domain",cf_str,".*","Specify the domain name for this host"},
   {"require_comments",cf_opts,CF_BOOL,"Warn about promises that do not have comment documentation"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFA_CONTROLBODY[] =
   {
   {"abortclasses",cf_slist,".*","A list of classes which if defined lead to termination of cf-agent"},
   {"abortbundleclasses",cf_slist,".*","A list of classes which if defined lead to termination of current bundle"},
   {"addclasses",cf_slist,".*","A list of classes to be defined always in the current context"},
   {"agentaccess",cf_slist,".*","A list of user names allowed to execute cf-agent"},
   {"agentfacility",cf_opts,CF_FACILITY,"The syslog facility for cf-agent"},
   {"auditing",cf_opts,CF_BOOL,"true/false flag to activate the cf-agent audit log"},
   {"binarypaddingchar",cf_str,"","Character used to pad unequal replacements in binary editing"},
   {"bindtointerface",cf_str,".*","Use this interface for outgoing connections"},
   {"hashupdates",cf_opts,CF_BOOL,"true/false whether stored hashes are updated when change is detected in source"},
   {"childlibpath",cf_str,".*","LD_LIBRARY_PATH for child processes"},
   {"defaultcopytype",cf_opts,"mtime,atime,ctime,digest,hash,binary"},
   {"dryrun",cf_opts,CF_BOOL,"All talk and no action mode"},
   {"editbinaryfilesize",cf_int,CF_VALRANGE,"Integer limit on maximum binary file size to be edited"},
   {"editfilesize",cf_int,CF_VALRANGE,"Integer limit on maximum text file size to be edited"},
   {"environment",cf_slist,"[A-Za-z_]+=.*","List of environment variables to be inherited by children"},
   {"exclamation",cf_opts,CF_BOOL,"true/false print exclamation marks during security warnings"},
   {"expireafter",cf_int,CF_VALRANGE,"Global default for time before on-going promise repairs are interrupted"},
   {"files_single_copy",cf_slist,"","List of filenames to be watched for multiple-source conflicts"},
   {"files_auto_define",cf_slist,"","List of filenames to define classes if copied"},
   {"fullencryption",cf_opts,CF_BOOL,"Full encryption mode in server connections, includes directory listings"},
   {"hostnamekeys",cf_opts,CF_BOOL,"true/false label ppkeys by hostname not IP address"},
   {"ifelapsed",cf_int,CF_VALRANGE,"Global default for time that must elapse before promise will be rechecked"},
   {"inform",cf_opts,CF_BOOL,"true/false set inform level default"},
   {"lastseen",cf_opts,CF_BOOL,"true/false record last observed time for all client-server connections (true)"},
   {"intermittency",cf_opts,CF_BOOL,"true/false store detailed recordings of last observed time for all client-server connections for reliability assessment (false)"},
   {"max_children",cf_int,CF_VALRANGE,"Maximum number of background tasks that should be allowed concurrently"},
   {"maxconnections",cf_int,CF_VALRANGE,"Maximum number of outgoing connections to cf-serverd"},
   {"mountfilesystems",cf_opts,CF_BOOL,"true/false mount any filesystems promised"},
   {"nonalphanumfiles",cf_opts,CF_BOOL,"true/false warn about filenames with no alphanumeric content"},
   {"repchar",cf_str,".","The character used to canonize pathnames in the file repository"},
   {"default_repository",cf_str,CF_PATHRANGE,"Path to the default file repository"},
   {"secureinput",cf_opts,CF_BOOL,"true/false check whether input files are writable by unauthorized users"},
   {"sensiblecount",cf_int,CF_VALRANGE,"Minimum number of files a mounted filesystem is expected to have"},
   {"sensiblesize",cf_int,CF_VALRANGE,"Minimum number of bytes a mounted filesystem is expected to have"},
   {"skipidentify",cf_opts,CF_BOOL,"Do not send IP/name during server connection because address resolution is broken"},
   {"suspiciousnames",cf_slist,"List of names to warn about if found during any file search"},
   {"syslog",cf_opts,CF_BOOL,"true/false switches on output to syslog at the inform level"},
   {"timezone",cf_slist,"","List of allowed timezones this machine must comply with"},
   {"default_timeout",cf_int,CF_VALRANGE,"Maximum time a network connection should attempt to connect"},
   {"verbose",cf_opts,CF_BOOL,"true/false switches on verbose standard output"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFS_CONTROLBODY[] =
   {
   {"cfruncommand",cf_str,CF_PATHRANGE,"Path to the cf-agent command or cf-execd wrapper for remote execution"},
   {"maxconnections",cf_int,CF_VALRANGE,"Maximum number of connections that will be accepted by cf-serverd"},
   {"denybadclocks",cf_opts,CF_BOOL,"true/false accept connections from hosts with clocks that are out of sync"},
   {"allowconnects",cf_slist,"","List of IPs or hostnames that may connect to the server port"},
   {"denyconnects",cf_slist,"","List of IPs or hostnames that may NOT connect to the server port"},
   {"allowallconnects",cf_slist,"","List of IPs or hostnames that may have more than one connection to the server port"},
   {"trustkeysfrom",cf_slist,"","List of IPs or hostnames from whom we accept public keys on trust"},
   {"allowusers",cf_slist,"","List of usernames who may execute requests from this server"},
   {"dynamicaddresses",cf_slist,"","List of IPs or hostnames for which the IP/name binding is expected to change"},
   {"skipverify",cf_slist,"","List of IPs or hostnames for which we expect no DNS binding and cannot verify"},
   {"logallconnections",cf_opts,CF_BOOL,"true/false causes the server to log all new connections to syslog"},
   {"logencryptedtransfers",cf_opts,CF_BOOL,"true/false log all successful transfers required to be encrypted"},
   {"hostnamekeys",cf_opts,CF_BOOL,"true/false store keys using hostname lookup instead of IP addresses"},
   {"auditing",cf_opts,CF_BOOL,"true/false activate auditing of server connections"},
   {"bindtointerface",cf_str,"","IP of the interface to which the server should bind on multi-homed hosts"},
   {"serverfacility",cf_opts,CF_FACILITY,"Menu option for syslog facility level"},
   {"port",cf_int,"1024,99999","Default port for cfengine server"},
   {NULL,cf_notype,NULL,NULL}
   };


struct BodySyntax CFM_CONTROLBODY[] =
   {
   {"forgetrate",cf_real,"0,1","Decimal fraction [0,1] weighting of new values over old in 2d-average computation"},
   {"monitorfacility",cf_opts,CF_FACILITY,"Menu option for syslog facility"},
   {"histograms",cf_opts,CF_BOOL,"true/false store signal histogram data"},
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
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFEX_CONTROLBODY[] = /* enum cfexcontrol */
   {
   {"splaytime",cf_int,CF_VALRANGE,"Time in minutes to splay this host based on its name hash"},
   {"mailfrom",cf_str,".*@.*","Email-address cfengine mail appears to come from"},
   {"mailto",cf_str,".*@.*","Email-address cfengine mail is sent to"},
   {"smtpserver",cf_str,".*","Name or IP of a willing smtp server for sending email"},
   {"mailmaxlines",cf_int,"0,1000","Maximum number of lines of output to send by email"},
   {"schedule",cf_slist,"","The class schedule for activating cf-execd"},
   {"executorfacility",cf_opts,CF_FACILITY,"Menu option for syslog facility level"},
   {"exec_command",cf_str,CF_PATHRANGE,"The full path and command to the executable run by default (overriding builtin)"},
   {NULL,cf_notype,NULL,NULL}
   };

struct BodySyntax CFK_CONTROLBODY[] =
   {
   {"id_prefix",cf_str,".*","The LTM identifier prefix used to label topic maps (used for disambiguation in merging)"},
   {"build_directory",cf_str,".*","The directory in which to generate output files"},
   {"sql_type",cf_opts,"mysql,postgres","Menu option for supported database type"},
   {"sql_database",cf_str,"","Name of database used for the topic map"},
   {"sql_owner",cf_str,"","User id of sql database user"},
   {"sql_passwd",cf_str,"","Embedded password for accessing sql database"},
   {"sql_server",cf_str,"","Name or IP of database server (or localhost)"},
   {"sql_connection_db",cf_str,"","The name of an existing database to connect to in order to create/manage other databases"},
   {"query_output",cf_opts,"html,text","Menu option for generated output format"},
   {"query_engine",cf_str,"","Name of a dynamic web-page used to accept and drive queries in a browser"},
   {"style_sheet",cf_str,"","Name of a style-sheet to be used in rendering html output (added to headers)"},
   {"html_banner",cf_str,"","HTML code for a banner to be added to rendered in html after the header"},
   {"html_footer",cf_str,"","HTML code for a page footer to be added to rendered in html before the end body tag"},
   {"graph_output",cf_opts,CF_BOOL,"true/false generate png visualization of topic map if possible (requires lib)"},
   {"graph_directory",cf_str,CF_PATHRANGE,"Path to directory where rendered .png files will be created"},
   {"generate_manual",cf_opts,CF_BOOL,"true/false generate texinfo manual page skeleton for this version"},
   {"manual_source_directory",cf_str,CF_PATHRANGE,"Path to directory where raw text about manual topics is found (defaults to build_directory)"},
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
   {"reports",cf_olist,"audit,performance,all_locks,active_locks,hashes,classes,last_seen,monitor_now,monitor_history,monitor_summary,compliance,setuid,file_changes,installed_software,software_patches,variables","A list of reports that may be generated"},
   {"report_output",cf_opts,"csv,html,text,xml","Menu option for generated output format. Applies only to text reports, graph data remain in xydy format."},
   {"style_sheet",cf_str,"","Name of a style-sheet to be used in rendering html output (added to headers)"},
   {"time_stamps",cf_opts,CF_BOOL,"true/false whether to generate timestamps on the output directory"},
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
     {"agent","*",CF_COMMON_BODIES},
     {"edit_line","*",CF_COMMON_EDITBODIES},
     {NULL,NULL,NULL}
     };

/*********************************************************/
/* THIS IS WHERE TO ATTACH SYNTAX MODULES                */
/*********************************************************/

/* Read in all parsable Bundle definitions */
/* REMEMBER TO REGISTER THESE IN cf3.extern.h */

struct SubTypeSyntax *CF_ALL_SUBTYPES[CF3_MODULES] =
   {
   CF_COMMON_SUBTYPES,     /* Add modules after this, mod_report.c is here */
   CF_EXEC_SUBTYPES,       /* mod_exec.c */
   CF_DATABASES_SUBTYPES,  /* mod_databases.c */
   CF_FILES_SUBTYPES,      /* mod_files.c */
   CF_INTERFACES_SUBTYPES, /* mod_interfaces.c */
   CF_METHOD_SUBTYPES,     /* mod_methods.c */
   CF_PACKAGES_SUBTYPES,   /* mod_packages.c */
   CF_PROCESS_SUBTYPES,    /* mod_process.c */
   CF_STORAGE_SUBTYPES,    /* mod_storage.c */
   CF_REMACCESS_SUBTYPES,  /* mod_access.c */
   CF_KNOWLEDGE_SUBTYPES,  /* mod_knowledge.c */
   CF_MEASUREMENT_SUBTYPES,/* mod_measurement.c */
   
   /* update CF3_MODULES in cf3.defs.h */
   };
