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

#ifdef HAVE_SYS_LOADAVG_H
# include <sys/loadavg.h>
#else
# define LOADAVG_5MIN    1
#endif

/* Implementation */

#ifdef HAVE_GETLOADAVG

void MonLoadGatherData(double *cf_this)
{
    double load[LOADAVG_5MIN], sum = 0.0;
    int i, n;

    CfDebug("GatherLoadData\n\n");

    if ((n = getloadavg(load, LOADAVG_5MIN)) == -1)
    {
        cf_this[ob_loadavg] = 0.0;
    }

    for (i = 0; i < n; ++i)
    {
        sum += load[i];
    }

    sum /= (double) n;

/* Scale load average by 100 to make it visible */

    cf_this[ob_loadavg] = sum;
    CfOut(cf_verbose, "", "Load Average = %.2lf\n", cf_this[ob_loadavg]);
}

#else

void MonLoadGatherData(double *cf_this)
{
    CfDebug("Average load data is not available.\n");
}

#endif
