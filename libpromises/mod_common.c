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

#include "mod_common.h"

#include "mod_environ.h"
#include "mod_outputs.h"
#include "mod_access.h"
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

#include "conversion.h"
#include "policy.h"
#include "syntax.h"

static const char *POLICY_ERROR_VARS_CONSTRAINT_DUPLICATE_TYPE = "Variable contains existing data type contstraint %s, tried to redefine with %s";
static const char *POLICY_ERROR_VARS_PROMISER_NUMERICAL = "Variable promises cannot have a purely numerical promiser (name)";
static const char *POLICY_ERROR_VARS_PROMISER_RESERVED = "Variable promise is using a reserved name";
static const char *POLICY_ERROR_CLASSES_PROMISER_NUMERICAL = "Classes promises cannot have a purely numerical promiser (name)";

static bool ActionCheck(const Body *body, Seq *errors)
{
    bool success = true;

    if (BodyHasConstraint(body, "log_kept")
        || BodyHasConstraint(body, "log_repaired")
        || BodyHasConstraint(body, "log_failed"))
    {
        if (!BodyHasConstraint(body, "log_string"))
        {
            SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BODY, body, "An action body with log_kept, log_repaired or log_failed is required to have a log_string attribute"));
            success = false;
        }
    }

    return success;
}

