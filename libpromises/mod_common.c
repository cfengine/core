/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/* This is a root node in the syntax tree */

#include <mod_common.h>

#include <mod_environ.h>
#include <mod_outputs.h>
#include <mod_access.h>
#include <mod_storage.h>
#include <mod_databases.h>
#include <mod_packages.h>
#include <mod_report.h>
#include <mod_files.h>
#include <mod_exec.h>
#include <mod_methods.h>
#include <mod_process.h>
#include <mod_services.h>
#include <mod_measurement.h>
#include <mod_knowledge.h>
#include <mod_users.h>

#include <conversion.h>
#include <policy.h>
#include <syntax.h>

#define CF_LOGRANGE    "stdout|udp_syslog|(\042?[a-zA-Z]:\\\\.*)|(/.*)"
#define CF_FACILITY "LOG_USER,LOG_DAEMON,LOG_LOCAL0,LOG_LOCAL1,LOG_LOCAL2,LOG_LOCAL3,LOG_LOCAL4,LOG_LOCAL5,LOG_LOCAL6,LOG_LOCAL7"

static const char *const POLICY_ERROR_VARS_CONSTRAINT_DUPLICATE_TYPE =
    "Variable contains existing data type contstraint %s, tried to "
    "redefine with %s";
static const char *const POLICY_ERROR_VARS_PROMISER_NUMERICAL =
    "Variable promises cannot have a purely numerical name (promiser)";
static const char *const POLICY_ERROR_VARS_PROMISER_INVALID =
    "Variable promise is using an invalid name (promiser)";
