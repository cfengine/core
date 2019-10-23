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

/*
 * Portability notes:
 *
 * The details of the per-process "/proc/nnn" vary across OSes.
 *
 * The initial draft is based on Linux: RHEL6 and RHEL7.
 * It may need "ifdef..." type adjustment on other kernels.
 */

#include <processes_select.h>

#include <string.h>
#include <sys/user.h>

#include <eval_context.h>
#include <files_names.h>
#include <conversion.h>
#include <item_lib.h>
#include <dir.h>


#define PROCDIR "/proc"

#define NPROC_GUESS 500

// keys for Json structure
#define JPROC_KEY_UID       "uid"      /* uid */
#define JPROC_KEY_UNAME     "uname"    /* username */
#define JPROC_KEY_CMD       "cmd"      /* cmd (argv[0]) only */
#define JPROC_KEY_CMDLINE   "cmdline"  /* cmd line and args */
#define JPROC_KEY_PSTATE    "pstate"   /* process state */
#define JPROC_KEY_PPID      "ppid"     /* parent pid */
#define JPROC_KEY_PGID      "pgid"     /* process group */
#define JPROC_KEY_TTY       "tty"      /* terminal dev_t */
#define JPROC_KEY_TTYNAME   "ttyname"  /* terminal name (e.g. "pts/nnn" */
#define JPROC_KEY_PRIORITY  "priority" /* priority */
#define JPROC_KEY_THREADS   "threads"  /* threads */
#define JPROC_KEY_CPUTIME   "ttime"    /* elapsed time (seconds) */
#define JPROC_KEY_STARTTIME "stime"    /* starttime (seconds since epoch) */
#define JPROC_KEY_RES_KB    "rkb"      /* resident kb */
#define JPROC_KEY_VIRT_KB   "vkb"      /* virtual kb */

/*
 * JSON "proc" info.
 *
 * A list of objects keyed by 'pid', reflecting "/proc/<pid>".
 *
 * {
 *   "i" : { "ppid" : i_parent, "cmd" : i_cmd, ... },
 *   "j" : { "ppid" : j_parent, "cmd" : j_cmd, ... },
 *   ...
 *   },
 * }
 */

static JsonElement *PROCTABLE = NULL;

long sys_boot_time = -1;

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
    const char *uname;
    for (rp = a->owner; rp != NULL; rp = rp->next)
    {
        uname = JsonObjectGetAsString(pdata, JPROC_KEY_UNAME);
        if (SelectProcRegexMatch(uname, RlistScalarValue(rp), true))
        {
            StringSetAdd(process_select_attributes, xstrdup("process_owner"));
            break;
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

    pdtime = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_STARTTIME));
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

    if (PROCTABLE == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: PROCESSTABLE is empty", __func__);
        return NULL;
    }

    const char *cmd;
    pid_t pid;
    const JsonElement *pdata;
    JsonIterator iter = JsonIteratorInit(PROCTABLE);
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

    if (PROCTABLE == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: PROCESSTABLE is empty", __func__);
        return false;
    }

    const JsonElement *pdata;
    const char *cmd;
    JsonIterator iter = JsonIteratorInit(PROCTABLE);
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


const char *GetProcessTableLegend(void)
{
    if (PROCTABLE)
    {
        return "<via /proc>";
    }
    else
    {
        return "<Process table not loaded>";
    }
}

/***************************************************************************/

/*
 * LoadProcessTable() and subordinates
 *
 * ClearProcessTable()
 */

/* "/proc/nnn" is a process directory only if pure integer "nnn" */
static bool IsProcDir(const char *name)
{
    const char *p = name;

    while (*p)
    {
        if (!isdigit(*p)) {
            return false;
        }
        p++;
    }

    return true;
}

/*
 * Obtain:
 *    boottime (seconds since epoch)
 */
static bool LoadMisc(void)
{
    char statfile[CF_MAXVARSIZE];
    FILE *fd;
    char statbuf[CF_MAXVARSIZE];
    char key[64];	// see also sscanf() below

    sys_boot_time = -1;

    snprintf(statfile, CF_MAXVARSIZE, "/%s/stat", PROCDIR);
    fd = fopen(statfile, "r");
    if (!fd)
    {
        return false;
    }

    while (fgets(statbuf, CF_MAXVARSIZE - 1, fd))
    {
        sscanf(statbuf, "%63s %lu", key, &sys_boot_time);

        if (strcmp(key, "btime") == 0) {
            break;
        }
    }

    return true;
}

/*
 * set:
 *   'cmd' -> ARGV[0]
 *   'cmdline' -> space-separated ARGLIST
 */
