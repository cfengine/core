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
 *
 * TODO:  Lots....
 *
 *  o  Stuff commented out (from Linux import), e.g. RSS, SIZE, ...
 *  o  Check and re-check numeric calculations (e.g. CPU seconds)
 *  o  Do fudged items properly, e.g. CMDLINE
 ********************************************************** */

/* **********************************************************
 * Create a JSON representation of processes.
 *
 * Whereas many modern UNIX-like systems offer "/proc"
 * the BSD family (at least "FreeBSD") offers "kvm_getprocs(3)".
 *
 * (David Lee, 2019)
 ********************************************************** */

/* As per the FreeBSD manual page */
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
/* As per 'ps' source code */
#include <paths.h>

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

/*
 * For OpenProcDir(), etc. this is the opaque data structure
 * holding our state information.
 */
typedef struct {
    kvm_t *kd;
    const struct kinfo_proc *procs;
    int nprocs;
    int index;
} pd_state_handle_t;


// static time_t sys_boot_time = -1;


/*
 * Set JPROC_KEY_CMDLINE
 */
//     TODO: Fudge to be same as CMD
static bool LoadProcCmdLine(JsonElement *pdata, const struct kinfo_proc *kp)
{
//     const struct pargs *args = kp->ki_args;

    JsonObjectAppendString(pdata, JPROC_KEY_CMDLINE, kp->ki_comm);
    return true;
}

/*
 * Supply JPROC_KEY_TTY and JPROC_KEY_TTYNAME
 *
 * Adpated from 'ps' source code.
 */
static bool LoadProcTTY(JsonElement *pdata, dev_t dev)
{
    char *ttname;
    char str[64];

    JsonObjectAppendInteger(pdata, JPROC_KEY_TTY, dev);

    // TODO: Try to form a name then set JPROC_KEY_TTYNAME
    if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
    {
        str[0] = '-';
        str[1] = '\0';
    }
    else {
        sprintf(str, "%s", ttname);
    }
    JsonObjectAppendString(pdata, JPROC_KEY_TTYNAME, str);

    return true;
}

static JsonElement *ReadProcInfo(const struct kinfo_proc *kp)
{
    JsonElement *pdata;
    char pstate[2];
    long long starttime;
    long cpusecs;

    pdata = JsonObjectCreate(12);

    JsonObjectAppendInteger(pdata, JPROC_KEY_PID, kp->ki_pid);

    JsonObjectAppendInteger(pdata, JPROC_KEY_UID, kp->ki_ruid);
    JsonObjectAppendInteger(pdata, JPROC_KEY_EUID, kp->ki_uid);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PPID, kp->ki_ppid);
    JsonObjectAppendInteger(pdata, JPROC_KEY_PGID, kp->ki_pgid);

    // convert ki_stat integer to a letter as per Linux
    // working note: this should be reviewed
    switch (kp->ki_stat) {
    case SRUN:    /* Currently runnable. */
    case SIDL:    /* Process being created by fork. */
        pstate[0] = 'R';
        break;
    case SSTOP:   /* Process debugging or suspension. */
        pstate[0] = 'T';
        break;
    case SSLEEP:  /* Sleeping on an address. */
        pstate[0] = 'S';
        break;
    case SZOMB:   /* Awaiting collection by parent. */
        pstate[0] = 'Z';
        break;
    case SWAIT:   /* Waiting for interrupt. */
    default:      /* (...and shouldn't happen) */
        pstate[0] = 'X';
        break;
    }
    pstate[1] = '\0';
    JsonObjectAppendString(pdata, JPROC_KEY_PSTATE, pstate);

    JsonObjectAppendString(pdata, JPROC_KEY_CMD, kp->ki_comm);

    LoadProcCmdLine(pdata, kp);

    starttime = kp->ki_start.tv_sec;
    JsonObjectAppendInteger(pdata, JPROC_KEY_STARTTIME_EPOCH, starttime);
    starttime -= LoadBootTime();  // => seconds since epoch
    JsonObjectAppendInteger(pdata, JPROC_KEY_STARTTIME_BOOT, starttime);

