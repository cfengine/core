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

#include "granules.h"
#include <math.h>
#include <assert.h>

static char *ConvTimeKey(char *str)
{
    int i;
    char buf1[10], buf2[10], buf3[10], buf4[10], buf5[10], buf[10], out[10];
    static char timekey[64];

    sscanf(str, "%s %s %s %s %s", buf1, buf2, buf3, buf4, buf5);

    timekey[0] = '\0';

/* Day */

    sprintf(timekey, "%s:", buf1);

/* Hours */

    sscanf(buf4, "%[^:]", buf);
    sprintf(out, "Hr%s", buf);
    strcat(timekey, out);

/* Minutes */

    sscanf(buf4, "%*[^:]:%[^:]", buf);
    sprintf(out, "Min%s", buf);
    strcat(timekey, ":");

    sscanf(buf, "%d", &i);

    switch ((i / 5))
    {
    case 0:
        strcat(timekey, "Min00_05");
        break;
    case 1:
        strcat(timekey, "Min05_10");
        break;
    case 2:
        strcat(timekey, "Min10_15");
        break;
    case 3:
        strcat(timekey, "Min15_20");
        break;
    case 4:
        strcat(timekey, "Min20_25");
        break;
    case 5:
        strcat(timekey, "Min25_30");
        break;
    case 6:
        strcat(timekey, "Min30_35");
        break;
    case 7:
        strcat(timekey, "Min35_40");
        break;
    case 8:
        strcat(timekey, "Min40_45");
        break;
    case 9:
        strcat(timekey, "Min45_50");
        break;
    case 10:
        strcat(timekey, "Min50_55");
        break;
    case 11:
        strcat(timekey, "Min55_00");
        break;
    }

    return timekey;
}

/*****************************************************************************/

char *GenTimeKey(time_t now)
{
    static char str[64];
    char timebuf[26];

    snprintf(str, sizeof(str), "%s", cf_strtimestamp_utc(now, timebuf));

    return ConvTimeKey(str);
}

int GetTimeSlot(time_t here_and_now)
{
    return ((here_and_now - (4 * 24 * 60 * 60)) % SECONDS_PER_WEEK) / (long)CF_MEASURE_INTERVAL;
}

/*****************************************************************************/

int GetShiftSlot(time_t here_and_now)
{
    time_t now = time(NULL);
    int slot = 0, chour = -1;
    char cstr[64];
    char str[64];
    char buf[10], cbuf[10];
    int hour = -1;
    char timebuf[26];

    snprintf(cstr, sizeof(str), "%s", cf_strtimestamp_utc(here_and_now, timebuf));
    sscanf(cstr, "%s %*s %*s %d", cbuf, &chour);

// Format Tue Sep 28 14:58:27 CEST 2010

    for (now = CF_MONDAY_MORNING; now < CF_MONDAY_MORNING + SECONDS_PER_WEEK; now += CF_SHIFT_INTERVAL, slot++)
    {
        snprintf(str, sizeof(str), "%s", cf_strtimestamp_utc(now, timebuf));
        sscanf(str, "%s %*s %*s %d", buf, &hour);

        if ((hour / 6 == chour / 6) && (strcmp(cbuf, buf) == 0))
        {
            return slot;
        }
    }

    return -1;
}

/*****************************************************************************/

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
