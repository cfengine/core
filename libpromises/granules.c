/*
   Copyright 2017 Northern.tech AS

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

#include <granules.h>
#include <misc_lib.h>


char *GenTimeKey(time_t now)
{
    struct tm tm;
    static char buf[18]; /* GLOBAL_R, no initialization needed */

    gmtime_r(&now, &tm);

    xsnprintf(buf, sizeof(buf), "%3.3s:Hr%02d:Min%02d_%02d",
             DAY_TEXT[tm.tm_wday ? (tm.tm_wday - 1) : 6],
             tm.tm_hour, tm.tm_min / 5 * 5, ((tm.tm_min + 5) / 5 * 5) % 60);

    return buf;
}

int GetTimeSlot(time_t here_and_now)
{
    return ((here_and_now - (4 * 24 * 60 * 60)) % SECONDS_PER_WEEK) / (long)CF_MEASURE_INTERVAL;
}

int GetShiftSlot(time_t t)
{
    return UnsignedModulus((t - CF_MONDAY_MORNING), SECONDS_PER_WEEK) / CF_SHIFT_INTERVAL;
}

time_t GetShiftSlotStart(time_t t)
{
    return (t - (t % SECONDS_PER_SHIFT));
}

time_t MeasurementSlotStart(time_t t)
{
    return (t - t % (time_t)CF_MEASURE_INTERVAL);
}

time_t MeasurementSlotTime(size_t slot, size_t num_slots, time_t now)
{
    assert(slot <= num_slots);

    size_t start_slot = GetTimeSlot(now);
    size_t distance = 0;
    if (slot <= start_slot)
    {
        distance = start_slot - slot;
    }
    else
    {
        distance = start_slot + (num_slots - slot - 1);
    }

    time_t start_time = MeasurementSlotStart(now);
    return start_time - (distance * CF_MEASURE_INTERVAL);
}