static const ConstraintSyntax action_constraints[] =
{
    ConstraintSyntaxNewOption("action_policy", "fix,warn,nop", "Whether to repair or report about non-kept promises"),
    ConstraintSyntaxNewInt("ifelapsed", CF_VALRANGE, "Number of minutes before next allowed assessment of promise. Default value: control body value"),
    ConstraintSyntaxNewInt("expireafter", CF_VALRANGE, "Number of minutes before a repair action is interrupted and retried. Default value: control body value"),
    ConstraintSyntaxNewString("log_string", "", "A message to be written to the log when a promise verification leads to a repair"),
    ConstraintSyntaxNewOption("log_level", "inform,verbose,error,log", "The reporting level sent to syslog"),
    ConstraintSyntaxNewString("log_kept", CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"),
    ConstraintSyntaxNewOption("log_priority", "emergency,alert,critical,error,warning,notice,info,debug","The priority level of the log message, as interpreted by a syslog server"),
    ConstraintSyntaxNewString("log_repaired", CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"),
    ConstraintSyntaxNewString("log_failed", CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger"),
    ConstraintSyntaxNewReal("value_kept", CF_REALRANGE, "A real number value attributed to keeping this promise"),
    ConstraintSyntaxNewReal("value_repaired", CF_REALRANGE, "A real number value attributed to reparing this promise"),
    ConstraintSyntaxNewReal("value_notkept", CF_REALRANGE, "A real number value (possibly negative) attributed to not keeping this promise"),
    ConstraintSyntaxNewBool("audit", "true/false switch for detailed audit records of this promise. Default value: false"),
    ConstraintSyntaxNewBool("background", "true/false switch for parallelizing the promise repair. Default value: false"),
    ConstraintSyntaxNewOption("report_level", "inform,verbose,error,log", "The reporting level for standard output for this promise. Default value: none"),
    ConstraintSyntaxNewString("measurement_class", "", "If set performance will be measured and recorded under this identifier"),
    ConstraintSyntaxNewNull()
};

static const BodyTypeSyntax action_body = BodyTypeSyntaxNew("action", action_constraints, ActionCheck);

static const ConstraintSyntax classes_constraints[] =
{
    ConstraintSyntaxNewOption("scope", "namespace,bundle", "Scope of the contexts set by this body"),
    ConstraintSyntaxNewStringList("promise_repaired", CF_IDRANGE, "A list of classes to be defined globally"),
    ConstraintSyntaxNewStringList("repair_failed", CF_IDRANGE, "A list of classes to be defined globally"),
    ConstraintSyntaxNewStringList("repair_denied", CF_IDRANGE, "A list of classes to be defined globally"),
    ConstraintSyntaxNewStringList("repair_timeout", CF_IDRANGE, "A list of classes to be defined globally"),
    ConstraintSyntaxNewStringList("promise_kept", CF_IDRANGE, "A list of classes to be defined globally"),
    ConstraintSyntaxNewStringList("cancel_kept", CF_IDRANGE, "A list of classes to be cancelled if the promise is kept"),
    ConstraintSyntaxNewStringList("cancel_repaired", CF_IDRANGE, "A list of classes to be cancelled if the promise is repaired"),
    ConstraintSyntaxNewStringList("cancel_notkept", CF_IDRANGE, "A list of classes to be cancelled if the promise is not kept for any reason"),
    ConstraintSyntaxNewStringList("kept_returncodes", CF_INTLISTRANGE, "A list of return codes indicating a kept command-related promise"),
    ConstraintSyntaxNewStringList("repaired_returncodes", CF_INTLISTRANGE,"A list of return codes indicating a repaired command-related promise"),
    ConstraintSyntaxNewStringList("failed_returncodes", CF_INTLISTRANGE, "A list of return codes indicating a failed command-related promise"),
    ConstraintSyntaxNewInt("persist_time", CF_VALRANGE, "A number of minutes the specified classes should remain active"),
    ConstraintSyntaxNewOption("timer_policy", "absolute,reset", "Whether a persistent class restarts its counter when rediscovered. Default value: reset"),
    ConstraintSyntaxNewNull()
};

static const BodyTypeSyntax classes_body = BodyTypeSyntaxNew("classes", classes_constraints, NULL);

const ConstraintSyntax CF_VARBODY[] =
{
    ConstraintSyntaxNewString("string", "", "A scalar string"),
    ConstraintSyntaxNewInt("int", CF_INTRANGE, "A scalar integer"),
    ConstraintSyntaxNewReal("real", CF_REALRANGE, "A scalar real number"),
    ConstraintSyntaxNewStringList("slist", "", "A list of scalar strings"),
    ConstraintSyntaxNewIntList("ilist", "A list of integers"),
    ConstraintSyntaxNewRealList("rlist", "A list of real numbers"),
    ConstraintSyntaxNewOption("policy", "free,overridable,constant,ifdefined", "The policy for (dis)allowing (re)definition of variables"),
    ConstraintSyntaxNewNull()
};

static bool CheckIdentifierNotPurelyNumerical(const char *identifier)
{
    return !((isdigit((int)*identifier)) && (IntFromString(identifier) != CF_NOINT));
}

static bool VarsParseTreeCheck(const Promise *pp, Seq *errors)
{
    bool success = true;

    if (!CheckIdentifierNotPurelyNumerical(pp->promiser))
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_PROMISE, pp,
                                         POLICY_ERROR_VARS_PROMISER_NUMERICAL));
        success = false;
    }

    if (!CheckParseVariableName(pp->promiser))
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_PROMISE, pp,
                                         POLICY_ERROR_VARS_PROMISER_RESERVED));
        success = false;
    }

    // ensure variables are declared with only one type.
    {
        char *data_type = NULL;

        for (size_t i = 0; i < SeqLength(pp->conlist); i++)
        {
            Constraint *cp = SeqAt(pp->conlist, i);

            if (IsDataType(cp->lval))
            {
                if (data_type != NULL)
                {
                    SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, cp,
                                                     POLICY_ERROR_VARS_CONSTRAINT_DUPLICATE_TYPE,
                                                     data_type, cp->lval));
                    success = false;
                }
                data_type = cp->lval;
            }
        }
    }

    return success;
}

