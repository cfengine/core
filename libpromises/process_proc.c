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
 * Create a JSON representation of processes from "/proc".
 *
 * The details of the per-process "/proc/nnn" vary across OSes.
 *
 * The initial draft is based on Linux: RHEL6 and RHEL7.
 * It may need "ifdef..." type adjustment on other kernels.
 *
 * (David Lee, 2019)
 ********************************************************** */

#include <processes_select.h>
#include <process_unix_priv.h>
#include <process_lib.h>

#include <string.h>
#include <sys/user.h>

#include <eval_context.h>
#include <files_names.h>
#include <conversion.h>
#include <item_lib.h>
#include <dir.h>


static time_t sys_boot_time = -1;

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
    snprintf(statfile, sizeof(statfile), "/%s/%jd/cmdline", PROCDIR, (intmax_t) pid);
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

    snprintf(statusfile, sizeof(statusfile), "/%s/%jd/status", PROCDIR, (intmax_t) pid);

    FILE *stream;
    char *line = NULL;

    stream = fopen(statusfile, "r");
    if (stream == NULL)
    {
        return false;
    }

    // look for a line:
    //   Uid: ruid euid ...
    // and extract the ruid number as the uid
    uid_t uid;
    bool found = false;
    size_t len = 0;
    ssize_t nread;
    int nscan;
    char key[CF_MAXVARSIZE];
    while ((nread = getline(&line, &len, stream)) != -1)
    {
        nscan = sscanf(line, "%s %d ", key, &uid);
        if (strcasecmp("Uid:", key) == 0 && nscan == 2)
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
static bool LoadProcTTY(JsonElement *pdata, int ttyn)
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
        snprintf(ttyname, sizeof(ttyname), "/dev/pts/%d", ttyminor);
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

/*
 * Collect all data for a given pid.  This comes from a variety of sources.
 *
 * Errors should be rare.  One case is if a process disappears mid-collection.
 * When that happens we clean up any half-collected data structures
 * and return NULL.
 */
JsonElement *LoadProcStat(pid_t pid)
{
    char statfile[CF_MAXVARSIZE];
    int fd, len;
    JsonElement *pdata;

    pdata = JsonObjectCreate(12);

    /* get uid from file 'status' */
    if (! LoadProcUid(pdata, pid))
    {
        JsonDestroy(pdata);

        return NULL;
    }

    /* extract command line info and store in 'pdata' */
    if (! LoadProcCmd(pid, pdata))
    {
        JsonDestroy(pdata);

        return NULL;
    }

    /* open the 'stat' file */
    snprintf(statfile, sizeof(statfile), "/%s/%jd/stat", PROCDIR, (intmax_t) pid);
    for (;;)
    {
        if ((fd = open(statfile, O_RDONLY)) != -1)
        {
            break;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == ENOENT || errno == ENOTDIR)
        {
            break;
        }

        if (errno == EACCES)
        {
            break;
        }

        assert (fd != -1 && "Unable to open /proc/<pid>/stat");
    }
    if (fd == -1) {
        JsonDestroy(pdata);

        return NULL;
    }

    /* read the 'stat' file into a buffer */
    char statbuf[CF_MAXVARSIZE];
    len = read(fd, statbuf, CF_MAXVARSIZE -1);
    close(fd);
    if (len <= 0) {
        JsonDestroy(pdata);

        return NULL;
    }
    statbuf[len] = '\0';

    /*
     * Big picture: "stat" is:
     *    pid (text_string_here) S n1 n2 n3 ...
     * We already know the pid. And the text is a variant of command (known).
     * We are interested in the state char 'S' and some numbers n1, n2, n3, ...
     *
     * The 'proc(5)' man page indicates 'scanf' compatibility for parsing.
     */

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
    sscanf(statbuf,
      "%*d %*s "
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

    // proc 'jiffies' since boot seconds
    starttime /= sysconf(_SC_CLK_TCK);

    // tty major/minor
    LoadProcTTY(pdata, ttyn);

    /*
     * Add data to the Json structure
     */
    JsonObjectAppendString(pdata, JPROC_KEY_PSTATE, pstate);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PPID, ppid);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PGID, pgid);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PRIORITY, priority);
    JsonObjectAppendInteger(pdata, JPROC_KEY_THREADS, threads);
    JsonObjectAppendInteger(pdata, JPROC_KEY_CPUTIME, cpusecs);
    JsonObjectAppendInteger(pdata, JPROC_KEY_STARTTIME_BOOT, starttime);
    starttime += sys_boot_time;  // => seconds since epoch
    JsonObjectAppendInteger(pdata, JPROC_KEY_STARTTIME_EPOCH, starttime);
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


/*
 * Obtain:
 *    boottime (seconds since epoch)
 *
 * This never changes during a run, so cache it in a static variable.
 */
time_t LoadBootTime(void)
{
    char statfile[CF_MAXVARSIZE];
    FILE *fd;
    char statbuf[CF_MAXVARSIZE];
    char key[64];       // see also sscanf() below

    if (sys_boot_time >= 0) {
        return sys_boot_time;
    }

    snprintf(statfile, sizeof(statfile), "/%s/stat", PROCDIR);
    fd = fopen(statfile, "r");
    if (!fd)
    {
        return false;
    }

    int nscan;
    while (fgets(statbuf, CF_MAXVARSIZE - 1, fd))
    {
        nscan = sscanf(statbuf, "%63s %lu", key, &sys_boot_time);
        if (strcmp(key, "btime") == 0  && nscan == 2) {
            break;
        }
    }

    fclose(fd);

    return sys_boot_time;
}


/*
 * Access points for individual processes.
 * Independent of complete PROCTABLE.
 * Initial use is mostly unit tests
 * although there are a couple of other uses.
 */

time_t GetProcessStartTime(pid_t pid)
{
    JsonElement *pdata;

    pdata = LoadProcStat(pid);
    if (pdata) {
        time_t t = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_STARTTIME_BOOT));

        JsonDestroy(pdata);

        return t;
    }
    else
    {
        return PROCESS_START_TIME_UNKNOWN;
    }
}

ProcessState GetProcessState(pid_t pid)
{
    JsonElement *pdata;

    pdata = LoadProcStat(pid);
    if (pdata) {
        ProcessState pstate;

        const char *status = JsonObjectGetAsString(pdata, JPROC_KEY_PSTATE);
        switch (status[0])
        {
        case 'T':
            pstate = PROCESS_STATE_STOPPED;
            break;
        case 'Z':
            pstate = PROCESS_STATE_ZOMBIE;
            break;
        default:
            pstate = PROCESS_STATE_RUNNING;
            break;
        }

        JsonDestroy(pdata);

        return pstate;
    }
    else
    {
        return PROCESS_STATE_DOES_NOT_EXIST;
    }
}