//     /*
//      * Transform numbers/units from OS into CFEngine-preferred number/units.
//      */
//     // proc bytes => CFEngine kilobytes
//     vsize /= 1024;
//     JsonObjectAppendInteger(pdata, JPROC_KEY_VIRT_KB, vsize);
//
//     // proc PAGE SIZE => CFEngine kilobytes
//     rss *= (PAGE_SIZE/1024);
//     JsonObjectAppendInteger(pdata, JPROC_KEY_RES_KB, rss);
//
    // CFEngine CPU seconds
    cpusecs = kp->ki_rusage.ru_utime.tv_sec + kp->ki_rusage.ru_stime.tv_sec;
    JsonObjectAppendInteger(pdata, JPROC_KEY_CPUTIME, cpusecs);

//     JsonObjectAppendInteger(pdata, JPROC_KEY_PRIORITY, priority);
    JsonObjectAppendInteger(pdata, JPROC_KEY_THREADS, kp->ki_numthreads);

    LoadProcTTY(pdata, kp->ki_tdev);

Log(LOG_LEVEL_VERBOSE, "%s() => pid:%d euid:%d state:%s stime:%lld comm:%s", __func__, kp->ki_pid, kp->ki_uid, pstate, starttime, kp->ki_comm);

    return pdata;
}


void *OpenProcDir()
{
    kvm_t *kd;
    char errmsg[_POSIX2_LINE_MAX];
    const struct kinfo_proc *procs;
    int nprocs;

    if ((kd = kvm_openfiles(NULL, _PATH_DEVNULL, NULL, O_RDONLY, errmsg)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: error opening kernel virtual memory", __func__);
        return NULL;
    }

    procs = kvm_getprocs(kd, KERN_PROC_ALL, 0, &nprocs);
    if (procs == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: error getting process list", __func__);

        kvm_close(kd);

        return NULL;
    }

    pd_state_handle_t *pdh = malloc(sizeof(pd_state_handle_t));
    pdh->kd = kd;
    pdh->procs = procs;
    pdh->nprocs = nprocs;
    pdh->index = 0;

    return pdh;
}

JsonElement *ReadProcDir(void *opaque)
{
    pd_state_handle_t *pdh;
    JsonElement *pdata;

    if (opaque == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: handle was NULL", __func__);
        return NULL;
    }

    pdh = (pd_state_handle_t *) opaque;

    if (pdh->index >= pdh->nprocs)
    {
        return NULL;
    }

    pdata = ReadProcInfo(&(pdh->procs[pdh->index]));

    pdh->index++;

    return pdata;
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
    kvm_t *kd;
    char errmsg[_POSIX2_LINE_MAX];
    struct kinfo_proc *procs, *kp;
    int nprocs;

    JsonElement *pdata;

    if ((kd = kvm_openfiles(NULL, _PATH_DEVNULL, NULL, O_RDONLY, errmsg)) == NULL) {
        Log(LOG_LEVEL_ERR, "%s: error opening kernel virtual memory", __func__);
        return NULL;
    }

    procs = kvm_getprocs(kd, KERN_PROC_PID, pid, &nprocs);
    if (procs == NULL || nprocs != 1) {
        Log(LOG_LEVEL_ERR, "%s: error getting process information for %d", __func__, pid);
        return NULL;
    }

    kp = &procs[0];

    pdata = ReadProcInfo(kp);

    kvm_close(kd);

    return pdata;
}

void CloseProcDir(void *opaque)
{
    if (opaque == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: handle was NULL", __func__);
        return;
    }

    kvm_t *kd = ((pd_state_handle_t *) opaque)->kd;

    kvm_close(kd);

    free(opaque);
}


/*
 * Obtain:
 *    boottime (seconds since epoch)
 *
 * This never changes during a run, so cache it in a static variable.
 */
time_t LoadBootTime(void)
{
//     char statfile[CF_MAXVARSIZE];
//     FILE *fd;
//     char statbuf[CF_MAXVARSIZE];
//     char key[64];       // see also sscanf() below
//
//     if (sys_boot_time >= 0) {
//         return sys_boot_time;
//     }
//
//     snprintf(statfile, sizeof(statfile), "/%s/stat", PROCDIR);
//     fd = fopen(statfile, "r");
//     if (!fd)
//     {
//         Log(LOG_LEVEL_ERR, "error opening %s for boot time", statfile);
//
//         return sys_boot_time;
//     }
//
//     int nscan;
//     while (fgets(statbuf, CF_MAXVARSIZE - 1, fd))
//     {
//         nscan = sscanf(statbuf, "%63s %lu", key, &sys_boot_time);
//         if (strcmp(key, "btime") == 0  && nscan == 2) {
//             break;
//         }
//     }
//
//     fclose(fd);
//
//     return sys_boot_time;
    return -1;
}
