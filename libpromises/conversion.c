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

#include "conversion.h"

#include "promises.h"
#include "files_names.h"
#include "dbm_api.h"
#include "mod_access.h"
#include "item_lib.h"
#include "reporting.h"
#include "cfstream.h"
#include "logging.h"
#include "rlist.h"

#include <assert.h>

static int IsSpace(char *remainder);

/***************************************************************/

char *MapAddress(char *unspec_address)
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

/***************************************************************************/

char *EscapeQuotes(const char *s, char *out, int outSz)
{
    char *spt;
    const char *spf;
    int i = 0;

    memset(out, 0, outSz);

    for (spf = s, spt = out; (i < outSz - 2) && (*spf != '\0'); spf++, spt++, i++)
    {
        switch (*spf)
        {
        case '\'':
        case '\"':
            *spt++ = '\\';
            *spt = *spf;
            i += 3;
            break;

        default:
            *spt = *spf;
            i++;
            break;
        }
    }

    return out;
}

/***************************************************************************/

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

OutputLevel OutputLevelFromString(const char *level)
{
    static const char *REPORT_LEVEL_TYPES[] = { "inform", "verbose", "error", "log", NULL };

    return FindTypeInArray(REPORT_LEVEL_TYPES, level, OUTPUT_LEVEL_NONE, OUTPUT_LEVEL_NONE);
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

DataType DataTypeFromString(const char *name)
/* convert abstract data type names: int, ilist etc */
{
    int i;

    CfDebug("typename2type(%s)\n", name);

    for (i = 0; i < (int) DATA_TYPE_NONE; i++)
    {
        if (name && (strcmp(CF_DATATYPES[i], name) == 0))
        {
            break;
        }
    }

    return (DataType) i;
}


DataType BodySyntaxGetDataType(const BodySyntax *body_syntax, const char *lval)
{
    int i = 0;

    for (i = 0; body_syntax[i].range != NULL; i++)
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
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! Error reading assumed integer value \"%s\" => \"%s\" (found remainder \"%s\")\n",
                  s, "non-value", remainder);
            if (strchr(s, '$'))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !! The variable might not yet be expandable - not necessarily an error");
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "Percentage out of range (%ld)", a);
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

    CfDebug("(%s)\n%ld=%s,%ld=%s,%ld,%ld,%ld\n", s, year, VYEAR, month, VMONTH, day, hour, min);

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

    CfDebug("Time %s CORRESPONDS %s\n", s, cf_ctime(&cftime));
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

double DoubleFromString(const char *s)
{
    double a = CF_NODOUBLE;
    char remainder[CF_BUFSIZE];
    char output[CF_BUFSIZE];
    char c = 'X';

    if (s == NULL)
    {
        return CF_NODOUBLE;
    }

    remainder[0] = '\0';

    sscanf(s, "%lf%c%s", &a, &c, remainder);

    if ((a == CF_NODOUBLE) || (!IsSpace(remainder)))
    {
        snprintf(output, CF_BUFSIZE, "Error reading assumed real value %s (anomalous remainder %s)\n", s, remainder);
        ReportError(output);
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "Percentage out of range (%.2lf)", a);
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

void IntRange2Int(char *intrange, long *min, long *max, const Promise *pp)
{
    Item *split;
    long lmax = CF_LOWINIT, lmin = CF_HIGHINIT;

/* Numeric types are registered by range separated by comma str "min,max" */

    if (intrange == NULL)
    {
        *min = CF_NOINT;
        *max = CF_NOINT;
        return;
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
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError("Could not make sense of integer range [%s]", intrange);
    }

    *min = lmin;
    *max = lmax;
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

AclInheritance AclInheritanceFromString(const char *string)
{
    static const char *ACL_INHERIT_TYPES[5] = { "nochange", "specify", "parent", "clear", NULL };

    return FindTypeInArray(ACL_INHERIT_TYPES, string, ACL_INHERITANCE_NONE, ACL_INHERITANCE_NONE);
}

ServicePolicy ServicePolicyFromString(const char *string)
{
    static const char *SERVICE_POLICY_TYPES[5] = { "start", "stop", "disable", "restart", NULL };

    return FindTypeInArray(SERVICE_POLICY_TYPES, string, SERVICE_POLICY_START, SERVICE_POLICY_START);
}

const char *DataTypeToString(DataType dtype)
{
    switch (dtype)
    {
    case DATA_TYPE_STRING:
        return "s";
    case DATA_TYPE_STRING_LIST:
        return "sl";
    case DATA_TYPE_INT:
        return "i";
    case DATA_TYPE_INT_LIST:
        return "il";
    case DATA_TYPE_REAL:
        return "r";
    case DATA_TYPE_REAL_LIST:
        return "rl";
    case DATA_TYPE_OPTION:
        return "m";
    case DATA_TYPE_OPTION_LIST:
        return "ml";
    default:
        return "D?";
    }
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

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int Month2Int(const char *string)
{
    return MonthLen2Int(string, MAX_MONTH_NAME);
}

/*************************************************************/

int MonthLen2Int(const char *string, int len)
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

    snprintf(tmp, sizeof(tmp), "%s", cf_ctime(&t));
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

/*************************************************************/

int NonEmptyLine(char *line)
{
    char *sp;

    for (sp = line; *sp != '\0'; sp++)
    {
        if (!isspace((int) *sp))
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

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

int IsRealNumber(const char *s)
{
    double a = CF_NODOUBLE;

    sscanf(s, "%lf", &a);

    if (a == CF_NODOUBLE)
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
                CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unknown user in promise \'%s\'\n", ip->name);

                if (pp != NULL)
                {
                    PromiseRef(OUTPUT_LEVEL_INFORM, pp);
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
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unknown user %s in promise\n", uidbuff);
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
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unknown group \'%s\' in promise\n", gidbuff);

            if (pp)
            {
                PromiseRef(OUTPUT_LEVEL_INFORM, pp);
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
