/*
   Copyright 2018 Northern.tech AS

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


#include <monitoring_read.h>

#include <file_lib.h>                                     /* FILE_SEPARATOR */
#include <known_dirs.h>


/* GLOBALS */

static const char *UNITS[CF_OBSERVABLES] =                      /* constant */
{
    "average users per 2.5 mins",
    "processes",
    "processes",
    "percent",
    "jobs",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "packets",
    "entries",
    "entries",
    "entries",
    "entries",
    "Celcius",
    "Celcius",
    "Celcius",
    "Celcius",
    "percent",
    "percent",
    "percent",
    "percent",
    "percent",
    "packets",
    "packets",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",
    "connections",

};

time_t slots_load_time = 0;
MonitoringSlot *SLOTS[CF_OBSERVABLES - ob_spare];


/*****************************************************************************/

void Nova_FreeSlot(MonitoringSlot *slot)
{
    if (slot)
    {
        free(slot->name);
        free(slot->description);
        free(slot->units);
        free(slot);
    }
}

MonitoringSlot *Nova_MakeSlot(const char *name, const char *description,
                              const char *units,
                              double expected_minimum, double expected_maximum,
                              bool consolidable)
{
    MonitoringSlot *slot = xmalloc(sizeof(MonitoringSlot));

    slot->name = xstrdup(name);
    slot->description = xstrdup(description);
    slot->units = xstrdup(units);
    slot->expected_minimum = expected_minimum;
    slot->expected_maximum = expected_maximum;
    slot->consolidable = consolidable;
    return slot;
}

void Nova_LoadSlots(void)
{
    FILE *f;
    char filename[CF_BUFSIZE];
    int i;

    snprintf(filename, CF_BUFSIZE - 1, "%s%cts_key", GetStateDir(), FILE_SEPARATOR);

    struct stat sb;

    if (stat(filename, &sb) != 0)
    {
        return;
    }

    if(sb.st_mtime <= slots_load_time)
    {
        return;
    }

    slots_load_time = sb.st_mtime;

    if ((f = fopen(filename, "r")) == NULL)
    {
        return;
    }

    for (i = 0; i < CF_OBSERVABLES; ++i)
    {
        if (i < ob_spare)
        {
            fscanf(f, "%*[^\n]\n");
        }
        else
        {
            char line[CF_MAXVARSIZE];

            char name[CF_MAXVARSIZE], desc[CF_MAXVARSIZE];
            char units[CF_MAXVARSIZE] = "unknown";
            double expected_min = 0.0;
            double expected_max = 100.0;
            int consolidable = true;

            if (fgets(line, CF_MAXVARSIZE, f) == NULL)
            {
                Log(LOG_LEVEL_ERR, "Error trying to read ts_key from file '%s'. (fgets: %s)", filename, GetErrorStr());
                continue;
            }

            int fields = sscanf(line, "%*d,%1023[^,],%1023[^,],%1023[^,],%lf,%lf,%d",
                                name, desc, units, &expected_min, &expected_max, &consolidable);

            if (fields == 2)
            {
                /* Old-style ts_key with name and description */
            }
            else if (fields == 6)
            {
                /* New-style ts_key with additional parameters */
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Wrong line format in ts_key: %s", line);
            }

            if (strcmp(name, "spare") != 0)
            {
                Nova_FreeSlot(SLOTS[i - ob_spare]);
                SLOTS[i - ob_spare] = Nova_MakeSlot(name, desc, units, expected_min, expected_max, consolidable);
            }
        }
    }
    fclose(f);
}


bool NovaHasSlot(int idx)
{
    Nova_LoadSlots();

    return idx < ob_spare || SLOTS[idx - ob_spare];
}

const char *NovaGetSlotName(int idx)
{
    Nova_LoadSlots();

    return idx < ob_spare ? OBSERVABLES[idx][0] : SLOTS[idx - ob_spare]->name;
}

const char *NovaGetSlotDescription(int idx)
{
    Nova_LoadSlots();

    return idx < ob_spare ? OBSERVABLES[idx][1] : SLOTS[idx - ob_spare]->description;
}

const char *NovaGetSlotUnits(int idx)
{
    Nova_LoadSlots();

    return idx < ob_spare ? UNITS[idx] : SLOTS[idx - ob_spare]->units;
}

// TODO: real expected minimum/maximum/consolidable for core slots

double NovaGetSlotExpectedMinimum(int idx)
{
    Nova_LoadSlots();

    return idx < ob_spare ? 0.0f : SLOTS[idx - ob_spare]->expected_minimum;
}

double NovaGetSlotExpectedMaximum(int idx)
{
    Nova_LoadSlots();

    return idx < ob_spare ? 100.0f : SLOTS[idx - ob_spare]->expected_maximum;
}

bool NovaIsSlotConsolidable(int idx)
{
    Nova_LoadSlots();

    return idx < ob_spare ? true : SLOTS[idx - ob_spare]->consolidable;
}




/*
 * This function returns beginning of last Monday relative to 'time'. If 'time'
 * is Monday, beginning of the same day is returned.
 */
time_t WeekBegin(time_t time)
{
    struct tm tm;

    gmtime_r(&time, &tm);

    /* Move back in time to reach Monday. */

    time -= ((tm.tm_wday == 0 ? 6 : tm.tm_wday - 1) * SECONDS_PER_DAY);

    /* Move to the beginning of day */

    time -= tm.tm_hour * SECONDS_PER_HOUR;
    time -= tm.tm_min * SECONDS_PER_MINUTE;
    time -= tm.tm_sec;

    return time;
}

time_t SubtractWeeks(time_t time, int weeks)
{
    return time - weeks * SECONDS_PER_WEEK;
}

time_t NextShift(time_t time)
{
    return time + SECONDS_PER_SHIFT;
}


void MakeTimekey(time_t time, char *result)
{
    /* Generate timekey for database */
    struct tm tm;

    gmtime_r(&time, &tm);

    snprintf(result, 64, "%d_%.3s_Lcycle_%d_%s",
             tm.tm_mday, MONTH_TEXT[tm.tm_mon], (tm.tm_year + 1900) % 3, SHIFT_TEXT[tm.tm_hour / 6]);
}

/* Returns true if entry was found, false otherwise */
bool GetRecordForTime(CF_DB *db, time_t time, Averages *result)
{
    char timekey[CF_MAXVARSIZE];

    MakeTimekey(time, timekey);

    return ReadDB(db, timekey, result, sizeof(Averages));
}