const ConstraintSyntax CF_METABODY[] =
{
    ConstraintSyntaxNewString("string", "", "A scalar string"),
    ConstraintSyntaxNewStringList("slist", "", "A list of scalar strings"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CF_DEFAULTSBODY[] =
{
    ConstraintSyntaxNewString("if_match_regex", "", "If this regular expression matches the current value of the variable, replace it with default"),
    ConstraintSyntaxNewString("string", "", "A scalar string"),
    ConstraintSyntaxNewStringList("slist", "", "A list of scalar strings"),
    ConstraintSyntaxNewNull()
};


const ConstraintSyntax CF_CLASSBODY[] =
{
    ConstraintSyntaxNewOption("scope", "namespace,bundle", "Scope of the class set by this promise"),
    ConstraintSyntaxNewContextList("and", "Combine class sources with AND"),
    ConstraintSyntaxNewRealList("dist", "Generate a probabilistic class distribution (from strategies in cfengine 2)"),
    ConstraintSyntaxNewContext("expression", "Evaluate string expression of classes in normal form"),
    ConstraintSyntaxNewContextList("or", "Combine class sources with inclusive OR"),
    ConstraintSyntaxNewInt("persistence", CF_VALRANGE, "Make the class persistent (cached) to avoid reevaluation, time in minutes"),
    ConstraintSyntaxNewContext("not", "Evaluate the negation of string expression in normal form"),
    ConstraintSyntaxNewContextList("select_class", "Select one of the named list of classes to define based on host identity. Default value: random_selection"),
    ConstraintSyntaxNewContextList("xor", "Combine class sources with XOR"),
    ConstraintSyntaxNewNull()
};

static bool ClassesParseTreeCheck(const Promise *pp, Seq *errors)
{
    bool success = true;

    if (!CheckIdentifierNotPurelyNumerical(pp->promiser))
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_PROMISE, pp,
                                         POLICY_ERROR_CLASSES_PROMISER_NUMERICAL));
        success = false;
    }

    return success;
}

