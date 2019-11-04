/*
   Copyright 2019 Northern.tech AS

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

/* **********************************************************
 * Let process state be examined, and processes be selected.
 *
 * Using "/proc" is preferable to using "ps ..." commands.
 * The earlier "processes_select.c" is a useful backstop
 * but should be avoided where reasonably possible
 * in favour for this "/proc" version.
 *
 * (David Lee, 2019)
 ********************************************************** */

#include <processes_select.h>
#include <process_unix_priv.h>
#include <process_lib.h>

#include <eval_context.h>
#include <conversion.h>


/*
 * internal functions
 */
static bool SelectProcRangeMatch(int value, int min, int max);
static bool SelectProcTimeCounterRangeMatch(time_t value, time_t min, time_t max);
static bool SelectProcRegexMatch(const char *str, const char *regex, bool anchored);

/***************************************************************************/

static bool SelectProcess(pid_t pid, const JsonElement *pdata,
                          const char *process_regex,
                          const ProcessSelect *a,
                          bool attrselect)
{
    const char *cmdline;
    const char *status;

    bool result = false;

    assert(process_regex);
    assert(a != NULL);

    cmdline = JsonObjectGetAsString(pdata, JPROC_KEY_CMDLINE);
    if (!cmdline)
    {
        return false;
    }

    status = JsonObjectGetAsString(pdata, JPROC_KEY_PSTATE);
    if (!status)
    {
        status = " ";
    }

    result = SelectProcRegexMatch(cmdline, process_regex, false);
    if (!result)
    {
        return false;
    }

    if (!attrselect)
    {
        // If we are not considering attributes, then the matching is done.
        return result;
    }

    StringSet *process_select_attributes = StringSetNew();

    Rlist *rp;
    uid_t uid;
    const char *uname;
    struct passwd *pwd;
    for (rp = a->owner; rp != NULL; rp = rp->next)
    {
        if (rp->val.type == RVAL_TYPE_FNCALL)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Function call '%s' in process_select body was not resolved, skipping",
                RlistFnCallValue(rp)->name);
        }



        else {
            uid = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_EUID));
            pwd = getpwuid(uid);
            if (!pwd)
            {
                // "probably shouldn't happen"... uid is untranslatable into name
                Log(LOG_LEVEL_WARNING, "could not translate uid %d into a username", uid);
                continue;
            }

            uname = pwd->pw_name;
            if (SelectProcRegexMatch(uname, RlistScalarValue(rp), true))
            {
                StringSetAdd(process_select_attributes, xstrdup("process_owner"));
                break;
            }
        }
    }

    int pdvalue;
    time_t pdtime;

    pdvalue = pid;
    if (SelectProcRangeMatch(pdvalue, a->min_pid, a->max_pid))
    {
        StringSetAdd(process_select_attributes, xstrdup("pid"));
    }

    pdvalue = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_PPID));
    if (SelectProcRangeMatch(pdvalue, a->min_ppid, a->max_ppid))
    {
        StringSetAdd(process_select_attributes, xstrdup("ppid"));
    }

    pdvalue = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_PGID));
    if (SelectProcRangeMatch(pdvalue, a->min_pgid, a->max_pgid))
    {
        StringSetAdd(process_select_attributes, xstrdup("pgid"));
    }

    pdvalue = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_VIRT_KB));
    if (SelectProcRangeMatch(pdvalue, a->min_vsize, a->max_vsize))
    {
        StringSetAdd(process_select_attributes, xstrdup("vsize"));
    }

    pdvalue = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_RES_KB));
    if (SelectProcRangeMatch(pdvalue, a->min_rsize, a->max_rsize))
    {
        StringSetAdd(process_select_attributes, xstrdup("rsize"));
    }

    pdtime = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_CPUTIME));
    if (SelectProcTimeCounterRangeMatch(pdtime, a->min_ttime, a->max_ttime))
    {
        StringSetAdd(process_select_attributes, xstrdup("ttime"));
    }

    pdtime = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_STARTTIME_EPOCH));
    if (SelectProcTimeCounterRangeMatch(pdtime, a->min_stime, a->max_stime))
    {
        StringSetAdd(process_select_attributes, xstrdup("stime"));
    }

    pdvalue = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_PRIORITY));
    if (SelectProcRangeMatch(pdvalue, a->min_pri, a->max_pri))
    {
        StringSetAdd(process_select_attributes, xstrdup("priority"));
    }

    pdvalue = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_THREADS));
    if (SelectProcRangeMatch(pdvalue, a->min_thread, a->max_thread))
    {
        StringSetAdd(process_select_attributes, xstrdup("threads"));
    }

    if (SelectProcRegexMatch(status, a->status, true))
    {
        StringSetAdd(process_select_attributes, xstrdup("status"));
    }

    if (SelectProcRegexMatch(cmdline, a->command, true))
    {
        StringSetAdd(process_select_attributes, xstrdup("command"));
    }

    const char *ttyname;
    ttyname = JsonObjectGetAsString(pdata, JPROC_KEY_TTYNAME);
    if (ttyname && SelectProcRegexMatch(ttyname, a->tty, true))
    {
        StringSetAdd(process_select_attributes, xstrdup("tty"));
    }

    result = false;

    if (!a->process_result)
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
        result = EvalProcessResult(a->process_result, process_select_attributes);
    }

    StringSetDestroy(process_select_attributes);

    return result;
}

Item *SelectProcesses(const char *process_name, const ProcessSelect *a, bool attrselect)
{
    assert(a != NULL);
    Item *result = NULL;

    const JsonElement *ptable;
    ptable = FetchProcessTable();
    if (ptable == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: PROCESSTABLE is empty", __func__);
        return NULL;
    }

    const char *cmd;
    pid_t pid;
    const JsonElement *pdata;
    JsonIterator iter = JsonIteratorInit(ptable);
    while ((pdata = JsonIteratorNextValue(&iter)))
    {
        pid = IntFromString(JsonIteratorCurrentKey(&iter));

        if (!SelectProcess(pid, pdata, process_name, a, attrselect))
        {
            continue;
        }

        cmd = JsonObjectGetAsString(pdata, JPROC_KEY_CMD);
        PrependItem(&result, cmd, "");
        result->counter = (int)pid;
    }

    return result;
}

static bool SelectProcRangeMatch(int value, int min, int max)
{
    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    return ((min <= value) && (value <= max)) ? true : false;
}

static bool SelectProcTimeCounterRangeMatch(time_t value, time_t min, time_t max)
{
    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    return ((min <= value) && (value <= max)) ? true : false;
}

static bool SelectProcRegexMatch(const char *str, const char *regex, bool anchored)
{
    bool result;

    if (regex == NULL)
    {
        return false;
    }

    if (anchored)
    {
        result = StringMatchFull(regex, str);
    }
    else
    {
        int s, e;
        result = StringMatch(regex, str, &s, &e);
    }

    return result;
}

/***************************************************************************/

bool IsProcessNameRunning(char *procNameRegex)
{
    bool matched = false;

    const JsonElement *ptable;
    ptable = FetchProcessTable();
    if (ptable == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: PROCESSTABLE is empty", __func__);
        return false;
    }

    const JsonElement *pdata;
    const char *cmd;
    JsonIterator iter = JsonIteratorInit(ptable);
    while (!matched && (pdata = JsonIteratorNextValue(&iter)))
    {
            cmd = JsonObjectGetAsString(pdata, JPROC_KEY_CMD);
            if (!cmd)
            {
                continue;
            }

            matched = StringMatchFull(procNameRegex, cmd);
    }

    return matched;
}
