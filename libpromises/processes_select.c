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

#include "processes_select.h"

#include "env_context.h"
#include "files_names.h"
#include "conversion.h"
#include "reporting.h"
#include "matching.h"
#include "cfstream.h"
#include "verify_processes.h"
#include "string_lib.h"
#include "item_lib.h"
#include "pipes.h"
#include "files_interfaces.h"
#include "logging.h"
#include "rlist.h"
#include "policy.h"

#ifdef HAVE_ZONE_H
# include <zone.h>
#endif

static int SelectProcRangeMatch(char *name1, char *name2, int min, int max, char **names, char **line);
static int SelectProcRegexMatch(char *name1, char *name2, char *regex, char **colNames, char **line);
static int SplitProcLine(char *proc, char **names, int *start, int *end, char **line);
static int SelectProcTimeCounterRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int SelectProcTimeAbsRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int GetProcColumnIndex(char *name1, char *name2, char **names);
static void GetProcessColumnNames(char *proc, char **names, int *start, int *end);
static int ExtractPid(char *psentry, char **names, int *start, int *end);

/***************************************************************************/

static int SelectProcess(char *procentry, char **names, int *start, int *end, ProcessSelect a)
{
    AlphaList proc_attr;
    int result = true, i;
    char *column[CF_PROCCOLS];
    Rlist *rp;

    CfDebug("SelectProcess(%s)\n", procentry);

    InitAlphaList(&proc_attr);

    if (!SplitProcLine(procentry, names, start, end, column))
    {
        return false;
    }

    if (DEBUG)
    {
        for (i = 0; names[i] != NULL; i++)
        {
            printf("COL[%s] = \"%s\"\n", names[i], column[i]);
        }
    }

    for (rp = a.owner; rp != NULL; rp = rp->next)
    {
        if (SelectProcRegexMatch("USER", "UID", (char *) rp->item, names, column))
        {
            PrependAlphaList(&proc_attr, "process_owner");
            break;
        }
    }

    if (SelectProcRangeMatch("PID", "PID", a.min_pid, a.max_pid, names, column))
    {
        PrependAlphaList(&proc_attr, "pid");
    }

    if (SelectProcRangeMatch("PPID", "PPID", a.min_ppid, a.max_ppid, names, column))
    {
        PrependAlphaList(&proc_attr, "ppid");
    }

    if (SelectProcRangeMatch("PGID", "PGID", a.min_pgid, a.max_pgid, names, column))
    {
        PrependAlphaList(&proc_attr, "pgid");
    }

    if (SelectProcRangeMatch("VSZ", "SZ", a.min_vsize, a.max_vsize, names, column))
    {
        PrependAlphaList(&proc_attr, "vsize");
    }

    if (SelectProcRangeMatch("RSS", "RSS", a.min_rsize, a.max_rsize, names, column))
    {
        PrependAlphaList(&proc_attr, "rsize");
    }

    if (SelectProcTimeCounterRangeMatch
        ("TIME", "TIME", a.min_ttime, a.max_ttime, names, column))
    {
        PrependAlphaList(&proc_attr, "ttime");
    }

    if (SelectProcTimeAbsRangeMatch
        ("STIME", "START", a.min_stime, a.max_stime, names, column))
    {
        PrependAlphaList(&proc_attr, "stime");
    }

    if (SelectProcRangeMatch("NI", "PRI", a.min_pri, a.max_pri, names, column))
    {
        PrependAlphaList(&proc_attr, "priority");
    }

    if (SelectProcRangeMatch("NLWP", "NLWP", a.min_thread, a.max_thread, names, column))
    {
        PrependAlphaList(&proc_attr, "threads");
    }

    if (SelectProcRegexMatch("S", "STAT", a.status, names, column))
    {
        PrependAlphaList(&proc_attr, "status");
    }

    if (SelectProcRegexMatch("CMD", "COMMAND", a.command, names, column))
    {
        PrependAlphaList(&proc_attr, "command");
    }

    if (SelectProcRegexMatch("TTY", "TTY", a.tty, names, column))
    {
        PrependAlphaList(&proc_attr, "tty");
    }

    result = EvalProcessResult(a.process_result, &proc_attr);

    DeleteAlphaList(&proc_attr);

    for (i = 0; column[i] != NULL; i++)
    {
        free(column[i]);
    }

    return result;
}

