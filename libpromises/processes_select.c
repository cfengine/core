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

#include <processes_select.h>

#include <eval_context.h>
#include <files_names.h>
#include <conversion.h>
#include <matching.h>
#include <string_lib.h>
#include <item_lib.h>
#include <pipes.h>
#include <files_interfaces.h>
#include <rlist.h>
#include <policy.h>
#include <zones.h>

static int SelectProcRangeMatch(char *name1, char *name2, int min, int max, char **names, char **line);
static bool SelectProcRegexMatch(const char *name1, const char *name2, const char *regex, char **colNames, char **line);
static int SplitProcLine(char *proc, char **names, int *start, int *end, char **line);
static int SelectProcTimeCounterRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int SelectProcTimeAbsRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int GetProcColumnIndex(const char *name1, const char *name2, char **names);
static void GetProcessColumnNames(char *proc, char **names, int *start, int *end);
static int ExtractPid(char *psentry, char **names, int *end);

/***************************************************************************/

static int SelectProcess(char *procentry, char **names, int *start, int *end, ProcessSelect a)
{
    int result = true, i;
    char *column[CF_PROCCOLS];
    Rlist *rp;

    StringSet *process_select_attributes = StringSetNew();

    if (!SplitProcLine(procentry, names, start, end, column))
    {
        return false;
    }

    for (i = 0; names[i] != NULL; i++)
    {
        Log(LOG_LEVEL_DEBUG, "In SelectProcess, COL[%s] = '%s'", names[i], column[i]);
    }

    for (rp = a.owner; rp != NULL; rp = rp->next)
    {
        if (SelectProcRegexMatch("USER", "UID", RlistScalarValue(rp), names, column))
        {
            StringSetAdd(process_select_attributes, xstrdup("process_owner"));
            break;
        }
    }

    if (SelectProcRangeMatch("PID", "PID", a.min_pid, a.max_pid, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("pid"));
    }

    if (SelectProcRangeMatch("PPID", "PPID", a.min_ppid, a.max_ppid, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("ppid"));
    }

    if (SelectProcRangeMatch("PGID", "PGID", a.min_pgid, a.max_pgid, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("pgid"));
    }

    if (SelectProcRangeMatch("VSZ", "SZ", a.min_vsize, a.max_vsize, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("vsize"));
    }

    if (SelectProcRangeMatch("RSS", "RSS", a.min_rsize, a.max_rsize, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("rsize"));
    }

    if (SelectProcTimeCounterRangeMatch("TIME", "TIME", a.min_ttime, a.max_ttime, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("ttime"));
    }

    if (SelectProcTimeAbsRangeMatch
        ("STIME", "START", a.min_stime, a.max_stime, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("stime"));
    }

    if (SelectProcRangeMatch("NI", "PRI", a.min_pri, a.max_pri, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("priority"));
    }

    if (SelectProcRangeMatch("NLWP", "NLWP", a.min_thread, a.max_thread, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("threads"));
    }

    if (SelectProcRegexMatch("S", "STAT", a.status, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("status"));
    }

    if (SelectProcRegexMatch("CMD", "COMMAND", a.command, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("command"));
    }

    if (SelectProcRegexMatch("TTY", "TTY", a.tty, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("tty"));
    }

    if (!a.process_result)
    {
        if (StringSetSize(process_select_attributes) == 0)
        {
            result = EvalProcessResult("", process_select_attributes);
        }
        else
        {
            Writer *w = StringWriter();
            StringSetIterator iter = StringSetIteratorInit(process_select_attributes);
            char *attr = StringSetIteratorNext(&iter);
            WriterWrite(w, attr);

            while ((attr = StringSetIteratorNext(&iter)))
            {
                WriterWriteChar(w, '.');
                WriterWrite(w, attr);
            }

            result = EvalProcessResult(StringWriterData(w), process_select_attributes);
            WriterClose(w);
        }
    }
    else
    {
        result = EvalProcessResult(a.process_result, process_select_attributes);
    }

    StringSetDestroy(process_select_attributes);

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

    pcre *rx = CompileRegex(process_name);
    if (rx)
    {
        for (Item *ip = processes->next; ip != NULL; ip = ip->next)
        {
            int s, e;

            if (StringMatchWithPrecompiledRegex(rx, ip->name, &s, &e))
            {
                if (NULL_OR_EMPTY(ip->name))
                {
                    continue;
                }

                if (attrselect && !SelectProcess(ip->name, names, start, end, a))
                {
                    continue;
                }

                pid_t pid = ExtractPid(ip->name, names, end);

                if (pid == -1)
                {
                    Log(LOG_LEVEL_VERBOSE, "Unable to extract pid while looking for %s", process_name);
                    continue;
                }

                PrependItem(&result, ip->name, "");
                result->counter = (int)pid;
            }
        }

        pcre_free(rx);
    }

    for (int i = 0; i < CF_PROCCOLS; i++)
    {
        free(names[i]);
    }

    return result;
}

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
            Log(LOG_LEVEL_INFO, "Failed to extract a valid integer from '%s' => '%s' in process list", names[i],
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
            return CF_NOINT;
        }
    }
    else
    {
        if (sscanf(s, "%ld:%ld", &h, &m) != 2)
        {
            snprintf(output, CF_BUFSIZE, "Unable to parse TIME 'ps' field, expected hH:mm, got '%s'", s);
            return CF_NOINT;
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
            Log(LOG_LEVEL_INFO, "Failed to extract a valid integer from %c => '%s' in process list", name1[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            Log(LOG_LEVEL_VERBOSE, "Selection filter matched counter range '%s/%s' = '%s' in [%jd,%jd] (= %jd secs)",
                  name1, name2, line[i], (intmax_t)min, (intmax_t)max, (intmax_t)value);
            return true;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG,
                "Selection filter REJECTED counter range '%s/%s' = '%s' in [%jd,%jd] (= %jd secs)",
                name1, name2, line[i],
                (intmax_t) min, (intmax_t) max, (intmax_t) value);
            return false;
        }
    }

    return false;
}

