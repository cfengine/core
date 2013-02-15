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

char *EscapeJson(char *s, char *out, int outSz)
{
    char *spt, *spf;
    int i = 0;

    memset(out, 0, outSz);

    for (spf = s, spt = out; (i < outSz - 2) && (*spf != '\0'); spf++, spt++, i++)
    {
        switch (*spf)
        {
        case '\"':
        case '\\':
        case '/':
            *spt++ = '\\';
            *spt = *spf;
            i += 2;
            break;
        case '\n':
            *spt++ = '\\';
            *spt = 'n';
            i += 2;
            break;
        case '\t':
            *spt++ = '\\';
            *spt = 't';
            i += 2;
            break;
        case '\r':
            *spt++ = '\\';
            *spt = 'r';
            i += 2;
            break;
        case '\b':
            *spt++ = '\\';
            *spt = 'b';
            i += 2;
            break;
        case '\f':
            *spt++ = '\\';
            *spt = 'f';
            i += 2;
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

static int FindTypeInArray(const char **haystack, const char *needle, int default_value, int null_value)
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


static const char *MEASURE_POLICY_TYPES[] = { "average", "sum", "first", "last",  NULL };

enum cfmeasurepolicy MeasurePolicy2Value(char *s)
{
    return FindTypeInArray(MEASURE_POLICY_TYPES, s, cfm_average, cfm_nomeasure);
}

static const char *ENV_STATE_TYPES[] = { "create", "delete", "running", "suspended", "down", NULL };

enum cfenvironment_state Str2EnvState(char *s)
{
    return FindTypeInArray(ENV_STATE_TYPES, s, cfvs_none, cfvs_create);
}

static const char *INSERT_MATCH_TYPES[] = { "ignore_leading", "ignore_trailing", "ignore_embedded",
                                            "exact_match", NULL };

enum insert_match String2InsertMatch(char *s)
{
    return FindTypeInArray(INSERT_MATCH_TYPES, s, cf_exact_match, cf_exact_match);
}

static const char *SYSLOG_PRIORITY_TYPES[] =
{ "emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", NULL };

int SyslogPriority2Int(char *s)
{
    return FindTypeInArray(SYSLOG_PRIORITY_TYPES, s, 3, 3);
}

static const char *DB_TYPES[] = { "mysql", "postgres", NULL };

enum cfdbtype Str2dbType(char *s)
{
    return FindTypeInArray(DB_TYPES, s, cfd_notype, cfd_notype);
}

static const char *PACKAGE_ACTION_TYPES[] =
{ "add", "delete", "reinstall", "update", "addupdate", "patch", "verify", NULL };

enum package_actions Str2PackageAction(char *s)
{
    return FindTypeInArray(PACKAGE_ACTION_TYPES, s, cfa_pa_none, cfa_pa_none);
}

static const char *PACKAGE_SELECT_TYPES[] = { "==", "!=", ">", "<", ">=", "<=", NULL };

enum version_cmp Str2PackageSelect(char *s)
{
    return FindTypeInArray(PACKAGE_SELECT_TYPES, s, cfa_cmp_none, cfa_cmp_none);
}

static const char *ACTION_POLICY_TYPES[] = { "individual", "bulk", NULL };

enum action_policy Str2ActionPolicy(char *s)
{
    return FindTypeInArray(ACTION_POLICY_TYPES, s, cfa_no_ppolicy, cfa_no_ppolicy);
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

int Signal2Int(char *s)
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

static const char *REPORT_LEVEL_TYPES[] = { "inform", "verbose", "error", "log", NULL };

enum cfreport String2ReportLevel(char *s)
{
    return FindTypeInArray(REPORT_LEVEL_TYPES, s, cf_noreport, cf_noreport);
}

static const char *LINK_TYPES[] = { "symlink", "hardlink", "relative", "absolute", NULL };

enum cflinktype String2LinkType(char *s)
{
    return FindTypeInArray(LINK_TYPES, s, cfa_symlink, cfa_symlink);
}

static const char *FILE_COMPARISON_TYPES[] =
{ "atime", "mtime", "ctime", "digest", "hash", "binary", "exists", NULL };

enum cfcomparison String2Comparison(char *s)
{
    return FindTypeInArray(FILE_COMPARISON_TYPES, s, cfa_nocomparison, cfa_nocomparison);
}

static const char *REPRESENTATION_TYPES[] = 
{ "url", "web", "file", "db", "literal", "image", "portal", NULL };

enum representations String2Representation(char *s)
{
    return FindTypeInArray(REPRESENTATION_TYPES, s, cfk_none, cfk_none);
}

/****************************************************************************/

enum cfsbundle Type2Cfs(char *name)
{
    int i;

    for (i = 0; i < (int) cfs_nobtype; i++)
    {
        if (name && (strcmp(CF_REMACCESS_SUBTYPES[i].subtype, name) == 0))
        {
            break;
        }
    }

    return (enum cfsbundle) i;
}

/****************************************************************************/

enum cfdatatype Typename2Datatype(char *name)
/* convert abstract data type names: int, ilist etc */
{
    int i;

    CfDebug("typename2type(%s)\n", name);

    for (i = 0; i < (int) cf_notype; i++)
    {
        if (name && (strcmp(CF_DATATYPES[i], name) == 0))
        {
            break;
        }
    }

    return (enum cfdatatype) i;
}

/****************************************************************************/

const char *AgentTypeToString(AgentType agent_type)
{
    return CF_AGENTTYPES[agent_type];
}

/****************************************************************************/

enum cfdatatype GetControlDatatype(const char *varname, const BodySyntax *bp)
{
    int i = 0;

    for (i = 0; bp[i].range != NULL; i++)
    {
        if (varname && (strcmp(bp[i].lval, varname) == 0))
        {
            return bp[i].dtype;
        }
    }

    return cf_notype;
}

/****************************************************************************/

int GetBoolean(const char *s)
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

long Str2Int(const char *s)
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
            CfOut(cf_inform, "", " !! Error reading assumed integer value \"%s\" => \"%s\" (found remainder \"%s\")\n",
                  s, "non-value", remainder);
            if (strchr(s, '$'))
            {
                CfOut(cf_inform, "", " !! The variable might not yet be expandable - not necessarily an error");
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
                CfOut(cf_error, "", "Percentage out of range (%ld)", a);
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

long TimeAbs2Int(char *s)
{
    time_t cftime;
    int i;
    char mon[4], h[3], m[3];
    long month = 0, day = 0, hour = 0, min = 0, year = 0;

    if (s == NULL)
    {
        return CF_NOINT;
    }

    year = Str2Int(VYEAR);

    if (strstr(s, ":"))         /* Hr:Min */
    {
        sscanf(s, "%2[^:]:%2[^:]:", h, m);
        month = Month2Int(VMONTH);
        day = Str2Int(VDAY);
        hour = Str2Int(h);
        min = Str2Int(m);
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
    year = Str2Int(VYEAR);

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

static const char *INTERVAL_TYPES[] = { "hourly", "daily", NULL };

enum cfinterval Str2Interval(char *string)
{
    return FindTypeInArray(INTERVAL_TYPES, string, cfa_nointerval, cfa_nointerval);
}

/*********************************************************************/

int Day2Number(char *datestring)
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

void UtcShiftInterval(time_t t, char *out, int outSz)
/* 00 - 06, 
   06 - 12, 
   12 - 18, 
   18 - 24*/
{
    char buf[CF_MAXVARSIZE];
    int hr = 0, fromHr = 0, toHr = 0;

    cf_strtimestamp_utc(t, buf);

    sscanf(buf + 11, "%d", &hr);
    buf[11] = '\0';

    if (hr < 6)
    {
        fromHr = 0;
        toHr = 6;
    }
    else if (hr < 12)
    {
        fromHr = 6;
        toHr = 12;
    }
    else if (hr < 18)
    {
        fromHr = 12;
        toHr = 18;
    }
    else
    {
        fromHr = 18;
        toHr = 24;
    }

    snprintf(out, outSz, "%s %02d-%02d", buf, fromHr, toHr);
}

/****************************************************************************/

double Str2Double(const char *s)
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
                CfOut(cf_error, "", "Percentage out of range (%.2lf)", a);
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
        PromiseRef(cf_error, pp);
        FatalError("Could not make sense of integer range [%s]", intrange);
    }

    *min = lmin;
    *max = lmax;
}

static const char *ACL_METHOD_TYPES[] = { "append", "overwrite", NULL };

enum cf_acl_method Str2AclMethod(char *string)
{
    return FindTypeInArray(ACL_METHOD_TYPES, string, cfacl_nomethod, cfacl_nomethod);
}

static const char *ACL_TYPES[]= { "generic", "posix", "ntfs", NULL };

enum cf_acl_type Str2AclType(char *string)
{
    return FindTypeInArray(ACL_TYPES, string, cfacl_notype, cfacl_notype);
}

static const char *ACL_INHERIT_TYPES[5] = { "nochange", "specify", "parent", "clear", NULL };

enum cf_acl_inherit Str2AclInherit(char *string)
{
    return FindTypeInArray(ACL_INHERIT_TYPES, string, cfacl_noinherit, cfacl_noinherit);
}

static const char *SERVICE_POLICY_TYPES[5] = { "start", "stop", "disable", "restart", NULL };

enum cf_srv_policy Str2ServicePolicy(char *string)
{
    return FindTypeInArray(SERVICE_POLICY_TYPES, string, cfsrv_start, cfsrv_start);
}

char *Dtype2Str(enum cfdatatype dtype)
{
    switch (dtype)
    {
    case cf_str:
        return "s";
    case cf_slist:
        return "sl";
    case cf_int:
        return "i";
    case cf_ilist:
        return "il";
    case cf_real:
        return "r";
    case cf_rlist:
        return "rl";
    case cf_opts:
        return "m";
    case cf_olist:
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

int Month2Int(char *string)
{
    return MonthLen2Int(string, MAX_MONTH_NAME);
}

/*************************************************************/

int MonthLen2Int(char *string, int len)
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

const char *GetArg0(const char *execstr)
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

void CommPrefix(char *execstr, char *comm)
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

/*************************************************************/

char *Item2String(Item *ip)
{
    Item *currItem;
    int stringSz = 0;
    char *buf;

    // compute required buffer size
    for (currItem = ip; currItem != NULL; currItem = currItem->next)
    {
        stringSz += strlen(currItem->name);
        stringSz++;             // newline space
    }

    // we automatically get \0-termination because we are not appending a \n after the last item

    buf = xcalloc(1, stringSz);

    // do the copy
    for (currItem = ip; currItem != NULL; currItem = currItem->next)
    {
        strcat(buf, currItem->name);

        if (currItem->next != NULL)     // no newline after last item
        {
            strcat(buf, "\n");
        }
    }

    return buf;
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

int IsRealNumber(char *s)
{
    double a = CF_NODOUBLE;

    sscanf(s, "%lf", &a);

    if (a == CF_NODOUBLE)
    {
        return false;
    }

    return true;
}

static const char *MENU_TYPES[] = { "delta", "full", "relay", "collect_call", NULL };

enum cfd_menu String2Menu(const char *s)
{
    return FindTypeInArray(MENU_TYPES, s, cfd_menu_error, cfd_menu_error);
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
                CfOut(cf_inform, "", " !! Unknown user in promise \'%s\'\n", ip->name);

                if (pp != NULL)
                {
                    PromiseRef(cf_inform, pp);
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
            CfOut(cf_inform, "", " !! Unknown user %s in promise\n", uidbuff);
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
            CfOut(cf_inform, "", " !! Unknown group \'%s\' in promise\n", gidbuff);

            if (pp)
            {
                PromiseRef(cf_inform, pp);
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