Item *SelectProcesses(const Item *processes, const char *process_name, ProcessSelect a, bool attrselect)
{
    Item *result = NULL;

    if (processes == NULL)
    {
        return result;
    }

    char *names[CF_PROCCOLS];
    int start[CF_PROCCOLS];
    int end[CF_PROCCOLS];

    GetProcessColumnNames(processes->name, &names[0], start, end);

    for (Item *ip = processes->next; ip != NULL; ip = ip->next)
    {
        int s, e;

        if (BlockTextMatch(process_name, ip->name, &s, &e))
        {
            if (NULL_OR_EMPTY(ip->name))
            {
                continue;
            }

            if (attrselect && !SelectProcess(ip->name, names, start, end, a))
            {
                continue;
            }

            pid_t pid = ExtractPid(ip->name, names, start, end);

            if (pid == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Unable to extract pid while looking for %s\n", process_name);
                continue;
            }

            PrependItem(&result, ip->name, "");
            result->counter = (int)pid;
        }
    }

    for (int i = 0; i < CF_PROCCOLS; i++)
    {
        free(names[i]);
    }

    return result;
}

int FindPidMatches(Item *procdata, Item **killlist, Attributes a, Promise *pp)
{
    int matches = 0;
    pid_t cfengine_pid = getpid();

    Item *matched = SelectProcesses(procdata, pp->promiser, a.process_select, a.haveselect);

    for (Item *ip = matched; ip != NULL; ip = ip->next)
    {
        CF_OCCUR++;

        if (a.transaction.action == cfa_warn)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Matched: %s\n", ip->name);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! Matched: %s\n", ip->name);
        }

        pid_t pid = ip->counter;

        if (pid == 1)
        {
            if ((RlistLen(a.signals) == 1) && (RlistIsStringIn(a.signals, "hup")))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "(Okay to send only HUP to init)\n");
            }
            else
            {
                continue;
            }
        }

        if ((pid < 4) && (a.signals))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Will not signal or restart processes 0,1,2,3 (occurred while looking for %s)\n",
                  pp->promiser);
            continue;
        }

        bool promised_zero = (a.process_count.min_range == 0) && (a.process_count.max_range == 0);

        if ((a.transaction.action == cfa_warn) && promised_zero)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Process alert: %s\n", procdata->name);     /* legend */
            CfOut(OUTPUT_LEVEL_ERROR, "", "Process alert: %s\n", ip->name);
            continue;
        }

        if ((pid == cfengine_pid) && (a.signals))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! cf-agent will not signal itself!\n");
            continue;
        }

        PrependItem(killlist, ip->name, "");
        (*killlist)->counter = pid;
        matches++;
    }

    DeleteItemList(matched);

    return matches;
}


