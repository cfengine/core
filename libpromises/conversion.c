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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <conversion.h>

#include <promises.h>
#include <files_names.h>
#include <dbm_api.h>
#include <mod_access.h>
#include <item_lib.h>
#include <logging.h>
#include <rlist.h>
#include <string_lib.h>


static int IsSpace(char *remainder);

/***************************************************************/

const char *MapAddress(const char *unspec_address)
{                               /* Is the address a mapped ipv4 over ipv6 address */

    if (strncmp(unspec_address, "::ffff:", 7) == 0)
    {
        return unspec_address + 7;
    }
    else
    {
        return unspec_address;
    }
}

int FindTypeInArray(const char *const haystack[], const char *needle, int default_value, int null_value)
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
    static const char *const MEASURE_POLICY_TYPES[] =
        { "average", "sum", "first", "last",  NULL };

    return FindTypeInArray(MEASURE_POLICY_TYPES, s, MEASURE_POLICY_AVERAGE, MEASURE_POLICY_NONE);
}

EnvironmentState EnvironmentStateFromString(const char *s)
{
    static const char *const ENV_STATE_TYPES[] =
        { "create", "delete", "running", "suspended", "down", NULL };

    return FindTypeInArray(ENV_STATE_TYPES, s, ENVIRONMENT_STATE_NONE, ENVIRONMENT_STATE_CREATE);
}

InsertMatchType InsertMatchTypeFromString(const char *s)
{
    static const char *const INSERT_MATCH_TYPES[] =
        { "ignore_leading", "ignore_trailing", "ignore_embedded",
          "exact_match", NULL };

    return FindTypeInArray(INSERT_MATCH_TYPES, s, INSERT_MATCH_TYPE_EXACT, INSERT_MATCH_TYPE_EXACT);
}

int SyslogPriorityFromString(const char *s)
{
    static const char *const SYSLOG_PRIORITY_TYPES[] =
    { "emergency", "alert", "critical", "error", "warning", "notice",
      "info", "debug", NULL };

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
    static const char *const DB_TYPES[] = { "mysql", "postgres", NULL };

    return FindTypeInArray(DB_TYPES, s, DATABASE_TYPE_NONE, DATABASE_TYPE_NONE);
}

UserState UserStateFromString(const char *s)
{
    static const char *const U_TYPES[] =
        { "present", "absent", "locked", NULL };

    return FindTypeInArray(U_TYPES, s, USER_STATE_NONE, USER_STATE_NONE);
}

PasswordFormat PasswordFormatFromString(const char *s)
{
    static const char *const U_TYPES[] = { "plaintext", "hash", NULL };

    return FindTypeInArray(U_TYPES, s, PASSWORD_FORMAT_NONE, PASSWORD_FORMAT_NONE);
}

PackageAction PackageActionFromString(const char *s)
{
    static const char *const PACKAGE_ACTION_TYPES[] =
    { "add", "delete", "reinstall", "update", "addupdate", "patch",
      "verify", NULL };

    return FindTypeInArray(PACKAGE_ACTION_TYPES, s, PACKAGE_ACTION_NONE, PACKAGE_ACTION_NONE);
}

NewPackageAction GetNewPackagePolicy(const char *s, const char **action_types)
{
    return FindTypeInArray(action_types, s, NEW_PACKAGE_ACTION_NONE, NEW_PACKAGE_ACTION_NONE);
}

PackageVersionComparator PackageVersionComparatorFromString(const char *s)
{
    static const char *const PACKAGE_SELECT_TYPES[] =
        { "==", "!=", ">", "<", ">=", "<=", NULL };

    return FindTypeInArray(PACKAGE_SELECT_TYPES, s, PACKAGE_VERSION_COMPARATOR_NONE, PACKAGE_VERSION_COMPARATOR_NONE);
}

PackageActionPolicy PackageActionPolicyFromString(const char *s)
{
    static const char *const ACTION_POLICY_TYPES[] =
        { "individual", "bulk", NULL };

    return FindTypeInArray(ACTION_POLICY_TYPES, s, PACKAGE_ACTION_POLICY_NONE, PACKAGE_ACTION_POLICY_NONE);
}

