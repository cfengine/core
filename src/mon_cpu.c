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
#include "cf3.extern.h"
#include "monitoring.h"

#if !defined(MINGW)

/* Constants */

#define MON_CPU_MAX 4

/* Globals */

static double LAST_CPU_Q[MON_CPU_MAX];
static long LAST_CPU_T[MON_CPU_MAX];

/* Implementation */

void MonCPUGatherData(double *cf_this)
{
double q,dq;
char name[CF_MAXVARSIZE],cpuname[CF_MAXVARSIZE],buf[CF_BUFSIZE];
long count,userticks=0,niceticks=0,systemticks=0,idle=0,iowait=0,irq=0,softirq=0;
long total_time = 1;
FILE *fp;
enum observables index = ob_spare;

if ((fp=fopen("/proc/stat","r")) == NULL)
   {
   CfOut(cf_verbose,"","Didn't find proc data\n");
   return;
   }

CfOut(cf_verbose,"","Reading /proc/stat utilization data -------\n");

count = 0;

while (!feof(fp))
   {
   fgets(buf,CF_BUFSIZE,fp);

   sscanf(buf,"%s%ld%ld%ld%ld%ld%ld%ld",cpuname,&userticks,&niceticks,&systemticks,&idle,&iowait,&irq,&softirq);
   snprintf(name,16,"cpu%ld",count);

   total_time = (userticks+niceticks+systemticks+idle); 

   q = 100.0 * (double)(total_time - idle);

   if (strncmp(cpuname,name,strlen(name)) == 0)
      {
      CfOut(cf_verbose,"","Found CPU %d\n",count);

      switch (count++)
         {
         case 0: index = ob_cpu0;
             break;
         case 1: index = ob_cpu1;
             break;
         case 2: index = ob_cpu2;
             break;
         case 3: index = ob_cpu3;
             break;
         default:
             index = ob_spare;
             CfOut(cf_verbose,"","Error reading proc/stat\n");
             continue;
         }
      }
   else if (strncmp(cpuname,"cpu",3) == 0)
      {
      CfOut(cf_verbose,"","Found aggregate CPU\n",count);
      index = ob_cpuall;
      }
   else
      {
      CfOut(cf_verbose,"","Found nothing (%s)\n",cpuname);
      index = ob_spare;
      fclose(fp);
      return;
      }

   dq = (q - LAST_CPU_Q[count])/(double)(total_time-LAST_CPU_T[count]); /* % Utilization */

   if (dq > 100 || dq < 0) // Counter wrap around
      {
      dq = 50;
      }

   cf_this[index] = dq;
   LAST_CPU_Q[count] = q;

   CfOut(cf_verbose,"","Set %s=%d to %.1lf after %ld 100ths of a second \n",OBS[index][1],index,cf_this[index],total_time);
   }

LAST_CPU_T[count] = total_time;
fclose(fp);
}

#endif /* !MINGW */