static bool LoadProcCmd(pid_t pid, JsonElement *pdata)
{
    char statfile[CF_MAXVARSIZE];
    FILE *fd;
    char cmd[CF_MAXVARSIZE];
    char *pt, *ept;

    /*
     * "/proc/.../cmdline" is "ARGV[0]\0ARGV[1]\0...ARGV[n]\0\0"
     * For the moment we are only interested in ARGV0 (command name).
     */
    snprintf(statfile, CF_MAXVARSIZE, "/%s/%d/cmdline", PROCDIR, pid);
    fd = fopen(statfile, "r");
    if (!fd)
    {
        return false;
    }

    memset(cmd, '\0', CF_MAXVARSIZE);
    pt = fgets(cmd, CF_MAXVARSIZE, fd);
    if (pt)
    {
        /* key 'cmd': ARGV[0] */
        JsonObjectAppendString(pdata, JPROC_KEY_CMD, cmd);

        /*
         * Produce full command.
         *     Find end of 'cmdline'.
         *     Convert intermediate '\0' to space.
         */

        /* back along buffer until ept -> last char (end of ARGV[n]) */
        ept = &cmd[CF_MAXVARSIZE-1];
        while (ept != cmd && *ept == '\0')
        {
            ept--;
        }

        /* back along buffer changing NULL to space */
        while (ept != cmd)
        {
            if (*ept == '\0')
            {
                *ept = ' ';
            }
            ept--;
        }

        /* key 'cmdline': "ARGV[0] ARGV[1] ARGV[2] ... ARGV[n]" inc. spaces */
        JsonObjectAppendString(pdata, JPROC_KEY_CMDLINE, cmd);
    }
    fclose(fd);

    return true;
}

/*
 * Read "/proc/pid/..." data into the "pdata" structure.
 * There are likely to be OS dependencies.
 *
 * Most of the data comes from file 'stat'.
 * But the uid is from 'status'.
 */

// Obtain and store uid/uname
static bool LoadProcUid(JsonElement *pdata, pid_t pid)
{
    char statusfile[CF_MAXVARSIZE];

    snprintf(statusfile, CF_MAXVARSIZE, "/%s/%d/status", PROCDIR, pid);

    FILE *stream;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    stream = fopen(statusfile, "r");
    if (stream == NULL)
    {
        return false;
    }

    uid_t uid;
    bool found = false;
    char key[CF_MAXVARSIZE];
    while ((nread = getline(&line, &len, stream)) != -1)
    {
        sscanf(line, "%s %d ", key, &uid);
        if (strcasecmp("Uid:", key) == 0)
        {
            found = true;
            break;
        }
    }

    // tidy up
    free(line);
    fclose(stream);

    // act on file-scan result

    // a "shouldn't happen" error
    if (!found)
    {
        Log(LOG_LEVEL_ERR, "error searching for uid %d in %s",
          uid, statusfile);

        return false;
    }

    JsonObjectAppendInteger(pdata, JPROC_KEY_UID, uid);

    // "probably shouldn't happen"... uid of process is untranslatable into name
    struct passwd *pwd = getpwuid(uid);
    if (!pwd)
    {
        Log(LOG_LEVEL_WARNING, "could not translate uid %d into a username", uid);
        return false;
    }

    JsonObjectAppendString(pdata, JPROC_KEY_UNAME, pwd->pw_name);

    return found;
}

/*
 * The caller is looking for a pattern such as "pts/[0-9]+".
 * Here we examine likely places under "/dev".
 * In this initial implementation we only look for "/dev/pts/nnn"/.
 *
 * The "/proc" data has given us the major/minor device "ttyn".
 * We look through our possible candidate list in "/dev" for a match.
 */
static bool LoadProcTTY(JsonElement *pdata, pid_t pid, int ttyn)
{
    char ttyname[CF_MAXVARSIZE];
    struct stat statbuf;
    int ttymajor = major(ttyn);
    int ttyminor = minor(ttyn);

    JsonObjectAppendInteger(pdata, JPROC_KEY_TTY, ttyn);

    /*
     * From kernel "devices.txt", character-major 136-143 inclusive are "pty".
     * We do a quick sanity-check on "/dev/pts/nnnn".
     * If it looks OK, we set the name "pts/nnn".
     */
    if (ttymajor >= 136 && ttymajor <= 143)
    {
        snprintf(ttyname, CF_MAXVARSIZE, "/dev/pts/%d", ttyminor);
        if (stat(ttyname, &statbuf) >= 0)
        {
            JsonObjectAppendString(pdata, JPROC_KEY_TTYNAME, &ttyname[5]);
            return true;
        }
    }

    /*
     * Could probably check for other things; "/dev/tty<n>" springs to mind.
     * But for now, fail safely.
     */

    return false;
}