/***************************************************************************/

char *Rlist2String(Rlist *list, char *sep)
{
    Writer *writer = StringWriter();

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        RvalWrite(writer, rp->val);

        if (rp->next != NULL)
        {
            WriterWrite(writer, sep);
        }
    }

    return StringWriterClose(writer);
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
    static const char *const CONTEXT_SCOPES[] = { "namespace", "bundle" };
    return FindTypeInArray(CONTEXT_SCOPES, scope_str, CONTEXT_SCOPE_NAMESPACE, CONTEXT_SCOPE_NONE);
}

FileLinkType FileLinkTypeFromString(const char *s)
{
    static const char *const LINK_TYPES[] =
        { "symlink", "hardlink", "relative", "absolute", NULL };

    return FindTypeInArray(LINK_TYPES, s, FILE_LINK_TYPE_SYMLINK, FILE_LINK_TYPE_SYMLINK);
}

FileComparator FileComparatorFromString(const char *s)
{
    static const char *const FILE_COMPARISON_TYPES[] =
    { "atime", "mtime", "ctime", "digest", "hash", "binary", "exists", NULL };

    return FindTypeInArray(FILE_COMPARISON_TYPES, s, FILE_COMPARATOR_NONE, FILE_COMPARATOR_NONE);
}

static const char *const datatype_strings[] =
{
    [CF_DATA_TYPE_STRING] = "string",
    [CF_DATA_TYPE_INT] = "int",
    [CF_DATA_TYPE_REAL] = "real",
    [CF_DATA_TYPE_STRING_LIST] = "slist",
    [CF_DATA_TYPE_INT_LIST] = "ilist",
    [CF_DATA_TYPE_REAL_LIST] = "rlist",
    [CF_DATA_TYPE_OPTION] = "option",
    [CF_DATA_TYPE_OPTION_LIST] = "olist",
    [CF_DATA_TYPE_BODY] = "body",
    [CF_DATA_TYPE_BUNDLE] = "bundle",
    [CF_DATA_TYPE_CONTEXT] = "context",
    [CF_DATA_TYPE_CONTEXT_LIST] = "clist",
    [CF_DATA_TYPE_INT_RANGE] = "irange",
    [CF_DATA_TYPE_REAL_RANGE] = "rrange",
    [CF_DATA_TYPE_COUNTER] = "counter",
    [CF_DATA_TYPE_CONTAINER] = "data",
    [CF_DATA_TYPE_NONE] = "none"
};

DataType DataTypeFromString(const char *name)
{
    for (int i = 0; i < CF_DATA_TYPE_NONE; i++)
    {
        if (strcmp(datatype_strings[i], name) == 0)
        {
            return i;
        }
    }

    return CF_DATA_TYPE_NONE;
}

const char *DataTypeToString(DataType type)
{
    assert(type < CF_DATA_TYPE_NONE);
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

    return CF_DATA_TYPE_NONE;
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
    long long a = CF_NOINT;
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

    sscanf(s, "%lld%c%s", &a, &c, remainder);

// Test whether remainder is space only

    if ((a == CF_NOINT) || (!IsSpace(remainder)))
    {
        Log(LOG_LEVEL_INFO, "Error reading assumed integer value '%s' => 'non-value', found remainder '%s'",
              s, remainder);
        if (strchr(s, '$'))
        {
            Log(LOG_LEVEL_INFO, "The variable might not yet be expandable - not necessarily an error");
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
                Log(LOG_LEVEL_ERR, "Percentage out of range (%lld)", a);
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

    /* TODO Use strtol() instead of scanf(), it properly checks for overflow
     * but it is prone to coding errors, so even better bring OpenBSD's
     * strtonum() for proper conversions. */

    if (a < LONG_MIN)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Number '%s' underflows a long int, truncating to %ld",
            s, LONG_MIN);
        return LONG_MIN;
    }
    else if (a > LONG_MAX)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Number '%s' overflows a long int, truncating to %ld",
            s, LONG_MAX);
        return LONG_MAX;
    }

    return (long) a;
}

