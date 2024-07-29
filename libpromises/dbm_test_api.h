/*
  Copyright 2024 Northern.tech AS

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

#ifndef __ANDROID__
#ifndef CFENGINE_DBM_TEST_API_H
#define CFENGINE_DBM_TEST_API_H

#include <dbm_api.h>
#include <set.h>

void DoRandomReads(dbid db_id,
                   int keys_refresh_s, long min_interval_ms, long max_interval_ms,
                   bool *terminate);

void DoRandomWrites(dbid db_id,
                    int sample_size_pct,
                    int prune_interval_s, long min_interval_ms, long max_interval_ms,
                    bool *terminate);

void DoRandomIterations(dbid db_id,
                        long min_interval_ms, long max_interval_ms,
                        bool *terminate);

typedef struct DBLoadSimulation_ DBLoadSimulation;
DBLoadSimulation *SimulateDBLoad(dbid db_id,
                                 int read_keys_refresh_s, long read_min_interval_ms, long read_max_interval_ms,
                                 int write_sample_size_pct,
                                 int write_prune_interval_s, long write_min_interval_ms, long write_max_interval_ms,
                                 long iter_min_interval_ms, long iter_max_interval_ms);

void StopSimulation(DBLoadSimulation *simulation);

typedef struct DBFilament_ DBFilament;
DBFilament *FillUpDB(dbid db_id, int usage_pct);
void RemoveFilament(DBFilament *filament);

#endif  /* CFENGINE_DBM_TEST_API_H */
#endif /* not __ANDROID__ */
