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


#include <cf3.defs.h>

#include <probes.h>
#include <shared_kstat.h>
#include <mon_cumulative.h>
#include <monitoring.h>                                 /* NovaRegisterSlot */

#include <sys/sysinfo.h>


#define KB 1024


/************************************************************************/

static void GetTotalMemory(double *cf_this)
{
    int total_slot = NovaRegisterSlot(MON_MEM_TOTAL, "Total system memory", "megabytes",
                                      512.0l, 4096.0l, true);

    if (total_slot != -1)
    {
        long long memory = ((long long) sysconf(_SC_PAGESIZE)) * sysconf(_SC_PHYS_PAGES);

        cf_this[total_slot] = memory / KB / KB;
    }
}

/************************************************************************/

static void GetKstatInfo(double *cf_this)
{
    kstat_ctl_t *kstat = GetKstatHandle();
    kstat_t *k;
    int free_slot = NovaRegisterSlot(MON_MEM_FREE, "Free system memory", "megabytes",
                                     0.0l, 4096.0l, true);
    vminfo_t vminfo;
    sysinfo_t sysinfo;

    if (!kstat)
    {
        return;
    }

    if ((k = kstat_lookup(kstat, "unix", 0, "sysinfo")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to lookup sysinfo in kstat (kstat_lookup)");
        return;
    }

    if (kstat_read(kstat, k, &sysinfo) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to read sysinfo from kstat (kstat_read)");
        return;
    }

    if ((k = kstat_lookup(kstat, "unix", 0, "vminfo")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to lookup vminfo in kstat (kstat_lookup)");
        return;
    }

    if (kstat_read(kstat, k, &vminfo) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to read vminfo from kstat (kstat_read)");
        return;
    }

    uint64_t freemem = GetInstantUint64Value("mem", "free", vminfo.freemem, sysinfo.updates);

    if (free_slot != -1)
    {
        if (freemem != (unsigned long long) -1)
        {
            cf_this[free_slot] = ((double) freemem) * sysconf(_SC_PAGESIZE) / KB / KB;
        }
    }

    uint64_t swap_resv = GetInstantUint64Value("swap", "resv", vminfo.swap_resv, sysinfo.updates);
    uint64_t swap_avail = GetInstantUint64Value("swap", "avail", vminfo.swap_avail, sysinfo.updates);

    if (swap_resv != (uint64_t) - 1 && swap_avail != (uint64_t) - 1)
    {
        int swap_slot = NovaRegisterSlot(MON_MEM_SWAP, "Total swap size", "megabytes",
                                         0.0l, 4096.0l, true);
        int free_swap_slot = NovaRegisterSlot(MON_MEM_FREE_SWAP, "Free swap size", "megabytes",
                                              0.0l, 4096.0l, true);

        if (swap_slot != -1)
        {
            cf_this[swap_slot] = ((double) swap_resv + swap_avail) * sysconf(_SC_PAGESIZE) / KB / KB;
        }
        if (free_swap_slot != -1)
        {
            cf_this[free_swap_slot] = ((double) swap_avail) * sysconf(_SC_PAGESIZE) / KB / KB;
        }
    }
}

/************************************************************************/

static void MonMemoryGatherData(double *cf_this)
{
    GetTotalMemory(cf_this);
    GetKstatInfo(cf_this);
}

/************************************************************************/

ProbeGatherData MonMemoryInit(const char **name, const char **error)
{
    *name = "Solaris kstat subsystem";
    *error = NULL;
    return &MonMemoryGatherData;
}
