/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_MON_H
#define CFENGINE_MON_H


/* mon_entropy.c */

void MonEntropyClassesInit(void);
void MonEntropyClassesReset(void);
void MonEntropyClassesSet(const char *service, const char *direction, double entropy);
void MonEntropyClassesPublish(FILE *fp);
void MonEntropyPurgeUnused(char *name);
double MonEntropyCalculate(const Item *items);

/* mon_cpu.c */

void MonCPUGatherData(double *cf_this);

/* mon_disk.c */

void MonDiskGatherData(double *cf_this);

/* mon_load.c */

void MonLoadGatherData(double *cf_this);

/* mon_network.c */

void MonNetworkInit(void);
void MonNetworkGatherData(double *cf_this);

/* mon_network_sniffer.c */

void MonNetworkSnifferInit(void);
void MonNetworkSnifferOpen(void);
void MonNetworkSnifferEnable(bool enable);
void MonNetworkSnifferSniff(Item *ip_addresses, long iteration, double *cf_this);
void MonNetworkSnifferGatherData(void);

/* mon_processes.c */

void MonProcessesGatherData(double *cf_this);

/* mon_temp.c */

void MonTempInit(void);
void MonTempGatherData(double *cf_this);

#endif