Interval IntervalFromString(const char *string)
{
    static const char *const INTERVAL_TYPES[] = { "hourly", "daily", NULL };

    return FindTypeInArray(INTERVAL_TYPES, string, INTERVAL_NONE, INTERVAL_NONE);
}

bool DoubleFromString(const char *s, double *value_out)
{
    double d;
    char quantifier, remainder;

    assert(s         != NULL);
    assert(value_out != NULL);

    int ret = sscanf(s, "%lf%c %c", &d, &quantifier, &remainder);

    if (ret < 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to parse real number: %s", s);
        return false;
    }
    else if (ret == 3)                               /* non-space remainder */
    {
        Log(LOG_LEVEL_ERR,
            "Anomalous remainder '%c' while parsing real number: %s",
            remainder, s);
        return false;
    }
    else if (ret == 1)                                /* no quantifier char */
    {
        /* nop */
    }
    else                                                  /* got quantifier */
    {
        assert(ret == 2);

        switch (quantifier)
        {
        case 'k':
            d *= 1000;
            break;
        case 'K':
            d *= 1024;
            break;
        case 'm':
            d *= 1000 * 1000;
            break;
        case 'M':
            d *= 1024 * 1024;
            break;
        case 'g':
            d *= 1000 * 1000 * 1000;
            break;
        case 'G':
            d *= 1024 * 1024 * 1024;
            break;
        case '%':
            if ((d < 0) || (d > 100))
            {
                Log(LOG_LEVEL_ERR, "Percentage out of range (%.2lf)", d);
                return false;
            }
            else
            {
                /* Represent percentages internally as negative numbers */
                /* TODO fix? */
                d *= -1;
            }
            break;

        case ' ':
            break;

        default:
            Log(LOG_LEVEL_VERBOSE,
                "Ignoring bad quantifier '%c' in real number: %s",
                quantifier, s);
            break;
        }
    }

    assert(ret == 1  ||  ret == 2);

    *value_out = d;
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
    static const char *const ACL_METHOD_TYPES[] =
        { "append", "overwrite", NULL };

    return FindTypeInArray(ACL_METHOD_TYPES, string, ACL_METHOD_NONE, ACL_METHOD_NONE);
}

AclType AclTypeFromString(const char *string)
{
    static const char *const ACL_TYPES[]=
        { "generic", "posix", "ntfs", NULL };

    return FindTypeInArray(ACL_TYPES, string, ACL_TYPE_NONE, ACL_TYPE_NONE);
}

/* For the deprecated attribute acl_directory_inherit. */
AclDefault AclInheritanceFromString(const char *string)
{
    static const char *const ACL_INHERIT_TYPES[5] =
        { "nochange", "specify", "parent", "clear", NULL };

    return FindTypeInArray(ACL_INHERIT_TYPES, string, ACL_DEFAULT_NONE, ACL_DEFAULT_NONE);
}

