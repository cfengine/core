/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

/*
  This file can act as a template for adding functionality to cfengine 3.  All
  functionality can be added by extending the main array

  CF_MOD_PROMISE_TYPES[CF3_MODULES]

  and its array dimension, in mod_common, in the manner shown here.
*/

#include <mod_access.h>

#include <syntax.h>
#include <string_lib.h>
#include <policy.h>

/*
  Read this module file backwards, as dependencies have to be defined first -
  these arrays declare pairs of constraints

  lval => rval

  in the form (lval,type,range)

  If the type is cf_body then the range is a pointer to another array of pairs,
  like in a body "sub-routine"
*/

static const char *const POLICY_ERROR_WRONG_RESOURCE_FOR_DATA_SELECT =
    "Constraint report_data_select is allowed only for 'query' resource_type";

static bool AccessParseTreeCheck(const Promise *pp, Seq *errors);

static const ConstraintSyntax report_data_select_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("classes_include", CF_ANYSTRING, "List of regex filters for class names to be included into class report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("classes_exclude", CF_ANYSTRING, "List of regex filters for class names to be excluded from class report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("variables_include", CF_ANYSTRING, "List of regex filters for variable full qualified path to be included into variables report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("variables_exclude", CF_ANYSTRING, "List of regex filters for variable full qualified path to be excluded from variables report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("promise_notkept_log_include", CF_ANYSTRING, "List of regex filters for handle name to be included into promise not kept log report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("promise_notkept_log_exclude", CF_ANYSTRING, "List of regex filters for handle name to be excluded from promise not kept log report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("promise_repaired_log_include", CF_ANYSTRING, "List of regex filters for handle name to be included into promise repaired log report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("promise_repaired_log_exclude", CF_ANYSTRING, "List of regex filters for handle name to be excluded from promise repaired log report", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewStringList("monitoring_include", CF_ANYSTRING, "List of regex filters for slot name to be included from monitoring report", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("monitoring_exclude", CF_ANYSTRING, "List of regex filters for slot name to be excluded from monitoring report", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("metatags_include", CF_ANYSTRING, "List of regex filters for metatags to be included into reports", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("metatags_exclude", CF_ANYSTRING, "List of regex filters for metatags to be excluded from reports", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("promise_handle_include", CF_ANYSTRING, "List of regex filters for promise handle to be included into reports", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("promise_handle_exclude", CF_ANYSTRING, "List of regex filters for promise handle to be excluded from reports", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax report_data_select_body = BodySyntaxNew("report_data_select", report_data_select_constraints, NULL, SYNTAX_STATUS_NORMAL);

const ConstraintSyntax CF_REMACCESS_BODIES[REMOTE_ACCESS_NONE + 1] =
{
    ConstraintSyntaxNewStringList("admit", "", "List of host names or IP addresses to grant access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("deny", "", "List of host names or IP addresses to deny access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("admit_ips", "", "List of IP addresses or subnet masks to grant access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("deny_ips", "", "List of IP addresses or subnet masks to deny access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("admit_hostnames", "", "List of hostnames to grant access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("deny_hostnames", "", "List of hostnames to deny access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("admit_keys", "", "List of host keys that will be granted access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("deny_keys", "", "List of host keys that will be denied access to file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("maproot", "", "List of host names or IP addresses to grant full read-privilege on the server", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ifencrypted", "true/false whether the current file access promise is conditional on the connection from the client being encrypted. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("resource_type", "path,literal,context,query,variable", "The type of object being granted access (the default grants access to files)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("report_data_select", &report_data_select_body, "Report content filter", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("shortcut", "", "For path resource_type, the server will expand a relative path beginning with this text", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CF_REMROLE_BODIES[REMOTE_ROLE_NONE + 1] =
{
    ConstraintSyntaxNewStringList("authorize", "", "List of public-key user names that are allowed to activate the promised class during remote agent activation", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_REMACCESS_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("server", "access", CF_REMACCESS_BODIES, &AccessParseTreeCheck, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("server", "roles", CF_REMROLE_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};

static bool AccessParseTreeCheck(const Promise *pp, Seq *errors)
{
    bool success = true;

    bool isResourceType = false;
    bool isReportDataSelect = false;
    Constraint *data_select_const = NULL;

    for (size_t i = 0; i <SeqLength(pp->conlist); i++)
    {
        Constraint *con = SeqAt(pp->conlist, i);

        if (StringSafeCompare("resource_type", con->lval) == 0)
        {
            if (con->rval.type == RVAL_TYPE_SCALAR)
            {
                if (StringSafeCompare("query", (char*)con->rval.item) == 0)
                {
                    isResourceType = true;
                }
            }
        }
        else if (StringSafeCompare("report_data_select", con->lval) == 0)
        {
            data_select_const = con;
            isReportDataSelect = true;
        }

    }

    if (isReportDataSelect && !isResourceType)
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_CONSTRAINT, data_select_const,
                                         POLICY_ERROR_WRONG_RESOURCE_FOR_DATA_SELECT));
        success = false;
    }

    return success;
}

