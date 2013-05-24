/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "conversion.h"

#include "promises.h"
#include "files_names.h"
#include "dbm_api.h"
#include "mod_access.h"
#include "item_lib.h"
#include "logging.h"
#include "rlist.h"

#include <assert.h>

static int IsSpace(char *remainder);

/***************************************************************/

const char *MapAddress(const char *unspec_address)
{                               /* Is the address a mapped ipv4 over ipv6 address */

    if (strncmp(unspec_address, "::ffff:", 7) == 0)
    {
        return (char *) (unspec_address + 7);
    }
    else
    {
        return unspec_address;
    }
}

int FindTypeInArray(const char **haystack, const char *needle, int default_value, int null_value)
{
    if (needle == NULL)
    {
        return null_value;
    }

    for (int i = 0; haystack[i] != NULL; ++i)
    {
        if (strcmp(needle, haystack[i]) == 0)
        {
            return i;
        }
    }

    return default_value;
}

MeasurePolicy MeasurePolicyFromString(const char *s)
{
    static const char *MEASURE_POLICY_TYPES[] = { "average", "sum", "first", "last",  NULL };

    return FindTypeInArray(MEASURE_POLICY_TYPES, s, MEASURE_POLICY_AVERAGE, MEASURE_POLICY_NONE);
}

EnvironmentState EnvironmentStateFromString(const char *s)
{
    static const char *ENV_STATE_TYPES[] = { "create", "delete", "running", "suspended", "down", NULL };

    return FindTypeInArray(ENV_STATE_TYPES, s, ENVIRONMENT_STATE_NONE, ENVIRONMENT_STATE_CREATE);
}

InsertMatchType InsertMatchTypeFromString(const char *s)
{
    static const char *INSERT_MATCH_TYPES[] = { "ignore_leading", "ignore_trailing", "ignore_embedded",
                                                "exact_match", NULL };

    return FindTypeInArray(INSERT_MATCH_TYPES, s, INSERT_MATCH_TYPE_EXACT, INSERT_MATCH_TYPE_EXACT);
}

int SyslogPriorityFromString(const char *s)
{
    static const char *SYSLOG_PRIORITY_TYPES[] =
    { "emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", NULL };

    return FindTypeInArray(SYSLOG_PRIORITY_TYPES, s, 3, 3);
}

ShellType ShellTypeFromString(const char *string)
{
    // For historical reasons, supports all CF_BOOL values (true/false/yes/no...),
    // as well as "noshell,useshell,powershell".
    char *start, *end;
    char *options = "noshell,useshell,powershell," CF_BOOL;
    int i;
    int size;

    if (string == NULL)
    {
        return SHELL_TYPE_NONE;
    }

    start = options;
    size = strlen(string);
    for (i = 0;; i++)
    {
        end = strchr(start, ',');
        if (end == NULL)
        {
            break;
        }
        if (size == end - start && strncmp(string, start, end - start) == 0)
        {
            int cfBoolIndex;
            switch (i)
            {
            case 0:
                return SHELL_TYPE_NONE;
            case 1:
                return SHELL_TYPE_USE;
            case 2:
                return SHELL_TYPE_POWERSHELL;
            default:
                // Even cfBoolIndex is true, odd cfBoolIndex is false (from CF_BOOL).
                cfBoolIndex = i-3;
                return (cfBoolIndex & 1) ? SHELL_TYPE_NONE : SHELL_TYPE_USE;
            }
        }
        start = end + 1;
    }
    return SHELL_TYPE_NONE;
}

DatabaseType DatabaseTypeFromString(const char *s)
{
    static const char *DB_TYPES[] = { "mysql", "postgres", NULL };

    return FindTypeInArray(DB_TYPES, s, DATABASE_TYPE_NONE, DATABASE_TYPE_NONE);
}

PackageAction PackageActionFromString(const char *s)
{
    static const char *PACKAGE_ACTION_TYPES[] =
    { "add", "delete", "reinstall", "update", "addupdate", "patch", "verify", NULL };

    return FindTypeInArray(PACKAGE_ACTION_TYPES, s, PACKAGE_ACTION_NONE, PACKAGE_ACTION_NONE);
}