AclDefault AclDefaultFromString(const char *string)
{
    static const char *const ACL_DEFAULT_TYPES[5] =
        { "nochange", "specify", "access", "clear", NULL };

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

bool DataTypeIsIterable(DataType t)
{
    if (t == CF_DATA_TYPE_STRING_LIST ||
        t == CF_DATA_TYPE_INT_LIST    ||
        t == CF_DATA_TYPE_REAL_LIST   ||
        t == CF_DATA_TYPE_CONTAINER)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int CoarseLaterThan(const char *bigger, const char *smaller)
{
    char month_small[CF_SMALLBUF];
    char month_big[CF_SMALLBUF];
    int m_small, day_small, year_small, m_big, year_big, day_big;

    sscanf(smaller, "%d %s %d", &day_small, month_small, &year_small);
    sscanf(bigger, "%d %s %d", &day_big, month_big, &year_big);

    if (year_big < year_small)
    {
        return false;
    }

    m_small = Month2Int(month_small);
    m_big = Month2Int(month_big);

    if (m_big < m_small)
    {
        return false;
    }

    if (day_big < day_small && m_big == m_small && year_big == year_small)
    {
        return false;
    }

    return true;
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

/**
 * Copy first argument of #src to #dst. Argument is delimited either by double
 * quotes if first character is double quotes, or by space.
 *
 * @note Thread-safe version of CommandArg0().
 *
 * @return The length of #dst, or (size_t) -1 in case of overflow.
 */
size_t CommandArg0_bound(char *dst, const char *src, size_t dst_size)
{
    const char *start;
    char end_delimiter;

    if(src[0] == '\"')
    {
        start = &src[1];
        end_delimiter = '\"';
    }
    else
    {
        start = src;
        end_delimiter = ' ';
    }

    char *end = strchrnul(start, end_delimiter);
    size_t len = end - start;
    if (len < dst_size)
    {
        memcpy(dst, start, len);
        dst[len] = '\0';
        return len;
    }
    else
    {
        /* Check return value of CommandArg0_bound! If -1, the user should
         * never use dst, but just in case we are writing a bogus string. */
        const char trap[] = "BUG: COMMANDARG0_TOO_LONG";
        strlcpy(dst, trap, dst_size);
        return (size_t) -1;
    }
}

const char *CommandArg0(const char *execstr)
/**
 * WARNING: Not thread-safe.
 **/
{
    static char arg[CF_BUFSIZE]; /* GLOBAL_R, no initialization needed */

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

#ifndef __MINGW32__

/*******************************************************************/
/* Unix-only functions                                             */
/*******************************************************************/

/****************************************************************************/
/* Rlist to Uid/Gid lists                                                   */
/****************************************************************************/

void UidListDestroy(UidList *uids)
{

    while (uids)
    {
        UidList *ulp = uids;
        uids = uids->next;
        free(ulp->uidname);
        free(ulp);
    }
}

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
    else /* Hang new element off end of list: */
    {
        UidList *u = *uidlist;

        while (u->next != NULL)
        {
            u = u->next;
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
        uid = Str2Uid(RlistScalarValue(rp), username, pp);
        AddSimpleUidItem(&uidlist, uid, username);
    }

    if (uidlist == NULL)
    {
        AddSimpleUidItem(&uidlist, CF_SAME_OWNER, NULL);
    }

    return uidlist;
}

/*********************************************************************/

void GidListDestroy(GidList *gids)
{
    while (gids)
    {
        GidList *glp = gids;
        gids = gids->next;
        free(glp->gidname);
        free(glp);
    }
}

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
    else /* Hang new element off end of list: */
    {
        GidList *g = *gidlist;
        while (g->next != NULL)
        {
            g = g->next;
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
        gid = Str2Gid(RlistScalarValue(rp), groupname, pp);
        AddSimpleGidItem(&gidlist, gid, groupname);
    }

    if (gidlist == NULL)
    {
        AddSimpleGidItem(&gidlist, CF_SAME_GROUP, NULL);
    }

    return gidlist;
}

/*********************************************************************/

uid_t Str2Uid(const char *uidbuff, char *usercopy, const Promise *pp)
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

    if (StringIsNumeric(uidbuff))
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

gid_t Str2Gid(const char *gidbuff, char *groupcopy, const Promise *pp)
{
    struct group *gr;
    int gid = -2, tmp = -2;

    if (StringIsNumeric(gidbuff))
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

            if (groupcopy != NULL)
            {
                strcpy(groupcopy, gidbuff);
            }
        }
    }

    return gid;
}

#else /* !__MINGW32__ */

/* Release everything NovaWin_Rlist2SidList() allocates: */
void UidListDestroy(UidList *uids)
{
    while (uids)
    {
        UidList *ulp = uids;
        uids = uids->next;
        free(ulp);
    }
}

void GidListDestroy(ARG_UNUSED GidList *gids)
{
    assert(gids == NULL);
}

#endif
