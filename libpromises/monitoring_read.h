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


#ifndef CFENGINE_MONITORING_READ_H
#define CFENGINE_MONITORING_READ_H


#include <cf3.defs.h>


extern MonitoringSlot *SLOTS[CF_OBSERVABLES - ob_spare];


void Nova_FreeSlot(MonitoringSlot *slot);
MonitoringSlot *Nova_MakeSlot(const char *name, const char *description,
                              const char *units,
                              double expected_minimum, double expected_maximum,
                              bool consolidable);
void Nova_LoadSlots(void);
bool NovaHasSlot(int idx);
const char *NovaGetSlotName(int idx);
const char *NovaGetSlotDescription(int index);
const char *NovaGetSlotUnits(int index);
double NovaGetSlotExpectedMinimum(int index);
double NovaGetSlotExpectedMaximum(int index);
bool NovaIsSlotConsolidable(int index);
bool GetRecordForTime(CF_DB *db, time_t time, Averages *result);


/* - date-related functions - */

void MakeTimekey(time_t time, char *result);
time_t WeekBegin(time_t time);
time_t SubtractWeeks(time_t time, int weeks);
time_t NextShift(time_t time);


#endif
