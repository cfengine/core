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

#ifndef CFENGINE_PROBES_H
#define CFENGINE_PROBES_H


#include <cf3.defs.h>


void MonOtherInit();
void MonOtherGatherData(double *cf_this);


/*
 * Type of callback collecting actual probe data.
 */
typedef void (*ProbeGatherData) (double *cf_this);

/*
 * Type of probe initialization function.
 *
 * Probe initialization function should either return callback and name of probe
 * provider in "name" argument, or NULL and error description in "error"
 * argument.
 *
 * Caller does not free data returned in "name" or "error".
 */
typedef ProbeGatherData(*ProbeInit) (const char **name, const char **error);

/*
 * Existing probes and their identifiers
 */

#define MON_IO_READS "io_reads"
#define MON_IO_WRITES "io_writes"
#define MON_IO_READDATA "io_readdata"
#define MON_IO_WRITTENDATA "io_writtendata"

ProbeGatherData MonIoInit(const char **name, const char **error);

#define MON_MEM_TOTAL "mem_total"
#define MON_MEM_FREE "mem_free"
#define MON_MEM_CACHED "mem_cached"
#define MON_MEM_SWAP "mem_swap"
#define MON_MEM_FREE_SWAP "mem_freeswap"

ProbeGatherData MonMemoryInit(const char **name, const char **error);


#endif