const ConstraintSyntax CFG_CONTROLBODY[] =
{
    ConstraintSyntaxNewStringList("bundlesequence", ".*", "List of promise bundles to verify in order"),
    ConstraintSyntaxNewStringList("goal_patterns", "", "A list of regular expressions that match promisees/topics considered to be organizational goals"),
    ConstraintSyntaxNewBool("ignore_missing_bundles", "If any bundles in the bundlesequence do not exist, ignore and continue. Default value: false"),
    ConstraintSyntaxNewBool("ignore_missing_inputs", "If any input files do not exist, ignore and continue. Default value: false"),
    ConstraintSyntaxNewStringList("inputs", ".*", "List of additional filenames to parse for promises"),
    ConstraintSyntaxNewString("version", "", "Scalar version string for this configuration"),
    ConstraintSyntaxNewInt("lastseenexpireafter", CF_VALRANGE, "Number of minutes after which last-seen entries are purged. Default value: one week"),
    ConstraintSyntaxNewString("output_prefix", "", "The string prefix for standard output"),
    ConstraintSyntaxNewString("domain", ".*", "Specify the domain name for this host"),
    ConstraintSyntaxNewBool("require_comments", "Warn about promises that do not have comment documentation. Default value: false"),
    ConstraintSyntaxNewInt("host_licenses_paid", CF_VALRANGE, "This promise is deprecated since CFEngine version 3.1 and is ignored. Default value: 25"),
    ConstraintSyntaxNewContextList("site_classes", "A list of classes that will represent geographical site locations for hosts. These should be defined elsewhere in the configuration in a classes promise."),
    ConstraintSyntaxNewString("syslog_host", CF_IPRANGE, "The name or address of a host to which syslog messages should be sent directly by UDP. Default value: 514"),
    ConstraintSyntaxNewInt("syslog_port", CF_VALRANGE, "The port number of a UDP syslog service"),
    ConstraintSyntaxNewBool("fips_mode", "Activate full FIPS mode restrictions. Default value: false"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFA_CONTROLBODY[] =
{
    ConstraintSyntaxNewStringList("abortclasses", ".*", "A list of classes which if defined lead to termination of cf-agent"),
    ConstraintSyntaxNewStringList("abortbundleclasses", ".*", "A list of classes which if defined lead to termination of current bundle"),
    ConstraintSyntaxNewStringList("addclasses", ".*", "A list of classes to be defined always in the current context"),
    ConstraintSyntaxNewStringList("agentaccess", ".*", "A list of user names allowed to execute cf-agent"),
    ConstraintSyntaxNewOption("agentfacility", CF_FACILITY, "The syslog facility for cf-agent. Default value: LOG_USER"),
    ConstraintSyntaxNewBool("allclassesreport", "Generate allclasses.txt report"),
    ConstraintSyntaxNewBool("alwaysvalidate", "true/false flag to determine whether configurations will always be checked before executing, or only after updates"),
    ConstraintSyntaxNewBool("auditing", "This option is deprecated, does nothing and is kept for backward compatibility. Default value: false"),
    ConstraintSyntaxNewString("binarypaddingchar", "", "Character used to pad unequal replacements in binary editing. Default value: space (ASC=32)"),
    ConstraintSyntaxNewString("bindtointerface", ".*", "Use this interface for outgoing connections"),
    ConstraintSyntaxNewBool("hashupdates", "true/false whether stored hashes are updated when change is detected in source. Default value: false"),
    ConstraintSyntaxNewString("childlibpath", ".*", "LD_LIBRARY_PATH for child processes"),
    ConstraintSyntaxNewInt("checksum_alert_time", "0,60", "The persistence time for the checksum_alert class. Default value: 10 mins"),
    ConstraintSyntaxNewOption("defaultcopytype", "mtime,atime,ctime,digest,hash,binary", "ctime or mtime differ"),
    ConstraintSyntaxNewBool("dryrun", "All talk and no action mode. Default value: false"),
    ConstraintSyntaxNewInt("editbinaryfilesize", CF_VALRANGE, "Integer limit on maximum binary file size to be edited. Default value: 100000"),
    ConstraintSyntaxNewInt("editfilesize", CF_VALRANGE, "Integer limit on maximum text file size to be edited. Default value: 100000"),
    ConstraintSyntaxNewStringList("environment", "[A-Za-z0-9_]+=.*", "List of environment variables to be inherited by children"),
    ConstraintSyntaxNewBool("exclamation", "true/false print exclamation marks during security warnings. Default value: true"),
    ConstraintSyntaxNewInt("expireafter", CF_VALRANGE, "Global default for time before on-going promise repairs are interrupted. Default value: 1 min"),
    ConstraintSyntaxNewStringList("files_single_copy", "", "List of filenames to be watched for multiple-source conflicts"),
    ConstraintSyntaxNewStringList("files_auto_define", "", "List of filenames to define classes if copied"),
    ConstraintSyntaxNewBool("hostnamekeys", "true/false label ppkeys by hostname not IP address. Default value: false"),
    ConstraintSyntaxNewInt("ifelapsed", CF_VALRANGE, "Global default for time that must elapse before promise will be rechecked. Default value: 1"),
    ConstraintSyntaxNewBool("inform", "true/false set inform level default. Default value: false"),
    ConstraintSyntaxNewBool("intermittency", "This option is deprecated, does nothing and is kept for backward compatibility. Default value: false"),
    ConstraintSyntaxNewInt("max_children", CF_VALRANGE, "Maximum number of background tasks that should be allowed concurrently. Default value: 1 concurrent agent promise"),
    ConstraintSyntaxNewInt("maxconnections", CF_VALRANGE, "Maximum number of outgoing connections to cf-serverd. Default value: 30 remote queries"),
    ConstraintSyntaxNewBool("mountfilesystems", "true/false mount any filesystems promised. Default value: false"),
    ConstraintSyntaxNewBool("nonalphanumfiles", "true/false warn about filenames with no alphanumeric content. Default value: false"),
    ConstraintSyntaxNewString("repchar", ".", "The character used to canonize pathnames in the file repository. Default value: _"),
    ConstraintSyntaxNewStringList("refresh_processes", CF_IDRANGE, "Reload the process table before verifying the bundles named in this list (lazy evaluation)"),
    ConstraintSyntaxNewString("default_repository", CF_ABSPATHRANGE, "Path to the default file repository. Default value: in situ"),
    ConstraintSyntaxNewBool("secureinput", "true/false check whether input files are writable by unauthorized users. Default value: false"),
    ConstraintSyntaxNewInt("sensiblecount", CF_VALRANGE, "Minimum number of files a mounted filesystem is expected to have. Default value: 2 files"),
    ConstraintSyntaxNewInt("sensiblesize", CF_VALRANGE, "Minimum number of bytes a mounted filesystem is expected to have. Default value: 1000 bytes"),
    ConstraintSyntaxNewBool("skipidentify", "Do not send IP/name during server connection because address resolution is broken. Default value: false"),
    ConstraintSyntaxNewStringList("suspiciousnames", "", "List of names to warn about if found during any file search"),
    ConstraintSyntaxNewBool("syslog", "true/false switches on output to syslog at the inform level. Default value: false"),
    ConstraintSyntaxNewBool("track_value", "true/false switches on tracking of promise valuation. Default value: false"),
    ConstraintSyntaxNewStringList("timezone", "", "List of allowed timezones this machine must comply with"),
    ConstraintSyntaxNewInt("default_timeout", CF_VALRANGE, "Maximum time a network connection should attempt to connect. Default value: 10 seconds"),
    ConstraintSyntaxNewBool("verbose", "true/false switches on verbose standard output. Default value: false"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFS_CONTROLBODY[] =
{
    ConstraintSyntaxNewStringList("allowallconnects", "","List of IPs or hostnames that may have more than one connection to the server port"),
    ConstraintSyntaxNewStringList("allowconnects", "", "List of IPs or hostnames that may connect to the server port"),
    ConstraintSyntaxNewStringList("allowusers", "", "List of usernames who may execute requests from this server"),
    ConstraintSyntaxNewBool("auditing", "true/false activate auditing of server connections. Default value: false"),
    ConstraintSyntaxNewString("bindtointerface", "", "IP of the interface to which the server should bind on multi-homed hosts"),
    ConstraintSyntaxNewString("cfruncommand", CF_PATHRANGE, "Path to the cf-agent command or cf-execd wrapper for remote execution"),
    ConstraintSyntaxNewInt("call_collect_interval", CF_VALRANGE, "The interval in minutes in between collect calls to the policy hub offering a tunnel for report collection (Enterprise)"),
    ConstraintSyntaxNewInt("collect_window", CF_VALRANGE, "A time in seconds that a collect-call tunnel remains open to a hub to attempt a report transfer before it is closed (Enterprise)"),
    ConstraintSyntaxNewBool("denybadclocks", "true/false accept connections from hosts with clocks that are out of sync. Default value: true"),
    ConstraintSyntaxNewStringList("denyconnects", "", "List of IPs or hostnames that may NOT connect to the server port"),
    ConstraintSyntaxNewStringList("dynamicaddresses", "", "List of IPs or hostnames for which the IP/name binding is expected to change"),
    ConstraintSyntaxNewBool("hostnamekeys", "true/false store keys using hostname lookup instead of IP addresses. Default value: false"),
    ConstraintSyntaxNewInt("keycacheTTL", CF_VALRANGE, "Maximum number of hours to hold public keys in the cache. Default value: 24"),
    ConstraintSyntaxNewBool("logallconnections", "true/false causes the server to log all new connections to syslog. Default value: false"),
    ConstraintSyntaxNewBool("logencryptedtransfers", "true/false log all successful transfers required to be encrypted. Default value: false"),
    ConstraintSyntaxNewInt("maxconnections", CF_VALRANGE, "Maximum number of connections that will be accepted by cf-serverd. Default value: 30 remote queries"),
    ConstraintSyntaxNewInt("port", "1024,99999", "Default port for cfengine server. Default value: 5308"),
    ConstraintSyntaxNewOption("serverfacility", CF_FACILITY, "Menu option for syslog facility level. Default value: LOG_USER"),
    ConstraintSyntaxNewStringList("skipverify", "", "List of IPs or hostnames for which we expect no DNS binding and cannot verify"),
    ConstraintSyntaxNewStringList("trustkeysfrom", "", "List of IPs from whom we accept public keys on trust"),
    ConstraintSyntaxNewBool("listen", "true/false enable server deamon to listen on defined port. Default value: true"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFM_CONTROLBODY[] =
{
    ConstraintSyntaxNewReal("forgetrate", "0,1", "Decimal fraction [0,1] weighting of new values over old in 2d-average computation. Default value: 0.6"),
    ConstraintSyntaxNewOption("monitorfacility", CF_FACILITY, "Menu option for syslog facility. Default value: LOG_USER"),
    ConstraintSyntaxNewBool("histograms", "Ignored, kept for backward compatibility. Default value: true"),
    ConstraintSyntaxNewBool("tcpdump", "true/false use tcpdump if found. Default value: false"),
    ConstraintSyntaxNewString("tcpdumpcommand", CF_ABSPATHRANGE, "Path to the tcpdump command on this system"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFR_CONTROLBODY[] =
{
    ConstraintSyntaxNewStringList("hosts", "", "List of host or IP addresses to attempt connection with"),
    ConstraintSyntaxNewInt("port", "1024,99999", "Default port for cfengine server. Default value: 5308"),
    ConstraintSyntaxNewBool("force_ipv4", "true/false force use of ipv4 in connection. Default value: false"),
    ConstraintSyntaxNewBool("trustkey", "true/false automatically accept all keys on trust from servers. Default value: false"),
    ConstraintSyntaxNewBool("encrypt", "true/false encrypt connections with servers. Default value: false"),
    ConstraintSyntaxNewBool("background_children", "true/false parallelize connections to servers. Default value: false"),
    ConstraintSyntaxNewInt("max_children", CF_VALRANGE, "Maximum number of simultaneous connections to attempt. Default value: 50 runagents"),
    ConstraintSyntaxNewBool("output_to_file", "true/false whether to send collected output to file(s). Default value: false"),
    ConstraintSyntaxNewString("output_directory", CF_ABSPATHRANGE, "Directory where the output is stored"),
    ConstraintSyntaxNewInt("timeout", "1,9999", "Connection timeout, sec"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFEX_CONTROLBODY[] = /* enum cfexcontrol */
{
    ConstraintSyntaxNewInt("splaytime", CF_VALRANGE, "Time in minutes to splay this host based on its name hash. Default value: 0"),
    ConstraintSyntaxNewString("mailfrom", ".*@.*", "Email-address cfengine mail appears to come from"),
    ConstraintSyntaxNewString("mailto", ".*@.*", "Email-address cfengine mail is sent to"),
    ConstraintSyntaxNewString("smtpserver", ".*", "Name or IP of a willing smtp server for sending email"),
    ConstraintSyntaxNewInt("mailmaxlines", "0,1000", "Maximum number of lines of output to send by email. Default value: 30"),
    ConstraintSyntaxNewStringList("schedule", "", "The class schedule used by cf-execd for activating cf-agent"),
    ConstraintSyntaxNewOption("executorfacility", CF_FACILITY, "Menu option for syslog facility level. Default value: LOG_USER"),
    ConstraintSyntaxNewString("exec_command", CF_ABSPATHRANGE,"The full path and command to the executable run by default (overriding builtin)"),
    ConstraintSyntaxNewInt("agent_expireafter", "0,10080", "Maximum agent runtime (in minutes). Default value: 10080"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFH_CONTROLBODY[] =  /* enum cfh_control */
{
    ConstraintSyntaxNewString("export_zenoss", CF_PATHRANGE, "Generate report for Zenoss integration"),
    ConstraintSyntaxNewStringList("exclude_hosts", "", "A list of IP addresses of hosts to exclude from report collection"),
    ConstraintSyntaxNewStringList("hub_schedule", "", "The class schedule used by cf-hub for report collation"),
    ConstraintSyntaxNewInt("port", "1024,99999", "Default port for contacting hub nodes. Default value: 5308"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFFILE_CONTROLBODY[] =  /* enum cfh_control */
{
    ConstraintSyntaxNewString("namespace", CF_IDRANGE, "Switch to a private namespace to protect current file from duplicate definitions"),
    ConstraintSyntaxNewNull()
};

/* This list is for checking free standing body lval => rval bindings */

const BodyTypeSyntax CONTROL_BODIES[] =
{
    BodyTypeSyntaxNew(CF_COMMONC, CFG_CONTROLBODY, NULL),
    BodyTypeSyntaxNew(CF_AGENTC, CFA_CONTROLBODY, NULL),
    BodyTypeSyntaxNew(CF_SERVERC, CFS_CONTROLBODY, NULL),
    BodyTypeSyntaxNew(CF_MONITORC, CFM_CONTROLBODY, NULL),
    BodyTypeSyntaxNew(CF_RUNC, CFR_CONTROLBODY, NULL),
    BodyTypeSyntaxNew(CF_EXECC, CFEX_CONTROLBODY, NULL),
    BodyTypeSyntaxNew(CF_HUBC, CFH_CONTROLBODY, NULL),
    BodyTypeSyntaxNew("file", CFFILE_CONTROLBODY, NULL),

    //  get others from modules e.g. "agent","files",CF_FILES_BODIES,

    BodyTypeSyntaxNewNull()
};

/*********************************************************/
/*                                                       */
/* Constraint values/types                               */
/*                                                       */
/*********************************************************/

 /* This is where we place lval => rval bindings that
    apply to more than one promise_type, e.g. generic
    processing behavioural details */

const ConstraintSyntax CF_COMMON_BODIES[] =
{
    ConstraintSyntaxNewBody("action", &action_body, "Output behaviour"),
    ConstraintSyntaxNewBody("classes", &classes_body, "Signalling behaviour"),
    ConstraintSyntaxNewString("comment", "", "A comment about this promise's real intention that follows through the program"),
    ConstraintSyntaxNewStringList("depends_on", "","A list of promise handles that this promise builds on or depends on somehow (for knowledge management)"),
    ConstraintSyntaxNewString("handle", "", "A unique id-tag string for referring to this as a promisee elsewhere"),
    ConstraintSyntaxNewString("ifvarclass", "", "Extended classes ANDed with context"),
    ConstraintSyntaxNewStringList("meta", "", "User-data associated with policy, e.g. key=value strings"),
    ConstraintSyntaxNewNull()
};

 /* This is where we place promise promise_types that apply
    to more than one type of bundle, e.g. agent,server.. */

const PromiseTypeSyntax CF_COMMON_PROMISE_TYPES[] =
{

    PromiseTypeSyntaxNew("*", "classes", CF_CLASSBODY, &ClassesParseTreeCheck),
    PromiseTypeSyntaxNew("*", "defaults", CF_DEFAULTSBODY, NULL),
    PromiseTypeSyntaxNew("*", "meta", CF_METABODY, NULL),
    PromiseTypeSyntaxNew("*", "reports", CF_REPORT_BODIES, NULL),
    PromiseTypeSyntaxNew("*", "vars", CF_VARBODY, &VarsParseTreeCheck),
    PromiseTypeSyntaxNew("*", "*", CF_COMMON_BODIES, NULL),
    PromiseTypeSyntaxNewNull()
};

/*********************************************************/
/* THIS IS WHERE TO ATTACH SYNTAX MODULES                */
/*********************************************************/

/* Read in all parsable Bundle definitions */
/* REMEMBER TO REGISTER THESE IN cf3.extern.h */

const PromiseTypeSyntax *CF_ALL_PROMISE_TYPES[] =
{
    CF_COMMON_PROMISE_TYPES,         /* Add modules after this, mod_report.c is here */
    CF_EXEC_PROMISE_TYPES,           /* mod_exec.c */
    CF_DATABASES_PROMISE_TYPES,      /* mod_databases.c */
    CF_ENVIRONMENT_PROMISE_TYPES,    /* mod_environ.c */
    CF_FILES_PROMISE_TYPES,          /* mod_files.c */
    CF_METHOD_PROMISE_TYPES,         /* mod_methods.c */
    CF_OUTPUTS_PROMISE_TYPES,        /* mod_outputs.c */
    CF_PACKAGES_PROMISE_TYPES,       /* mod_packages.c */
    CF_PROCESS_PROMISE_TYPES,        /* mod_process.c */
    CF_SERVICES_PROMISE_TYPES,       /* mod_services.c */
    CF_STORAGE_PROMISE_TYPES,        /* mod_storage.c */
    CF_REMACCESS_PROMISE_TYPES,      /* mod_access.c */
    CF_MEASUREMENT_PROMISE_TYPES,    /* mod_measurement.c */
};

const int CF3_MODULES = (sizeof(CF_ALL_PROMISE_TYPES) / sizeof(CF_ALL_PROMISE_TYPES[0]));


CommonControl CommonControlFromString(const char *lval)
{
    int i = 0;
    for (const ConstraintSyntax *s = CFG_CONTROLBODY; s->lval; s++, i++)
    {
        if (strcmp(lval, s->lval) == 0)
        {
            return (CommonControl)i;
        }
    }

    return COMMON_CONTROL_NONE;
}