static JsonElement *LoadProcStat(pid_t pid)
{
    char statfile[CF_MAXVARSIZE];
    int fd;
    JsonElement *pdata;

    pdata = JsonObjectCreate(12);

    /* get uid from file 'status' */
    if (! LoadProcUid(pdata, pid))
    {
        return pdata;
    }

    /* extract command line info and store in 'pdata' */
    LoadProcCmd(pid, pdata);

    /* open the 'stat' file */
    snprintf(statfile, CF_MAXVARSIZE, "/%s/%d/stat", PROCDIR, pid);
    fd = open(statfile, O_RDONLY, 0);
    if (fd < 0) {
        return pdata;
    }

    /* read the 'stat' file into a buffer */
    char statbuf[CF_MAXVARSIZE];
    read(fd, statbuf, CF_MAXVARSIZE -1);
    close(fd);

    /*
     * Big picture: "stat" is:
     *    pid (some text here) S n1 n2 n3 ...
     * We already know the pid. And the text is a variant of command (known).
     * We are interested in the state char 'S' and numbers n1, n2, n3, ...
     */

    /* Find closing ')' and place pointer on the char 'S'. */
    char *pt;
    pt = strrchr(statbuf, ')');
    pt++;
    pt++;

    /* Extract the 'stat' values.  May vary with OS/release. */

#if defined(__linux__)

    char pstate[2];
    pid_t ppid;
    pid_t pgid;
    int ttyn;
    long utime;
    long stime;
    long cpusecs;
    long priority;
    long nice;
    long threads;
    long long starttime;
    ulong vsize;
    long rss;

    pstate[1] = '\0';
    sscanf(pt,
      "%c "
      "%d %d %*d %d %*d %*u "
      "%*u %*u %*u %*u "
      "%lu %lu %*d %*d "
      "%ld %ld %ld %*d "
      "%llu %lu %ld %*d ",
      &pstate[0],
      &ppid, &pgid, &ttyn,
      &utime, &stime,
      &priority, &nice, &threads,
      &starttime, &vsize, &rss);

    /*
     * Transform numbers/units from OS into CFEngine-preferred number/units.
     */
    // proc bytes => CFEngine kilobytes
    vsize /= 1024;

    // proc PAGE SIZE => CFEngine kilobytes
    rss *= (PAGE_SIZE/1024);

    // proc clock ticks of process => CFEngine CPU seconds
    cpusecs = (utime + stime) / sysconf(_SC_CLK_TCK);

    // proc 'jiffies' since boot => CFEngine seconds since epoch
    starttime /= sysconf(_SC_CLK_TCK);
    starttime += sys_boot_time;

    // tty major/minor
    LoadProcTTY(pdata, pid, ttyn);

    /*
     * Add data to the Json structure
     */
    JsonObjectAppendString(pdata, JPROC_KEY_PSTATE, pstate);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PPID, ppid);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PGID, pgid);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PRIORITY, priority);
    JsonObjectAppendInteger(pdata, JPROC_KEY_THREADS, threads);
    JsonObjectAppendInteger(pdata, JPROC_KEY_CPUTIME, cpusecs);
    JsonObjectAppendInteger(pdata, JPROC_KEY_STARTTIME, starttime);
    JsonObjectAppendInteger(pdata, JPROC_KEY_VIRT_KB, vsize);
    JsonObjectAppendInteger(pdata, JPROC_KEY_RES_KB, rss);

#elif defined(BSD)

#error "Support for '/proc/<pid>' not yet written for BSD. Can you provide it?"

#elif defined(__sun)

#error "Support for '/proc/<pid>' not yet written for Sun/Solaris. Can you provide it?"

#else

#error "Support for '/proc/<pid>' not yet available."

#endif

    return pdata;
}

bool LoadProcessTable()
{
    Dir *dirh = NULL;
    const struct dirent *dirp;

    if (PROCTABLE)
    {
        Log(LOG_LEVEL_VERBOSE, "Reusing cached process table");
        return true;
    }

    /* Get misc. data (e.g. system boot time) */
    if (! LoadMisc())
    {
        Log(LOG_LEVEL_ERR, "Error loading miscellaneous information");
        return false;
    }

    if ((dirh = DirOpen(PROCDIR)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open %s directory'. (opendir: %s)", PROCDIR, GetErrorStr());
        return false;
    }

    PROCTABLE = JsonObjectCreate(NPROC_GUESS);

    pid_t  pid;
    JsonElement *pdata;
    while ((dirp = DirRead(dirh)) != NULL)
    {
        /*
         * Process next entry. Skip non-numeric names as being non-process.
         */
        if (! IsProcDir(dirp->d_name))
        {
            continue;
        }

        pid = atol(dirp->d_name);

        /* It ought to be a directory... */
        if (dirp->d_type != DT_DIR)
        {
            Log(LOG_LEVEL_ERR, "'%s' not a directory\n", dirp->d_name);
            continue;
        }

        pdata = LoadProcStat(pid);
        if (pdata == NULL)
        {
            Log(LOG_LEVEL_ERR, "failure creating 'stat' data for '%s'\n", dirp->d_name);
            continue;
        }

        JsonObjectAppendObject(PROCTABLE, dirp->d_name, pdata);

    }

    DirClose(dirh);

    return true;

}

void ClearProcessTable(void)
{
    JsonDestroy(PROCTABLE);
    PROCTABLE= NULL;
}
