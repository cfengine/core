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

#include "cf3.defs.h"

#include "env_context.h"
#include "files_names.h"
#include "conversion.h"
#include "reporting.h"
#include "matching.h"
#include "cfstream.h"
#include "verify_processes.h"

static int SelectProcRangeMatch(char *name1, char *name2, int min, int max, char **names, char **line);
static int SelectProcRegexMatch(char *name1, char *name2, char *regex, char **colNames, char **line);
static int SplitProcLine(char *proc, char **names, int *start, int *end, char **line);
static int SelectProcTimeCounterRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int SelectProcTimeAbsRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int GetProcColumnIndex(char *name1, char *name2, char **names);

/***************************************************************************/

int SelectProcess(char *procentry, char **names, int *start, int *end, Attributes a, Promise *pp)
{
    AlphaList proc_attr;
    int result = true, i;
    char *column[CF_PROCCOLS];
    Rlist *rp;

    CfDebug("SelectProcess(%s)\n", procentry);

    InitAlphaList(&proc_attr);

    if (!a.haveselect)
    {
        return true;
    }

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

    for (rp = a.process_select.owner; rp != NULL; rp = rp->next)
    {
        if (SelectProcRegexMatch("USER", "UID", (char *) rp->item, names, column))
        {
            PrependAlphaList(&proc_attr, "process_owner");
            break;
        }
    }

    if (SelectProcRangeMatch("PID", "PID", a.process_select.min_pid, a.process_select.max_pid, names, column))
    {
        PrependAlphaList(&proc_attr, "pid");
    }

    if (SelectProcRangeMatch("PPID", "PPID", a.process_select.min_ppid, a.process_select.max_ppid, names, column))
    {
        PrependAlphaList(&proc_attr, "ppid");
    }

    if (SelectProcRangeMatch("PGID", "PGID", a.process_select.min_pgid, a.process_select.max_pgid, names, column))
    {
        PrependAlphaList(&proc_attr, "pgid");
    }

    if (SelectProcRangeMatch("VSZ", "SZ", a.process_select.min_vsize, a.process_select.max_vsize, names, column))
    {
        PrependAlphaList(&proc_attr, "vsize");
    }

    if (SelectProcRangeMatch("RSS", "RSS", a.process_select.min_rsize, a.process_select.max_rsize, names, column))
    {
        PrependAlphaList(&proc_attr, "rsize");
    }

    if (SelectProcTimeCounterRangeMatch
        ("TIME", "TIME", a.process_select.min_ttime, a.process_select.max_ttime, names, column))
    {
        PrependAlphaList(&proc_attr, "ttime");
    }

    if (SelectProcTimeAbsRangeMatch
        ("STIME", "START", a.process_select.min_stime, a.process_select.max_stime, names, column))
    {
        PrependAlphaList(&proc_attr, "stime");
    }

    if (SelectProcRangeMatch("NI", "PRI", a.process_select.min_pri, a.process_select.max_pri, names, column))
    {
        PrependAlphaList(&proc_attr, "priority");
    }

    if (SelectProcRangeMatch("NLWP", "NLWP", a.process_select.min_thread, a.process_select.max_thread, names, column))
    {
        PrependAlphaList(&proc_attr, "threads");
    }

    if (SelectProcRegexMatch("S", "STAT", a.process_select.status, names, column))
    {
        PrependAlphaList(&proc_attr, "status");
    }

    if (SelectProcRegexMatch("CMD", "COMMAND", a.process_select.command, names, column))
    {
        PrependAlphaList(&proc_attr, "command");
    }

    if (SelectProcRegexMatch("TTY", "TTY", a.process_select.tty, names, column))
    {
        PrependAlphaList(&proc_attr, "tty");
    }

    result = EvalProcessResult(a.process_select.process_result, &proc_attr);
   
    DeleteAlphaList(&proc_attr);

    if (result)
    {
        if (a.transaction.action == cfa_warn)
        {
            CfOut(cf_error, "", " !! Matched: %s\n", procentry);
        }
        else
        {
            CfOut(cf_inform, "", " !! Matched: %s\n", procentry);
        }
    }

    for (i = 0; column[i] != NULL; i++)
    {
        free(column[i]);
    }

    return result;
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
        value = Str2Int(line[i]);

        if (value == CF_NOINT)
        {
            CfOut(cf_inform, "", "Failed to extract a valid integer from %s => \"%s\" in process list\n", names[i],
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
            ReportError(output);
        }
    }
    else
    {
        if (sscanf(s, "%ld:%ld", &h, &m) != 2)
        {
            snprintf(output, CF_BUFSIZE, "Unable to parse TIME 'ps' field, expected hH:mm, got '%s'", s);
            ReportError(output);
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
            CfOut(cf_inform, "", "Failed to extract a valid integer from %c => \"%s\" in process list\n", name1[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            CfOut(cf_verbose, "", "Selection filter matched counter range %s/%s = %s in [%jd,%jd] (= %jd secs)\n",
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
            CfOut(cf_inform, "", "Failed to extract a valid integer from %c => \"%s\" in process list\n", name1[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            CfOut(cf_verbose, "", "Selection filter matched absolute %s/%s = %s in [%jd,%jd]\n", name1, name2, line[i],
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

        Chop(cols2[i]);

        if (strcmp(cols2[i], cols1[i]) != 0)
        {
            CfOut(cf_inform, "", " !! Unacceptable model uncertainty examining processes");
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

    CfOut(cf_verbose, "", " INFO - process column %s/%s was not supported on this system", name1, name2);
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

    if (PROCESSTABLE == NULL)
    {
        CfOut(cf_error, "", "!! IsProcessNameRunning: PROCESSTABLE is empty");
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
            CfOut(cf_error, "", "!! IsProcessNameRunning: Could not split process line \"%s\"", ip->name);
            continue;
        }

        if (SelectProcRegexMatch("CMD", "COMMAND", procNameRegex, colHeaders, lineSplit))
        {
            matched = true;
            break;
        }
    }

    return matched;
}