/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int SelectProcRangeMatch(char *name1, char *name2, int min, int max, char **names, char **line)
{
    int i;
    long value;

    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, names)) != -1)
    {
        value = IntFromString(line[i]);

        if (value == CF_NOINT)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Failed to extract a valid integer from %s => \"%s\" in process list\n", names[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    return false;
}

/***************************************************************************/

static long TimeCounter2Int(const char *s)
{
    long d = 0, h = 0, m = 0;
    char output[CF_BUFSIZE];

    if (s == NULL)
    {
        return CF_NOINT;
    }

    if (strchr(s, '-'))
    {
        if (sscanf(s, "%ld-%ld:%ld", &d, &h, &m) != 3)
        {
            snprintf(output, CF_BUFSIZE, "Unable to parse TIME 'ps' field, expected dd-hh:mm, got '%s'", s);
            FatalError("%s", output);
        }
    }
    else
    {
        if (sscanf(s, "%ld:%ld", &h, &m) != 2)
        {
            snprintf(output, CF_BUFSIZE, "Unable to parse TIME 'ps' field, expected hH:mm, got '%s'", s);
            FatalError("%s", output);
        }
    }

    return 60 * (m + 60 * (h + 24 * d));
}

static int SelectProcTimeCounterRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line)
{
    int i;
    time_t value;

    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, names)) != -1)
    {
        value = (time_t) TimeCounter2Int(line[i]);

        if (value == CF_NOINT)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Failed to extract a valid integer from %c => \"%s\" in process list\n", name1[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Selection filter matched counter range %s/%s = %s in [%jd,%jd] (= %jd secs)\n",
                  name1, name2, line[i], (intmax_t)min, (intmax_t)max, (intmax_t)value);
            return true;
        }
        else
        {
            CfDebug("Selection filter REJECTED counter range %s/%s = %s in [%" PRIdMAX ",%" PRIdMAX "] (= %" PRIdMAX " secs)\n", name1, name2,
                    line[i], (intmax_t)min, (intmax_t)max, (intmax_t)value);
            return false;
        }
    }

    return false;
}

/***************************************************************************/

static int SelectProcTimeAbsRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line)
{
    int i;
    time_t value;

    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, names)) != -1)
    {
        value = (time_t) TimeAbs2Int(line[i]);

        if (value == CF_NOINT)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "Failed to extract a valid integer from %c => \"%s\" in process list\n", name1[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Selection filter matched absolute %s/%s = %s in [%jd,%jd]\n", name1, name2, line[i],
                  (intmax_t)min, (intmax_t)max);
            return true;
        }
        else
        {
            return false;
        }
    }

    return false;
}

/***************************************************************************/

static int SelectProcRegexMatch(char *name1, char *name2, char *regex, char **colNames, char **line)
{
    int i;

    if (regex == NULL)
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, colNames)) != -1)
    {

        if (FullTextMatch(regex, line[i]))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    return false;
}

/*******************************************************************/

static int SplitProcLine(char *proc, char **names, int *start, int *end, char **line)
{
    int i, s, e;

    char *sp = NULL;
    char cols1[CF_PROCCOLS][CF_SMALLBUF] = { "" };
    char cols2[CF_PROCCOLS][CF_SMALLBUF] = { "" };

    CfDebug("SplitProcLine(%s)\n", proc);

    if ((proc == NULL) || (strlen(proc) == 0))
    {
        return false;
    }

    memset(line, 0, sizeof(char *) * CF_PROCCOLS);

// First try looking at all the separable items

    sp = proc;

    for (i = 0; (i < CF_PROCCOLS) && (names[i] != NULL); i++)
    {
        while (*sp == ' ')
        {
            sp++;
        }

        if ((strcmp(names[i], "CMD") == 0) || (strcmp(names[i], "COMMAND") == 0))
        {
            sscanf(sp, "%127[^\n]", cols1[i]);
            sp += strlen(cols1[i]);
        }
        else
        {
            sscanf(sp, "%127s", cols1[i]);
            sp += strlen(cols1[i]);
        }

        // Some ps stimes may contain spaces, e.g. "Jan 25"
        if ((strcmp(names[i], "STIME") == 0) && (strlen(cols1[i]) == 3))
        {
            char s[CF_SMALLBUF] = { 0 };
            sscanf(sp, "%127s", s);
            strcat(cols1[i], " ");
            strcat(cols1[i], s);
            sp += strlen(s) + 1;
        }
    }

// Now try looking at columne alignment

    for (i = 0; (i < CF_PROCCOLS) && (names[i] != NULL); i++)
    {
        // Start from the header/column tab marker and count backwards until we find 0 or space
        for (s = start[i]; (s >= 0) && (!isspace((int) *(proc + s))); s--)
        {
        }

        if (s < 0)
        {
            s = 0;
        }

        // Make sure to strip off leading spaces
        while (isspace((int) proc[s]))
        {
            s++;
        }

        if ((strcmp(names[i], "CMD") == 0) || (strcmp(names[i], "COMMAND") == 0))
        {
            e = strlen(proc);
        }
        else
        {
            for (e = end[i]; (e <= end[i] + 10) && (!isspace((int) *(proc + e))); e++)
            {
            }

            while (isspace((int) proc[e]))
            {
                if (e > 0)
                {
                    e--;
                }

                if(e == 0)
                {
                    break;
                }
            }
        }

        if (s <= e)
        {
            strncpy(cols2[i], (char *) (proc + s), MIN(CF_SMALLBUF - 1, (e - s + 1)));
        }
        else
        {
            cols2[i][0] = '\0';
        }

        if (Chop(cols2[i], CF_EXPANDSIZE) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
        }

        if (strcmp(cols2[i], cols1[i]) != 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unacceptable model uncertainty examining processes");
        }

        line[i] = xstrdup(cols1[i]);
    }

    return true;
}