PackageVersionComparator PackageVersionComparatorFromString(const char *s)
{
    static const char *PACKAGE_SELECT_TYPES[] = { "==", "!=", ">", "<", ">=", "<=", NULL };

    return FindTypeInArray(PACKAGE_SELECT_TYPES, s, PACKAGE_VERSION_COMPARATOR_NONE, PACKAGE_VERSION_COMPARATOR_NONE);
}

PackageActionPolicy PackageActionPolicyFromString(const char *s)
{
    static const char *ACTION_POLICY_TYPES[] = { "individual", "bulk", NULL };

    return FindTypeInArray(ACTION_POLICY_TYPES, s, PACKAGE_ACTION_POLICY_NONE, PACKAGE_ACTION_POLICY_NONE);
}

/***************************************************************************/

char *Rlist2String(Rlist *list, char *sep)
{
    char line[CF_BUFSIZE];
    Rlist *rp;

    line[0] = '\0';

    for (rp = list; rp != NULL; rp = rp->next)
    {
        strcat(line, (char *) rp->item);

        if (rp->next)
        {
            strcat(line, sep);
        }
    }

    return xstrdup(line);
}

/***************************************************************************/

int SignalFromString(const char *s)
{
    int i = 0;
    Item *ip, *names = SplitString(CF_SIGNALRANGE, ',');

    for (ip = names; ip != NULL; ip = ip->next)
    {
        if (strcmp(s, ip->name) == 0)
        {
            break;
        }
        i++;
    }

    DeleteItemList(names);

    switch (i)
    {
    case cfa_hup:
        return SIGHUP;
    case cfa_int:
        return SIGINT;
    case cfa_trap:
        return SIGTRAP;
    case cfa_kill:
        return SIGKILL;
    case cfa_pipe:
        return SIGPIPE;
    case cfa_cont:
        return SIGCONT;
    case cfa_abrt:
        return SIGABRT;
    case cfa_stop:
        return SIGSTOP;
    case cfa_quit:
        return SIGQUIT;
    case cfa_term:
        return SIGTERM;
    case cfa_child:
        return SIGCHLD;
    case cfa_usr1:
        return SIGUSR1;
    case cfa_usr2:
        return SIGUSR2;
    case cfa_bus:
        return SIGBUS;
    case cfa_segv:
        return SIGSEGV;
    default:
        return -1;
    }

}

ContextScope ContextScopeFromString(const char *scope_str)
{
    static const char *CONTEXT_SCOPES[] = { "namespace", "bundle" };
    return FindTypeInArray(CONTEXT_SCOPES, scope_str, CONTEXT_SCOPE_NAMESPACE, CONTEXT_SCOPE_NONE);
}

FileLinkType FileLinkTypeFromString(const char *s)
{
    static const char *LINK_TYPES[] = { "symlink", "hardlink", "relative", "absolute", NULL };

    return FindTypeInArray(LINK_TYPES, s, FILE_LINK_TYPE_SYMLINK, FILE_LINK_TYPE_SYMLINK);
}

FileComparator FileComparatorFromString(const char *s)
{
    static const char *FILE_COMPARISON_TYPES[] =
    { "atime", "mtime", "ctime", "digest", "hash", "binary", "exists", NULL };

    return FindTypeInArray(FILE_COMPARISON_TYPES, s, FILE_COMPARATOR_NONE, FILE_COMPARATOR_NONE);
}