static time_t TimeAbs2Int(const char *s)
{
    if (s == NULL)
    {
        return CF_NOINT;
    }

    struct tm tm;
    localtime_r(&CFSTARTTIME, &tm);
    tm.tm_sec = 0;
    tm.tm_isdst = -1;

    if (strstr(s, ":"))         /* Hr:Min */
    {
        char h[3], m[3];
        sscanf(s, "%2[^:]:%2[^:]:", h, m);
        tm.tm_hour = IntFromString(h);
        tm.tm_min = IntFromString(m);
    }
    else                        /* Month day */
    {
        char mon[4];
        long day;
        sscanf(s, "%3[a-zA-Z] %ld", mon, &day);
        int month = Month2Int(mon);
        if (tm.tm_mon < month - 1)
        {
            /* Wrapped around */
            tm.tm_year--;
        }
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = 0;
        tm.tm_min = 0;
    }

    return mktime(&tm);
}

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
        value = TimeAbs2Int(line[i]);

        if (value == CF_NOINT)
        {
            Log(LOG_LEVEL_INFO, "Failed to extract a valid integer from %c => '%s' in process list", name1[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            Log(LOG_LEVEL_VERBOSE, "Selection filter matched absolute '%s/%s' = '%s(%jd)' in [%jd,%jd]", name1, name2, line[i], (intmax_t)value,
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

static bool SelectProcRegexMatch(const char *name1, const char *name2,
                                 const char *regex, char **colNames, char **line)
{
    int i;

    if (regex == NULL)
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, colNames)) != -1)
    {

        if (StringMatchFull(regex, line[i]))
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
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }

        if (strcmp(cols2[i], cols1[i]) != 0)
        {
            Log(LOG_LEVEL_INFO, "Unacceptable model uncertainty examining processes");
        }

        line[i] = xstrdup(cols1[i]);
    }

    return true;
}

/*******************************************************************/