/*******************************************************************/

static int GetProcColumnIndex(char *name1, char *name2, char **names)
{
    int i;

    for (i = 0; names[i] != NULL; i++)
    {
        if ((strcmp(names[i], name1) == 0) || (strcmp(names[i], name2) == 0))
        {
            return i;
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " INFO - process column %s/%s was not supported on this system", name1, name2);
    return -1;
}

/**********************************************************************************/

bool IsProcessNameRunning(char *procNameRegex)
{
    char *colHeaders[CF_PROCCOLS];
    Item *ip;
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    bool matched = false;
    int i;

    if (PROCESSTABLE == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! IsProcessNameRunning: PROCESSTABLE is empty");
        return false;
    }

    GetProcessColumnNames(PROCESSTABLE->name, (char **) colHeaders, start, end);

    for (ip = PROCESSTABLE->next; ip != NULL; ip = ip->next)    // iterate over ps lines
    {
        char *lineSplit[CF_PROCCOLS];

        if (NULL_OR_EMPTY(ip->name))
        {
            continue;
        }

        if (!SplitProcLine(ip->name, colHeaders, start, end, lineSplit))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! IsProcessNameRunning: Could not split process line \"%s\"", ip->name);
            continue;
        }

        if (SelectProcRegexMatch("CMD", "COMMAND", procNameRegex, colHeaders, lineSplit))
        {
            matched = true;
            break;
        }

        i = 0;
        while (lineSplit[i] != NULL)
        {
            free(lineSplit[i]);
            i++;
        }
    }

    i = 0;
    while (colHeaders[i] != NULL)
    {
        free(colHeaders[i]);
        i++;
    }

    return matched;
}


static void GetProcessColumnNames(char *proc, char **names, int *start, int *end)
{
    char *sp, title[16];
    int col, offset = 0;

    for (col = 0; col < CF_PROCCOLS; col++)
    {
        start[col] = end[col] = -1;
        names[col] = NULL;
    }

    col = 0;

    for (sp = proc; *sp != '\0'; sp++)
    {
        offset = sp - proc;

        if (isspace((int) *sp))
        {
            if (start[col] != -1)
            {
                CfDebug("End of %s is %d\n", title, offset - 1);
                end[col++] = offset - 1;
                if (col > CF_PROCCOLS - 1)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Column overflow in process table");
                    break;
                }
            }
            continue;
        }

        else if (start[col] == -1)
        {
            start[col] = offset;
            sscanf(sp, "%15s", title);
            CfDebug("Start of %s is %d\n", title, offset);
            names[col] = xstrdup(title);
            CfDebug("Col[%d]=%s\n", col, names[col]);
        }
    }

    if (end[col] == -1)
    {
        CfDebug("End of %s is %d\n", title, offset);
        end[col] = offset;
    }
}

static const char *GetProcessOptions(void)
{
# ifdef HAVE_GETZONEID
    zoneid_t zid;
    char zone[ZONENAME_MAX];
    static char psopts[CF_BUFSIZE];

    zid = getzoneid();
    getzonenamebyid(zid, zone, ZONENAME_MAX);

    if (strcmp(zone, "global") == 0)
    {
        snprintf(psopts, CF_BUFSIZE, "%s,zone", VPSOPTS[VSYSTEMHARDCLASS]);
        return psopts;
    }
# endif

# ifdef __linux__
    if (strncmp(VSYSNAME.release, "2.4", 3) == 0)
    {
        // No threads on 2.4 kernels
        return "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,stime,time,args";
    }

# endif

    return VPSOPTS[VSYSTEMHARDCLASS];
}