static const char *datatype_strings[] =
{
    [DATA_TYPE_STRING] = "string",
    [DATA_TYPE_INT] = "int",
    [DATA_TYPE_REAL] = "real",
    [DATA_TYPE_STRING_LIST] = "slist",
    [DATA_TYPE_INT_LIST] = "ilist",
    [DATA_TYPE_REAL_LIST] = "rlist",
    [DATA_TYPE_OPTION] = "option",
    [DATA_TYPE_OPTION_LIST] = "olist",
    [DATA_TYPE_BODY] = "body",
    [DATA_TYPE_BUNDLE] = "bundle",
    [DATA_TYPE_CONTEXT] = "context",
    [DATA_TYPE_CONTEXT_LIST] = "clist",
    [DATA_TYPE_INT_RANGE] = "irange",
    [DATA_TYPE_REAL_RANGE] = "rrange",
    [DATA_TYPE_COUNTER] = "counter",
    [DATA_TYPE_NONE] = "none"
};

DataType DataTypeFromString(const char *name)
{
    for (int i = 0; i < DATA_TYPE_NONE; i++)
    {
        if (strcmp(datatype_strings[i], name) == 0)
        {
            return i;
        }
    }

    return DATA_TYPE_NONE;
}

const char *DataTypeToString(DataType type)
{
    assert(type < DATA_TYPE_NONE);
    return datatype_strings[type];
}

DataType ConstraintSyntaxGetDataType(const ConstraintSyntax *body_syntax, const char *lval)
{
    int i = 0;

    for (i = 0; body_syntax[i].lval != NULL; i++)
    {
        if (lval && (strcmp(body_syntax[i].lval, lval) == 0))
        {
            return body_syntax[i].dtype;
        }
    }

    return DATA_TYPE_NONE;
}

/****************************************************************************/