static int GetProcColumnIndex(const char *name1, const char *name2, char **names)
{
    int i;

    for (i = 0; names[i] != NULL; i++)
    {
        if ((strcmp(names[i], name1) == 0) || (strcmp(names[i], name2) == 0))
        {
            return i;
        }
    }

    Log(LOG_LEVEL_VERBOSE, " INFO - process column %s/%s was not supported on this system", name1, name2);
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
        Log(LOG_LEVEL_ERR, "IsProcessNameRunning: PROCESSTABLE is empty");
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
            Log(LOG_LEVEL_ERR, "IsProcessNameRunning: Could not split process line '%s'", ip->name);
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
                Log(LOG_LEVEL_DEBUG, "End of '%s' is %d", title, offset - 1);
                end[col++] = offset - 1;
                if (col > CF_PROCCOLS - 1)
                {
                    Log(LOG_LEVEL_ERR, "Column overflow in process table");
                    break;
                }
            }
            continue;
        }

        else if (start[col] == -1)
        {
            start[col] = offset;
            sscanf(sp, "%15s", title);
            Log(LOG_LEVEL_DEBUG, "Start of '%s' is %d", title, offset);
            names[col] = xstrdup(title);
            Log(LOG_LEVEL_DEBUG, "Col[%d] = '%s'", col, names[col]);
        }
    }

    if (end[col] == -1)
    {
        Log(LOG_LEVEL_DEBUG, "End of '%s' is %d", title, offset);
        end[col] = offset;
    }
}

#ifndef __MINGW32__
static const char *GetProcessOptions(void)
{
    static char psopts[CF_BUFSIZE]; /* GLOBAL_R, no initialization needed */

    if (IsGlobalZone())
    {
        snprintf(psopts, CF_BUFSIZE, "%s,zone", VPSOPTS[VSYSTEMHARDCLASS]);
        return psopts;
    }

# ifdef __linux__
    if (strncmp(VSYSNAME.release, "2.4", 3) == 0)
    {
        // No threads on 2.4 kernels
        return "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,stime,time,args";
    }

# endif

    return VPSOPTS[VSYSTEMHARDCLASS];
}
#endif

static int ExtractPid(char *psentry, char **names, int *end)
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

#ifndef __MINGW32__
int LoadProcessTable(Item **procdata)
{
    FILE *prp;
    char pscomm[CF_MAXLINKSIZE], *sp = NULL;
    Item *rootprocs = NULL;
    Item *otherprocs = NULL;

    if (PROCESSTABLE)
    {
        Log(LOG_LEVEL_VERBOSE, "Reusing cached process table");
        return true;
    }

    const char *psopts = GetProcessOptions();

    snprintf(pscomm, CF_MAXLINKSIZE, "%s %s", VPSCOMM[VSYSTEMHARDCLASS], psopts);

    Log(LOG_LEVEL_VERBOSE, "Observe process table with %s", pscomm);

    if ((prp = cf_popen(pscomm, "r", false)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open the process list with command '%s'. (popen: %s)", pscomm, GetErrorStr());
        return false;
    }

    size_t vbuff_size = CF_BUFSIZE;
    char *vbuff = xmalloc(vbuff_size);

    for (;;)
    {
        ssize_t res = CfReadLine(&vbuff, &vbuff_size, prp);
        if (res == -1)
        {
            if (!feof(prp))
            {
                Log(LOG_LEVEL_ERR, "Unable to read process list with command '%s'. (fread: %s)", pscomm, GetErrorStr());
                cf_pclose(prp);
                free(vbuff);
                return false;
            }
            else
            {
                break;
            }
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
    RawSaveItemList(*procdata, vbuff, NewLineMode_Unix);

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
    RawSaveItemList(rootprocs, vbuff, NewLineMode_Unix);
    DeleteItemList(rootprocs);

    snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_otherprocs", CFWORKDIR);
    RawSaveItemList(otherprocs, vbuff, NewLineMode_Unix);
    DeleteItemList(otherprocs);

    free(vbuff);
    return true;
}
#endif