static const char *const POLICY_ERROR_CLASSES_PROMISER_NUMERICAL =
    "Classes promises cannot have a purely numerical name (promiser)";

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
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("action_policy", "fix,warn,nop", "Whether to repair or report about non-kept promises", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("ifelapsed", CF_VALRANGE, "Number of minutes before next allowed assessment of promise. Default value: control body value", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("expireafter", CF_VALRANGE, "Number of minutes before a repair action is interrupted and retried. Default value: control body value", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("log_string", "", "A message to be written to the log when a promise verification leads to a repair", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("log_level", "inform,verbose,error,log", "The reporting level sent to syslog", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("log_kept", CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("log_priority", "emergency,alert,critical,error,warning,notice,info,debug","The priority level of the log message, as interpreted by a syslog server", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("log_repaired", CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("log_failed", CF_LOGRANGE,"This should be filename of a file to which log_string will be saved, if undefined it goes to the system logger", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewReal("value_kept", CF_REALRANGE, "A real number value attributed to keeping this promise", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewReal("value_repaired", CF_REALRANGE, "A real number value attributed to reparing this promise", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewReal("value_notkept", CF_REALRANGE, "A real number value (possibly negative) attributed to not keeping this promise", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewBool("audit", "true/false switch for detailed audit records of this promise. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("background", "true/false switch for parallelizing the promise repair. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("report_level", "inform,verbose,error,log", "The reporting level for standard output for this promise. Default value: none", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("measurement_class", "", "If set performance will be measured and recorded under this identifier", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax action_body = BodySyntaxNew("action", action_constraints, ActionCheck, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax classes_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("scope", "namespace,bundle", "Scope of the contexts set by this body", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("promise_repaired", CF_IDRANGE, "A list of classes to be defined globally", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("repair_failed", CF_IDRANGE, "A list of classes to be defined globally", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("repair_denied", CF_IDRANGE, "A list of classes to be defined globally", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("repair_timeout", CF_IDRANGE, "A list of classes to be defined globally", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("promise_kept", CF_IDRANGE, "A list of classes to be defined globally", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("cancel_kept", CF_IDRANGE, "A list of classes to be cancelled if the promise is kept", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("cancel_repaired", CF_IDRANGE, "A list of classes to be cancelled if the promise is repaired", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("cancel_notkept", CF_IDRANGE, "A list of classes to be cancelled if the promise is not kept for any reason", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("kept_returncodes", CF_INTLISTRANGE, "A list of return codes indicating a kept command-related promise", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("repaired_returncodes", CF_INTLISTRANGE,"A list of return codes indicating a repaired command-related promise", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("failed_returncodes", CF_INTLISTRANGE, "A list of return codes indicating a failed command-related promise", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("persist_time", CF_VALRANGE, "A number of minutes the specified classes should remain active", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("timer_policy", "absolute,reset", "Whether a persistent class restarts its counter when rediscovered. Default value: reset", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax classes_body = BodySyntaxNew("classes", classes_constraints, NULL, SYNTAX_STATUS_NORMAL);

const ConstraintSyntax CF_VARBODY[] =
{
    ConstraintSyntaxNewString("string", "", "A scalar string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("int", CF_INTRANGE, "A scalar integer", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewReal("real", CF_REALRANGE, "A scalar real number", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("slist", "", "A list of scalar strings", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntList("ilist", "A list of integers", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewRealList("rlist", "A list of real numbers", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContainer("data", "A data container", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("policy", "free,overridable,constant,ifdefined", "The policy for (dis)allowing (re)definition of variables", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static bool CheckIdentifierNotPurelyNumerical(const char *identifier)
{
    if (*identifier == '\0')
    {
        return true;
    }

    for (const char *check = identifier; *check != '\0' && check - identifier < CF_BUFSIZE; check++)
    {
        if (!isdigit(*check))
        {
            return true;
        }
    }

    return false;
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
                                         POLICY_ERROR_VARS_PROMISER_INVALID));
        success = false;
    }

    // ensure variables are declared with only one type.
    {
        char *data_type = NULL;

        for (size_t i = 0; i < SeqLength(pp->conlist); i++)
        {
            Constraint *cp = SeqAt(pp->conlist, i);

            if (DataTypeFromString(cp->lval) != CF_DATA_TYPE_NONE)
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
    ConstraintSyntaxNewString("string", "", "A scalar string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("slist", "", "A list of scalar strings", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContainer("data", "A data container", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CF_DEFAULTSBODY[] =
{
    ConstraintSyntaxNewString("if_match_regex", "", "If this regular expression matches the current value of the variable, replace it with default", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("string", "", "A scalar string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("slist", "", "A list of scalar strings", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};


const ConstraintSyntax CF_CLASSBODY[] =
{
    ConstraintSyntaxNewOption("scope", "namespace,bundle", "Scope of the class set by this promise", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContextList("and", "Combine class sources with AND", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewRealList("dist", "Generate a probabilistic class distribution (from strategies in cfengine 2)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContext("expression", "Evaluate string expression of classes in normal form", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContextList("or", "Combine class sources with inclusive OR", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("persistence", CF_VALRANGE, "Make the class persistent (cached) to avoid reevaluation, time in minutes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContext("not", "Evaluate the negation of string expression in normal form", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContextList("select_class", "Select one of the named list of classes to define based on host identity. Default value: random_selection", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContextList("xor", "Combine class sources with XOR", SYNTAX_STATUS_NORMAL),
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

const ConstraintSyntax CFG_CONTROLBODY[COMMON_CONTROL_MAX + 1] =
{
    ConstraintSyntaxNewStringList("bundlesequence", ".*", "List of promise bundles to verify in order", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("goal_patterns", "", "A list of regular expressions that match promisees/topics considered to be organizational goals", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ignore_missing_bundles", "If any bundles in the bundlesequence do not exist, ignore and continue. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ignore_missing_inputs", "If any input files do not exist, ignore and continue. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("inputs", ".*", "List of additional filenames to parse for promises", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("version", "", "Scalar version string for this configuration", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("lastseenexpireafter", CF_VALRANGE, "Number of minutes after which last-seen entries are purged. Default value: one week", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("output_prefix", "", "The string prefix for standard output", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("domain", ".*", "Specify the domain name for this host", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("require_comments", "Warn about promises that do not have comment documentation. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("host_licenses_paid", CF_VALRANGE, "This promise is deprecated since CFEngine version 3.1 and is ignored. Default value: 25", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewContextList("site_classes", "A list of classes that will represent geographical site locations for hosts. These should be defined elsewhere in the configuration in a classes promise.", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("syslog_host", CF_IPRANGE, "The name or address of a host to which syslog messages should be sent directly by UDP. Default value: localhost", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("syslog_port", CF_VALRANGE, "The port number of a UDP syslog service. Default value: 514", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("fips_mode", "Activate full FIPS mode restrictions. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewReal("bwlimit", CF_VALRANGE, "Limit outgoing protocol bandwidth in Bytes per second", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("cache_system_functions", "Cache the result of system functions. Default value: true", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("protocol_version", "0,undefined,1,classic,2,latest", "CFEngine protocol version to use when connecting to the server. Default: \"latest\"", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tls_ciphers", "", "List of acceptable ciphers in outgoing TLS connections, defaults to OpenSSL's default. For syntax help see man page for \"openssl ciphers\"", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tls_min_version", "", "Minimum acceptable TLS version for outgoing connections, defaults to OpenSSL's default", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("package_inventory", ".*", "Name of the package manager used for software inventory management", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_module", ".*", "Name of the default package manager", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFA_CONTROLBODY[] =
{
    ConstraintSyntaxNewStringList("abortclasses", ".*", "A list of classes which if defined in an agent bundle lead to termination of cf-agent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("abortbundleclasses", ".*", "A list of classes which if defined lead to termination of current bundle", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("addclasses", ".*", "A list of classes to be defined always in the current context", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("agentaccess", ".*", "A list of user names allowed to execute cf-agent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("agentfacility", CF_FACILITY, "The syslog facility for cf-agent. Default value: LOG_USER", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("allclassesreport", "Generate allclasses.txt report", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("alwaysvalidate", "true/false flag to determine whether configurations will always be checked before executing, or only after updates", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("auditing", "This option is deprecated, does nothing and is kept for backward compatibility. Default value: false", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("binarypaddingchar", "", "Character used to pad unequal replacements in binary editing. Default value: space (ASC=32)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("bindtointerface", ".*", "Use this interface for outgoing connections", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("hashupdates", "true/false whether stored hashes are updated when change is detected in source. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("childlibpath", ".*", "LD_LIBRARY_PATH for child processes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("checksum_alert_time", "0,60", "The persistence time for the checksum_alert class. Default value: 10 mins", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("defaultcopytype", "mtime,atime,ctime,digest,hash,binary", "ctime or mtime differ", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("dryrun", "All talk and no action mode. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("editbinaryfilesize", CF_VALRANGE, "Integer limit on maximum binary file size to be edited. Default value: 100000", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("editfilesize", CF_VALRANGE, "Integer limit on maximum text file size to be edited. Default value: 100000", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("environment", "[A-Za-z0-9_]+=.*", "List of environment variables to be inherited by children", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("exclamation", "true/false print exclamation marks during security warnings. Default value: true", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewInt("expireafter", CF_VALRANGE, "Global default for time before on-going promise repairs are interrupted. Default value: 1 min", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("files_single_copy", "", "List of filenames to be watched for multiple-source conflicts", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("files_auto_define", "", "List of filenames to define classes if copied", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("hostnamekeys", "true/false label ppkeys by hostname not IP address. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("ifelapsed", CF_VALRANGE, "Global default for time that must elapse before promise will be rechecked. Default value: 1", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("inform", "true/false set inform level default. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("intermittency", "This option is deprecated, does nothing and is kept for backward compatibility. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("max_children", CF_VALRANGE, "Maximum number of background tasks that should be allowed concurrently. Default value: 1 concurrent agent promise", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("maxconnections", CF_VALRANGE, "Maximum number of outgoing connections to cf-serverd. Default value: 30 remote queries", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("mountfilesystems", "true/false mount any filesystems promised. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("nonalphanumfiles", "true/false warn about filenames with no alphanumeric content. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("repchar", ".", "The character used to canonize pathnames in the file repository. Default value: _", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("refresh_processes", CF_IDRANGE, "Reload the process table before verifying the bundles named in this list (lazy evaluation)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("default_repository", CF_ABSPATHRANGE, "Path to the default file repository. Default value: in situ", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("secureinput", "true/false check whether input files are writable by unauthorized users. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("sensiblecount", CF_VALRANGE, "Minimum number of files a mounted filesystem is expected to have. Default value: 2 files", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("sensiblesize", CF_VALRANGE, "Minimum number of bytes a mounted filesystem is expected to have. Default value: 1000 bytes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("skipidentify", "Do not send IP/name during server connection because address resolution is broken. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("suspiciousnames", "", "List of names to skip and warn about if found during any file search", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("syslog", "true/false switches on output to syslog at the inform level. Default value: false", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewBool("track_value", "true/false switches on tracking of promise valuation. Default value: false", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("timezone", "", "List of allowed timezones this machine must comply with", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("default_timeout", CF_VALRANGE, "Maximum time a network connection should attempt to connect. Default value: 10 seconds", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("verbose", "true/false switches on verbose standard output. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("report_class_log", "true/false enables logging classes at the end of agent execution. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("select_end_match_eof", "Set the default behavior of select_end_match_eof in edit_line promises. Default: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFS_CONTROLBODY[SERVER_CONTROL_MAX + 1] =
{
    ConstraintSyntaxNewStringList("allowallconnects", "","List of IPs or hostnames that may have more than one connection to the server port", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("allowconnects", "", "List of IPs or hostnames that may connect to the server port", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("allowusers", "", "List of usernames who may execute requests from this server", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("auditing", "true/false activate auditing of server connections. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("bindtointerface", "", "IP of the interface to which the server should bind on multi-homed hosts", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("cfruncommand", CF_PATHRANGE, "Path to the cf-agent command or cf-execd wrapper for remote execution", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("call_collect_interval", CF_VALRANGE, "The interval in minutes in between collect calls to the policy hub offering a tunnel for report collection (Enterprise)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("collect_window", CF_VALRANGE, "A time in seconds that a collect-call tunnel remains open to a hub to attempt a report transfer before it is closed (Enterprise)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("denybadclocks", "true/false accept connections from hosts with clocks that are out of sync. Default value: true", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("denyconnects", "", "List of IPs or hostnames that may NOT connect to the server port", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("dynamicaddresses", "", "List of IPs or hostnames for which the IP/name binding is expected to change", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("hostnamekeys", "true/false store keys using hostname lookup instead of IP addresses. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("keycacheTTL", CF_VALRANGE, "Maximum number of hours to hold public keys in the cache. Default value: 24", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewBool("logallconnections", "true/false causes the server to log all new connections to syslog. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("logencryptedtransfers", "true/false log all successful transfers required to be encrypted. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("maxconnections", CF_VALRANGE, "Maximum number of connections that will be accepted by cf-serverd. Default value: 30 remote queries", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("port", "1,65535", "Default port for cfengine server. Default value: 5308", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("serverfacility", CF_FACILITY, "Menu option for syslog facility level. Default value: LOG_USER", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("skipverify", "", "This option is deprecated, does nothing and is kept for backward compatibility.", SYNTAX_STATUS_DEPRECATED),
    ConstraintSyntaxNewStringList("trustkeysfrom", "", "List of IPs from whom we accept public keys on trust", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("listen", "true/false enable server daemon to listen on defined port. Default value: true", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("allowciphers", "", "List of ciphers the server accepts. For Syntax help see man page for \"openssl ciphers\". Default is \"AES256-GCM-SHA384:AES256-SHA\"", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("allowlegacyconnects", "", "List of IPs from whom we accept legacy protocol connections", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("allowtlsversion", "", "Minimum TLS version allowed for incoming connections", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFM_CONTROLBODY[] =
{
    ConstraintSyntaxNewReal("forgetrate", "0,1", "Decimal fraction [0,1] weighting of new values over old in 2d-average computation. Default value: 0.6", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("monitorfacility", CF_FACILITY, "Menu option for syslog facility. Default value: LOG_USER", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("histograms", "Ignored, kept for backward compatibility. Default value: true", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("tcpdump", "true/false use tcpdump if found. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tcpdumpcommand", CF_ABSPATHRANGE, "Path to the tcpdump command on this system", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFR_CONTROLBODY[] =
{
    ConstraintSyntaxNewStringList("hosts", "", "List of host or IP addresses to attempt connection with", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("port", "1,65535", "Default port for cfengine server. Default value: 5308", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("force_ipv4", "true/false force use of ipv4 in connection. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("trustkey", "true/false automatically accept all keys on trust from servers. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("encrypt", "true/false encrypt connections with servers. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("background_children", "true/false parallelize connections to servers. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("max_children", CF_VALRANGE, "Maximum number of simultaneous connections to attempt. Default value: 50 runagents", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("output_to_file", "true/false whether to send collected output to file(s). Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("output_directory", CF_ABSPATHRANGE, "Directory where the output is stored", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("timeout", "1,9999", "Connection timeout, sec", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFEX_CONTROLBODY[] = /* enum cfexcontrol */
{
    ConstraintSyntaxNewInt("splaytime", CF_VALRANGE, "Time in minutes to splay this host based on its name hash. Default value: 0", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("mailfrom", ".*@.*", "Email-address cfengine mail appears to come from", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("mailto", ".*@.*", "Email-address cfengine mail is sent to", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("mailsubject", "", "Define a custom mailsubject for the email message", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("smtpserver", ".*", "Name or IP of a willing smtp server for sending email", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("mailmaxlines", "0,1000", "Maximum number of lines of output to send by email. Default value: 30", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("mailfilter_include", "", "Which lines from the cf-agent output will be included in emails (regular expression)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("mailfilter_exclude", "", "Which lines from the cf-agent output will be excluded in emails (regular expression)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("schedule", "", "The class schedule used by cf-execd for activating cf-agent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("executorfacility", CF_FACILITY, "Menu option for syslog facility level. Default value: LOG_USER", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("exec_command", CF_ABSPATHRANGE,"The full path and command to the executable run by default (overriding builtin)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("agent_expireafter", "0,10080", "Maximum agent runtime (in minutes). Default value: 120", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFH_CONTROLBODY[] =  /* enum cfh_control */
{
    ConstraintSyntaxNewString("export_zenoss", CF_PATHRANGE, "Generate report for Zenoss integration", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("exclude_hosts", "", "A list of IP addresses of hosts to exclude from report collection", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("hub_schedule", "", "The class schedule used by cf-hub for report collation", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("port", "1,65535", "Default port for contacting hub nodes. Default value: 5308", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("client_history_timeout", "1,65535", "Threshold in hours over which if client did not report, hub will start query for full state of the host and discard all accumulated report history on the client. Default value: 6 hours", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax file_control_constraints[] =  /* enum cfh_control */
{
    ConstraintSyntaxNewString("namespace", "[a-zA-Z_][a-zA-Z0-9_]*", "Switch to a private namespace to protect current file from duplicate definitions", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("inputs", ".*", "List of additional filenames to parse for promises", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CFRE_CONTROLBODY[] = /* enum cfrecontrol */
{
    ConstraintSyntaxNewString("aggregation_point", CF_ABSPATHRANGE, "The root directory of the data cache for CMDB aggregation", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("auto_scaling", CF_BOOL, "true/false whether to auto-scale graph output to optimize use of space. Default value: true", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("build_directory", ".*", "The directory in which to generate output files", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("csv2xml", "", "A list of csv formatted files in the build directory to convert to simple xml", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("error_bars", CF_BOOL, "true/false whether to generate error bars on graph output", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("html_banner", "", "HTML code for a banner to be added to rendered in html after the header", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("html_embed", CF_BOOL, "If true, no header and footer tags will be added to html output", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("html_footer", "", "HTML code for a page footer to be added to rendered in html before the end body tag", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("query_engine", "", "Name of a dynamic web-page used to accept and drive queries in a browser", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOptionList("reports", "all,audit,performance,all_locks,active_locks,hashes,classes,last_seen,monitor_now,monitor_history,monitor_summary,compliance,setuid,file_changes,installed_software,software_patches,value,variables", "A list of reports that may be generated", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("report_output", "csv,html,text,xml", "Menu option for generated output format. Applies only to text reports, graph data remain in xydy format.", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("style_sheet", "", "Name of a style-sheet to be used in rendering html output (added to headers)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("time_stamps", CF_BOOL, "true/false whether to generate timestamps in the output directory name", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};


const ConstraintSyntax CFK_CONTROLBODY[] =
{
    ConstraintSyntaxNewString("build_directory", ".*", "The directory in which to generate output files", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("document_root", ".*", "The directory in which the web root resides", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("generate_manual", CF_BOOL, "true/false generate texinfo manual page skeleton for this version", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("graph_directory", CF_ABSPATHRANGE, "Path to directory where rendered .png files will be created", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("graph_output", CF_BOOL, "true/false generate png visualization of topic map if possible (requires lib)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("html_banner", "", "HTML code for a banner to be added to rendered in html after the header", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("html_footer", "", "HTML code for a page footer to be added to rendered in html before the end body tag", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("id_prefix", ".*", "The LTM identifier prefix used to label topic maps (used for disambiguation in merging)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("manual_source_directory", CF_ABSPATHRANGE, "Path to directory where raw text about manual topics is found (defaults to build_directory)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("query_engine", "", "Name of a dynamic web-page used to accept and drive queries in a browser", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("query_output", "html,text", "Menu option for generated output format", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("sql_type", "mysql,postgres", "Menu option for supported database type", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("sql_database", "", "Name of database used for the topic map", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("sql_owner", "", "User id of sql database user", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("sql_passwd", "", "Embedded password for accessing sql database", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("sql_server", "", "Name or IP of database server (or localhost)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("sql_connection_db", "", "The name of an existing database to connect to in order to create/manage other databases", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("style_sheet", "", "Name of a style-sheet to be used in rendering html output (added to headers)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("view_projections", CF_BOOL, "Perform view-projection analytics in graph generation", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};


/* This list is for checking free standing body lval => rval bindings */

const BodySyntax CONTROL_BODIES[] =
{
    BodySyntaxNew(CF_COMMONC, CFG_CONTROLBODY, NULL, SYNTAX_STATUS_NORMAL),
    BodySyntaxNew(CF_AGENTC, CFA_CONTROLBODY, NULL, SYNTAX_STATUS_NORMAL),
    BodySyntaxNew(CF_SERVERC, CFS_CONTROLBODY, NULL, SYNTAX_STATUS_NORMAL),
    BodySyntaxNew(CF_MONITORC, CFM_CONTROLBODY, NULL, SYNTAX_STATUS_NORMAL),
    BodySyntaxNew(CF_RUNC, CFR_CONTROLBODY, NULL, SYNTAX_STATUS_NORMAL),
    BodySyntaxNew(CF_EXECC, CFEX_CONTROLBODY, NULL, SYNTAX_STATUS_NORMAL),
    BodySyntaxNew(CF_HUBC, CFH_CONTROLBODY, NULL, SYNTAX_STATUS_NORMAL),
    BodySyntaxNew("file", file_control_constraints, NULL, SYNTAX_STATUS_NORMAL),

    BodySyntaxNew("reporter", CFRE_CONTROLBODY, NULL, SYNTAX_STATUS_REMOVED),
    BodySyntaxNew("knowledge", CFK_CONTROLBODY, NULL, SYNTAX_STATUS_REMOVED),

    //  get others from modules e.g. "agent","files",CF_FILES_BODIES,

    BodySyntaxNewNull()
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
    ConstraintSyntaxNewBody("action", &action_body, "Output behaviour", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("classes", &classes_body, "Signalling behaviour", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("comment", "", "A comment about this promise's real intention that follows through the program", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("depends_on", "","A list of promise handles that this promise builds on or depends on somehow (for knowledge management)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("handle", "", "A unique id-tag string for referring to this as a promisee elsewhere", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ifvarclass", "", "Extended classes ANDed with context (alias for 'if')", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("if", "", "Extended classes ANDed with context", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("unless", "", "Negated 'if'", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("meta", "", "User-data associated with policy, e.g. key=value strings", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("with", "", "A string that will replace every instance of $(with)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

 /* This is where we place promise promise_types that apply
    to more than one type of bundle, e.g. agent,server.. */

const PromiseTypeSyntax CF_COMMON_PROMISE_TYPES[] =
{

    PromiseTypeSyntaxNew("*", "classes", CF_CLASSBODY, &ClassesParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("*", "defaults", CF_DEFAULTSBODY, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("*", "meta", CF_METABODY, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("*", "reports", CF_REPORT_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("*", "vars", CF_VARBODY, &VarsParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("*", "*", CF_COMMON_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};

/*********************************************************/
/* THIS IS WHERE TO ATTACH SYNTAX MODULES                */
/*********************************************************/

/* Read in all parsable Bundle definitions */
/* REMEMBER TO REGISTER THESE IN cf3.extern.h */

const PromiseTypeSyntax *const CF_ALL_PROMISE_TYPES[] =
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
    CF_KNOWLEDGE_PROMISE_TYPES,      /* mod_knowledge.c */
    CF_USERS_PROMISE_TYPES,          /* mod_users.c */
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

    return COMMON_CONTROL_MAX;
}