bool BooleanFromString(const char *s)
{
    Item *list = SplitString(CF_BOOL, ','), *ip;
    int count = 0;

    for (ip = list; ip != NULL; ip = ip->next)
    {
        if (strcmp(s, ip->name) == 0)
        {
            break;
        }

        count++;
    }

    DeleteItemList(list);

    if (count % 2)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/****************************************************************************/

long IntFromString(const char *s)
{
    long a = CF_NOINT;
    char c = 'X';
    char remainder[CF_BUFSIZE];

    if (s == NULL)
    {
        return CF_NOINT;
    }

    if (strcmp(s, "inf") == 0)
    {
        return (long) CF_INFINITY;
    }

    if (strcmp(s, "now") == 0)
    {
        return (long) CFSTARTTIME;
    }

    remainder[0] = '\0';

    sscanf(s, "%ld%c%s", &a, &c, remainder);

// Test whether remainder is space only

    if ((a == CF_NOINT) || (!IsSpace(remainder)))
    {
        if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
        {
            Log(LOG_LEVEL_INFO, "Error reading assumed integer value '%s' => 'non-value', found remainder '%s'",
                  s, remainder);
            if (strchr(s, '$'))
            {
                Log(LOG_LEVEL_INFO, "The variable might not yet be expandable - not necessarily an error");
            }
        }
    }
    else
    {
        switch (c)
        {
        case 'k':
            a = 1000 * a;
            break;
        case 'K':
            a = 1024 * a;
            break;
        case 'm':
            a = 1000 * 1000 * a;
            break;
        case 'M':
            a = 1024 * 1024 * a;
            break;
        case 'g':
            a = 1000 * 1000 * 1000 * a;
            break;
        case 'G':
            a = 1024 * 1024 * 1024 * a;
            break;
        case '%':
            if ((a < 0) || (a > 100))
            {
                Log(LOG_LEVEL_ERR, "Percentage out of range (%ld)", a);
                return CF_NOINT;
            }
            else
            {
                /* Represent percentages internally as negative numbers */
                a = -a;
            }
            break;

        case ' ':
            break;

        default:
            break;
        }
    }

    return a;
}

/****************************************************************************/

static const long DAYS[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int GetMonthLength(int month, int year)
{
    /* FIXME: really? */
    if ((month == 1) && (year % 4 == 0))
    {
        return 29;
    }
    else
    {
        return DAYS[month];
    }
}

long TimeAbs2Int(const char *s)
{
    time_t cftime;
    int i;
    char mon[4], h[3], m[3];
    long month = 0, day = 0, hour = 0, min = 0, year = 0;

    if (s == NULL)
    {
        return CF_NOINT;
    }

    year = IntFromString(VYEAR);

    if (strstr(s, ":"))         /* Hr:Min */
    {
        sscanf(s, "%2[^:]:%2[^:]:", h, m);
        month = Month2Int(VMONTH);
        day = IntFromString(VDAY);
        hour = IntFromString(h);
        min = IntFromString(m);
    }
    else                        /* date Month */
    {
        sscanf(s, "%3[a-zA-Z] %ld", mon, &day);

        month = Month2Int(mon);

        if (Month2Int(VMONTH) < month)
        {
            /* Wrapped around */
            year--;
        }
    }

    cftime = 0;
    cftime += min * 60;
    cftime += hour * 3600;
    cftime += (day - 1) * 24 * 3600;
    cftime += 24 * 3600 * ((year - 1970) / 4);  /* Leap years */

    for (i = 0; i < month - 1; i++)
    {
        cftime += GetMonthLength(i, year) * 24 * 3600;
    }

    cftime += (year - 1970) * 365 * 24 * 3600;

    return (long) cftime;
}

/****************************************************************************/

long Months2Seconds(int m)
{
    long tot_days = 0;
    int this_month, i, month, year;

    if (m == 0)
    {
        return 0;
    }

    this_month = Month2Int(VMONTH);
    year = IntFromString(VYEAR);

    for (i = 0; i < m; i++)
    {
        month = (this_month - i) % 12;

        while (month < 0)
        {
            month += 12;
            year--;
        }

        tot_days += GetMonthLength(month, year);
    }

    return (long) tot_days *3600 * 24;
}

/*********************************************************************/

Interval IntervalFromString(const char *string)
{
    static const char *INTERVAL_TYPES[] = { "hourly", "daily", NULL };

    return FindTypeInArray(INTERVAL_TYPES, string, INTERVAL_NONE, INTERVAL_NONE);
}

/*********************************************************************/

int Day2Number(const char *datestring)
{
    int i = 0;

    for (i = 0; i < 7; i++)
    {
        if (strncmp(datestring, DAY_TEXT[i], 3) == 0)
        {
            return i;
        }
    }

    return -1;
}

/****************************************************************************/

bool DoubleFromString(const char *s, double *value_out)
{
    static const double NO_DOUBLE = -123.45;

    double a = NO_DOUBLE;
    char remainder[CF_BUFSIZE];
    char c = 'X';

    if (s == NULL)
    {
        return false;
    }

    remainder[0] = '\0';

    sscanf(s, "%lf%c%s", &a, &c, remainder);

    if ((a == NO_DOUBLE) || (!IsSpace(remainder)))
    {
        Log(LOG_LEVEL_ERR, "Reading assumed real value '%s', anomalous remainder '%s'", s, remainder);
        return false;
    }
    else
    {
        switch (c)
        {
        case 'k':
            a = 1000 * a;
            break;
        case 'K':
            a = 1024 * a;
            break;
        case 'm':
            a = 1000 * 1000 * a;
            break;
        case 'M':
            a = 1024 * 1024 * a;
            break;
        case 'g':
            a = 1000 * 1000 * 1000 * a;
            break;
        case 'G':
            a = 1024 * 1024 * 1024 * a;
            break;
        case '%':
            if ((a < 0) || (a > 100))
            {
                Log(LOG_LEVEL_ERR, "Percentage out of range (%.2lf)", a);
                return false;
            }
            else
            {
                /* Represent percentages internally as negative numbers */
                a = -a;
            }
            break;

        case ' ':
            break;

        default:
            break;
        }
    }

    *value_out = a;
    return true;
}

/****************************************************************************/

/**
 * @return true if successful
 */
bool IntegerRangeFromString(const char *intrange, long *min_out, long *max_out)
{
    Item *split;
    long lmax = CF_LOWINIT, lmin = CF_HIGHINIT;

/* Numeric types are registered by range separated by comma str "min,max" */

    if (intrange == NULL)
    {
        *min_out = CF_NOINT;
        *max_out = CF_NOINT;
        return true;
    }

    split = SplitString(intrange, ',');

    sscanf(split->name, "%ld", &lmin);

    if (strcmp(split->next->name, "inf") == 0)
    {
        lmax = (long) CF_INFINITY;
    }
    else
    {
        sscanf(split->next->name, "%ld", &lmax);
    }

    DeleteItemList(split);

    if ((lmin == CF_HIGHINIT) || (lmax == CF_LOWINIT))
    {
        return false;
    }

    *min_out = lmin;
    *max_out = lmax;
    return true;
}

AclMethod AclMethodFromString(const char *string)
{
    static const char *ACL_METHOD_TYPES[] = { "append", "overwrite", NULL };

    return FindTypeInArray(ACL_METHOD_TYPES, string, ACL_METHOD_NONE, ACL_METHOD_NONE);
}

AclType AclTypeFromString(const char *string)
{
    static const char *ACL_TYPES[]= { "generic", "posix", "ntfs", NULL };

    return FindTypeInArray(ACL_TYPES, string, ACL_TYPE_NONE, ACL_TYPE_NONE);
}

/* For the deprecated attribute acl_directory_inherit. */
AclDefault AclInheritanceFromString(const char *string)
{
    static const char *ACL_INHERIT_TYPES[5] = { "nochange", "specify", "parent", "clear", NULL };

    return FindTypeInArray(ACL_INHERIT_TYPES, string, ACL_DEFAULT_NONE, ACL_DEFAULT_NONE);
}

AclDefault AclDefaultFromString(const char *string)
{
    static const char *ACL_DEFAULT_TYPES[5] = { "nochange", "specify", "access", "clear", NULL };

    return FindTypeInArray(ACL_DEFAULT_TYPES, string, ACL_DEFAULT_NONE, ACL_DEFAULT_NONE);
}

AclInherit AclInheritFromString(const char *string)
{
    char *start, *end;
    char *options = CF_BOOL ",nochange";
    int i;
    int size;

    if (string == NULL)
    {
        return ACL_INHERIT_NOCHANGE;
    }

    start = options;
    size = strlen(string);
    for (i = 0;; i++)
    {
        end = strchr(start, ',');
        if (end == NULL)
        {
            break;
        }
        if (size == end - start && strncmp(string, start, end - start) == 0)
        {
            // Even i is true, odd i is false (from CF_BOOL).
            return (i & 1) ? ACL_INHERIT_FALSE : ACL_INHERIT_TRUE;
        }
        start = end + 1;
    }
    return ACL_INHERIT_NOCHANGE;
}

ServicePolicy ServicePolicyFromString(const char *string)
{
    static const char *SERVICE_POLICY_TYPES[] = { "start", "stop", "disable", "restart", "reload", NULL };

    return FindTypeInArray(SERVICE_POLICY_TYPES, string, SERVICE_POLICY_START, SERVICE_POLICY_START);
}

const char *DataTypeShortToType(char *short_type)
{
    assert(short_type);

    if(strcmp(short_type, "s") == 0)
    {
        return "string";
    }

    if(strcmp(short_type, "i") == 0)
    {
        return "int";
    }

    if(strcmp(short_type, "r") == 0)
    {
        return "real";
    }

    if(strcmp(short_type, "m") == 0)
    {
        return "menu";
    }

    if(strcmp(short_type, "sl") == 0)
    {
        return "string list";
    }

    if(strcmp(short_type, "il") == 0)
    {
        return "int list";
    }

    if(strcmp(short_type, "rl") == 0)
    {
        return "real list";
    }

    if(strcmp(short_type, "ml") == 0)
    {
        return "menu list";
    }

    return "unknown type";
}

int Month2Int(const char *string)
{
    int i;

    if (string == NULL)
    {
        return -1;
    }

    for (i = 0; i < 12; i++)
    {
        if (strncmp(MONTH_TEXT[i], string, strlen(string)) == 0)
        {
            return i + 1;
            break;
        }
    }

    return -1;
}

/*********************************************************************/

void TimeToDateStr(time_t t, char *outStr, int outStrSz)
/**
 * Formats a time as "30 Sep 2010".
 */
{
    char month[CF_SMALLBUF], day[CF_SMALLBUF], year[CF_SMALLBUF];
    char tmp[CF_SMALLBUF];

    snprintf(tmp, sizeof(tmp), "%s", ctime(&t));
    sscanf(tmp, "%*s %5s %3s %*s %5s", month, day, year);
    snprintf(outStr, outStrSz, "%s %s %s", day, month, year);
}

/*********************************************************************/

const char *CommandArg0(const char *execstr)
/** 
 * WARNING: Not thread-safe.
 **/
{
    static char arg[CF_BUFSIZE];

    const char *start;
    char end_delimiter;
    
    if(execstr[0] == '\"')
    {
        start = execstr + 1;
        end_delimiter = '\"';
    }
    else
    {
        start = execstr;
        end_delimiter = ' ';
    }

    strlcpy(arg, start, sizeof(arg));
    
    char *cut = strchr(arg, end_delimiter);
    
    if(cut)
    {
        *cut = '\0';
    }

    return arg;
}

/*************************************************************/

void CommandPrefix(char *execstr, char *comm)
{
    char *sp;

    for (sp = execstr; (*sp != ' ') && (*sp != '\0'); sp++)
    {
    }

    if (sp - 10 >= execstr)
    {
        sp -= 10;               /* copy 15 most relevant characters of command */
    }
    else
    {
        sp = execstr;
    }

    memset(comm, 0, 20);
    strncpy(comm, sp, 15);
}

static int IsSpace(char *remainder)
{
    char *sp;

    for (sp = remainder; *sp != '\0'; sp++)
    {
        if (!isspace((int)*sp))
        {
            return false;
        }
    }

    return true;
}

/*******************************************************************/

bool IsRealNumber(const char *s)
{
    static const double NO_DOUBLE = -123.45;
    double a = NO_DOUBLE;

    sscanf(s, "%lf", &a);

    if (a == NO_DOUBLE)
    {
        return false;
    }

    return true;
}

/*******************************************************************/
/* Unix-only functions                                             */
/*******************************************************************/

#ifndef __MINGW32__

/****************************************************************************/
/* Rlist to Uid/Gid lists                                                   */
/****************************************************************************/

static void AddSimpleUidItem(UidList ** uidlist, uid_t uid, char *uidname)
{
    UidList *ulp = xcalloc(1, sizeof(UidList));

    ulp->uid = uid;

    if (uid == CF_UNKNOWN_OWNER)        /* unknown user */
    {
        ulp->uidname = xstrdup(uidname);
    }

    if (*uidlist == NULL)
    {
        *uidlist = ulp;
    }
    else
    {
        UidList *u;

        for (u = *uidlist; u->next != NULL; u = u->next)
        {
        }
        u->next = ulp;
    }
}

UidList *Rlist2UidList(Rlist *uidnames, const Promise *pp)
{
    UidList *uidlist = NULL;
    Rlist *rp;
    char username[CF_MAXVARSIZE];
    uid_t uid;

    for (rp = uidnames; rp != NULL; rp = rp->next)
    {
        username[0] = '\0';
        uid = Str2Uid(rp->item, username, pp);
        AddSimpleUidItem(&uidlist, uid, username);
    }

    if (uidlist == NULL)
    {
        AddSimpleUidItem(&uidlist, CF_SAME_OWNER, NULL);
    }

    return (uidlist);
}

/*********************************************************************/

static void AddSimpleGidItem(GidList ** gidlist, gid_t gid, char *gidname)
{
    GidList *glp = xcalloc(1, sizeof(GidList));

    glp->gid = gid;

    if (gid == CF_UNKNOWN_GROUP)        /* unknown group */
    {
        glp->gidname = xstrdup(gidname);
    }

    if (*gidlist == NULL)
    {
        *gidlist = glp;
    }
    else
    {
        GidList *g;

        for (g = *gidlist; g->next != NULL; g = g->next)
        {
        }
        g->next = glp;
    }
}

GidList *Rlist2GidList(Rlist *gidnames, const Promise *pp)
{
    GidList *gidlist = NULL;
    Rlist *rp;
    char groupname[CF_MAXVARSIZE];
    gid_t gid;

    for (rp = gidnames; rp != NULL; rp = rp->next)
    {
        groupname[0] = '\0';
        gid = Str2Gid(rp->item, groupname, pp);
        AddSimpleGidItem(&gidlist, gid, groupname);
    }

    if (gidlist == NULL)
    {
        AddSimpleGidItem(&gidlist, CF_SAME_GROUP, NULL);
    }

    return (gidlist);
}

/*********************************************************************/

uid_t Str2Uid(char *uidbuff, char *usercopy, const Promise *pp)
{
    Item *ip, *tmplist;
    struct passwd *pw;
    int offset, uid = -2, tmp = -2;
    char *machine, *user, *domain;

    if (uidbuff[0] == '+')      /* NIS group - have to do this in a roundabout     */
    {                           /* way because calling getpwnam spoils getnetgrent */
        offset = 1;
        if (uidbuff[1] == '@')
        {
            offset++;
        }

        setnetgrent(uidbuff + offset);
        tmplist = NULL;

        while (getnetgrent(&machine, &user, &domain))
        {
            if (user != NULL)
            {
                AppendItem(&tmplist, user, NULL);
            }
        }

        endnetgrent();

        for (ip = tmplist; ip != NULL; ip = ip->next)
        {
            if ((pw = getpwnam(ip->name)) == NULL)
            {
                Log(LOG_LEVEL_INFO, "Unknown user in promise '%s'", ip->name);

                if (pp != NULL)
                {
                    PromiseRef(LOG_LEVEL_INFO, pp);
                }

                uid = CF_UNKNOWN_OWNER; /* signal user not found */
            }
            else
            {
                uid = pw->pw_uid;

                if (usercopy != NULL)
                {
                    strcpy(usercopy, ip->name);
                }
            }
        }

        DeleteItemList(tmplist);
        return uid;
    }

    if (isdigit((int) uidbuff[0]))
    {
        sscanf(uidbuff, "%d", &tmp);
        uid = (uid_t) tmp;
    }
    else
    {
        if (strcmp(uidbuff, "*") == 0)
        {
            uid = CF_SAME_OWNER;        /* signals wildcard */
        }
        else if ((pw = getpwnam(uidbuff)) == NULL)
        {
            Log(LOG_LEVEL_INFO, "Unknown user '%s' in promise", uidbuff);
            uid = CF_UNKNOWN_OWNER;     /* signal user not found */

            if (usercopy != NULL)
            {
                strcpy(usercopy, uidbuff);
            }
        }
        else
        {
            uid = pw->pw_uid;
        }
    }

    return uid;
}

/*********************************************************************/

gid_t Str2Gid(char *gidbuff, char *groupcopy, const Promise *pp)
{
    struct group *gr;
    int gid = -2, tmp = -2;

    if (isdigit((int) gidbuff[0]))
    {
        sscanf(gidbuff, "%d", &tmp);
        gid = (gid_t) tmp;
    }
    else
    {
        if (strcmp(gidbuff, "*") == 0)
        {
            gid = CF_SAME_GROUP;        /* signals wildcard */
        }
        else if ((gr = getgrnam(gidbuff)) == NULL)
        {
            Log(LOG_LEVEL_INFO, "Unknown group '%s' in promise", gidbuff);

            if (pp)
            {
                PromiseRef(LOG_LEVEL_INFO, pp);
            }

            gid = CF_UNKNOWN_GROUP;
        }
        else
        {
            gid = gr->gr_gid;
            strcpy(groupcopy, gidbuff);
        }
    }

    return gid;
}

#endif /* !__MINGW32__ */
