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

#include "monitoring.h"
#include "cfstream.h"

#if !defined(MINGW)

/* Constants */

# define MON_CPU_MAX 4

/* Globals */

static double LAST_CPU_Q[MON_CPU_MAX + 1];
static long LAST_CPU_T[MON_CPU_MAX + 1];

/* Implementation */

void MonCPUGatherData(double *cf_this)
{
    double q, dq;
    char cpuname[CF_MAXVARSIZE], buf[CF_BUFSIZE];
    long cpuidx, userticks = 0, niceticks = 0, systemticks = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
    long total_time = 1;
    FILE *fp;
    enum observables slot = ob_spare;

    if ((fp = fopen("/proc/stat", "r")) == NULL)
    {
        CfOut(cf_verbose, "", "Didn't find proc data\n");
        return;
    }

    CfOut(cf_verbose, "", "Reading /proc/stat utilization data -------\n");

    while (!feof(fp))
    {
        fgets(buf, CF_BUFSIZE, fp);

        sscanf(buf, "%s%ld%ld%ld%ld%ld%ld%ld", cpuname, &userticks, &niceticks, &systemticks, &idle, &iowait, &irq,
               &softirq);

        total_time = (userticks + niceticks + systemticks + idle);

        q = 100.0 * (double) (total_time - idle);

        if (strcmp(cpuname, "cpu") == 0)
        {
            CfOut(cf_verbose, "", "Found aggregate CPU\n");
            slot = ob_cpuall;
            cpuidx = MON_CPU_MAX;
        }
        else if (strncmp(cpuname, "cpu", 3) == 0)
        {
            if (sscanf(cpuname, "cpu%ld", &cpuidx) == 1)
            {
                if ((cpuidx < 0) || (cpuidx >= MON_CPU_MAX))
                {
                    continue;
                }
            }
            slot = ob_cpu0 + cpuidx;
        }
        else
        {
            CfOut(cf_verbose, "", "Found nothing (%s)\n", cpuname);
            slot = ob_spare;
            fclose(fp);
            return;
        }

        dq = (q - LAST_CPU_Q[cpuidx]) / (double) (total_time - LAST_CPU_T[cpuidx]);     /* % Utilization */

        if ((dq > 100) || (dq < 0)) // Counter wrap around
        {
            dq = 50;
        }

        cf_this[slot] = dq;
        LAST_CPU_Q[cpuidx] = q;
        LAST_CPU_T[cpuidx] = total_time;

        CfOut(cf_verbose, "", "Set %s=%d to %.1lf after %ld 100ths of a second \n", OBS[slot][1], slot, cf_this[slot],
              total_time);
    }

    fclose(fp);
}

#endif /* !MINGW */