static int ExtractPid(char *psentry, char **names, int *start, int *end)
{
    char *sp;
    int col, pid = -1, offset = 0;

    for (col = 0; col < CF_PROCCOLS; col++)
    {
        if (strcmp(names[col], "PID") == 0)
        {
            if (col > 0)
            {
                offset = end[col - 1];
            }
            break;
        }
    }

    for (sp = psentry + offset; *sp != '\0'; sp++)      /* if first field contains alpha, skip */
    {
        /* If start with alphanum then skip it till the first space */

        if (isalnum((int) *sp))
        {
            while ((*sp != ' ') && (*sp != '\0'))
            {
                sp++;
            }
        }

        while ((*sp == ' ') && (*sp == '\t'))
        {
            sp++;
        }

        sscanf(sp, "%d", &pid);

        if (pid != -1)
        {
            break;
        }
    }

    return pid;
}

static int ForeignZone(char *s)
{
// We want to keep the banner

    if (strstr(s, "%CPU"))
    {
        return false;
    }

# ifdef HAVE_GETZONEID
    zoneid_t zid;
    char *sp, zone[ZONENAME_MAX];

    zid = getzoneid();
    getzonenamebyid(zid, zone, ZONENAME_MAX);

    if (strcmp(zone, "global") == 0)
    {
        if (strcmp(s + strlen(s) - 6, "global") == 0)
        {
            *(s + strlen(s) - 6) = '\0';

            for (sp = s + strlen(s) - 1; isspace(*sp); sp--)
            {
                *sp = '\0';
            }

            return false;
        }
        else
        {
            return true;
        }
    }
# endif
    return false;
}

#ifndef __MINGW32__
int LoadProcessTable(Item **procdata)
{
    FILE *prp;
    char pscomm[CF_MAXLINKSIZE], vbuff[CF_BUFSIZE], *sp;
    Item *rootprocs = NULL;
    Item *otherprocs = NULL;

    if (PROCESSTABLE)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Reusing cached process state");
        return true;
    }

    const char *psopts = GetProcessOptions();

    snprintf(pscomm, CF_MAXLINKSIZE, "%s %s", VPSCOMM[VSYSTEMHARDCLASS], psopts);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Observe process table with %s\n", pscomm);

    if ((prp = cf_popen(pscomm, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "popen", "Couldn't open the process list with command %s\n", pscomm);
        return false;
    }

    while (!feof(prp))
    {
        memset(vbuff, 0, CF_BUFSIZE);
        if (CfReadLine(vbuff, CF_BUFSIZE, prp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        for (sp = vbuff + strlen(vbuff) - 1; (sp > vbuff) && (isspace((int)*sp)); sp--)
        {
            *sp = '\0';
        }

        if (ForeignZone(vbuff))
        {
            continue;
        }

        AppendItem(procdata, vbuff, "");
    }

    cf_pclose(prp);

/* Now save the data */

    snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_procs", CFWORKDIR);
    RawSaveItemList(*procdata, vbuff);

    CopyList(&rootprocs, *procdata);
    CopyList(&otherprocs, *procdata);

    while (DeleteItemNotContaining(&rootprocs, "root"))
    {
    }

    while (DeleteItemContaining(&otherprocs, "root"))
    {
    }

    if (otherprocs)
    {
        PrependItem(&rootprocs, otherprocs->name, NULL);
    }

    snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_rootprocs", CFWORKDIR);
    RawSaveItemList(rootprocs, vbuff);
    DeleteItemList(rootprocs);

    snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_otherprocs", CFWORKDIR);
    RawSaveItemList(otherprocs, vbuff);
    DeleteItemList(otherprocs);

    return true;
}
#endif
