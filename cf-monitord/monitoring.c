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


#include <cf3.defs.h>

#include <known_dirs.h>
#include <eval_context.h>
#include <promises.h>
#include <probes.h>
#include <files_lib.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <vars.h>
#include <item_lib.h>
#include <conversion.h>
#include <scope.h>
#include <matching.h>
#include <instrumentation.h>
#include <pipes.h>
#include <locks.h>
#include <string_lib.h>
#include <exec_tools.h>
#include <unix.h>
#include <file_lib.h>
#include <monitoring_read.h>


void NovaNamedEvent(const char *eventname, double value)
{
    Event ev_new, ev_old;
    time_t now = time(NULL);
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_measure))
    {
        return;
    }

    ev_new.t = now;

    if (ReadDB(dbp, eventname, &ev_old, sizeof(ev_old)))
    {
        if (isnan(ev_old.Q.expect))
        {
            ev_old.Q.expect = value;
        }

        if (isnan(ev_old.Q.var))
        {
            ev_old.Q.var = 0;
        }

        ev_new.Q = QAverage(ev_old.Q, value, 0.7);
    }
    else
    {
        ev_new.Q = QDefinite(value);
    }

    Log(LOG_LEVEL_VERBOSE, "Wrote scalar named event \"%s\" = (%.2lf,%.2lf,%.2lf)", eventname, ev_new.Q.q,
          ev_new.Q.expect, sqrt(ev_new.Q.var));
    WriteDB(dbp, eventname, &ev_new, sizeof(ev_new));

    CloseDB(dbp);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/


static void Nova_DumpSlots(void)
{
#define MAX_KEY_FILE_SIZE 16384  /* usually around 4000, cannot grow much */

    char filename[CF_BUFSIZE];
    int i;

    snprintf(filename, CF_BUFSIZE - 1, "%s%cts_key", GetStateDir(), FILE_SEPARATOR);

    char file_contents_new[MAX_KEY_FILE_SIZE] = {0};

    for (i = 0; i < CF_OBSERVABLES; i++)
    {
        char line[CF_MAXVARSIZE];

        if (NovaHasSlot(i))
        {
            snprintf(line, sizeof(line), "%d,%s,%s,%s,%.3lf,%.3lf,%d\n",
                    i,
                    NULLStringToEmpty((char*)NovaGetSlotName(i)),
                    NULLStringToEmpty((char*)NovaGetSlotDescription(i)),
                    NULLStringToEmpty((char*)NovaGetSlotUnits(i)),
                    NovaGetSlotExpectedMinimum(i), NovaGetSlotExpectedMaximum(i), NovaIsSlotConsolidable(i) ? 1 : 0);
        }
        else
        {
            snprintf(line, sizeof(line), "%d,spare,unused\n", i);
        }

        strlcat(file_contents_new, line, sizeof(file_contents_new));
    }

    bool contents_changed = true;

    Writer *w = FileRead(filename, MAX_KEY_FILE_SIZE, NULL);
    if (w)
    {
        if(strcmp(StringWriterData(w), file_contents_new) == 0)
        {
            contents_changed = false;
        }
        WriterClose(w);
    }

    if(contents_changed)
    {
        Log(LOG_LEVEL_VERBOSE, "Updating %s with new slot information", filename);

        if(!FileWriteOver(filename, file_contents_new))
        {
            Log(LOG_LEVEL_ERR, "Nova_DumpSlots: Could not write file '%s'. (FileWriteOver: %s)", filename,
                GetErrorStr());
        }
    }

    chmod(filename, 0600);
}

void GetObservable(int i, char *name, char *desc)
{
    Nova_LoadSlots();

    if (i < ob_spare)
    {
        strncpy(name, OBSERVABLES[i][0], CF_MAXVARSIZE - 1);
        strncpy(desc, OBSERVABLES[i][1], CF_MAXVARSIZE - 1);
    }
    else
    {
        if (SLOTS[i - ob_spare])
        {
            strncpy(name, SLOTS[i - ob_spare]->name, CF_MAXVARSIZE - 1);
            strncpy(desc, SLOTS[i - ob_spare]->description, CF_MAXVARSIZE - 1);
        }
        else
        {
            strncpy(name, OBSERVABLES[i][0], CF_MAXVARSIZE - 1);
            strncpy(desc, OBSERVABLES[i][1], CF_MAXVARSIZE - 1);
        }
    }
}

void SetMeasurementPromises(Item ** classlist)
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    char eventname[CF_MAXVARSIZE], assignment[CF_BUFSIZE];
    Event entry;
    char *key;
    void *stored;
    int ksize, vsize;

    if (!OpenDB(&dbp, dbid_measure))
    {
        return;
    }

    if (!NewDBCursor(dbp, &dbcp))
    {
        Log(LOG_LEVEL_INFO, "Unable to scan class db");
        CloseDB(dbp);
        return;
    }

    memset(&entry, 0, sizeof(entry));

    while (NextDB(dbcp, &key, &ksize, &stored, &vsize))
    {
        if (stored != NULL)
        {
            if (sizeof(entry) < vsize)
            {
                Log(LOG_LEVEL_ERR, "Invalid entry in measurements database. Expected size: %zu, actual size: %d", sizeof(entry), vsize);
                continue;
            }

            strcpy(eventname, (char *) key);
            memcpy(&entry, stored, MIN(vsize, sizeof(entry)));

            Log(LOG_LEVEL_VERBOSE, "Setting measurement event %s", eventname);

            // a.measure.data_type is not longer known here, so look for zero decimals

            if ((int) (entry.Q.q * 10) % 10 == 0)
            {
                snprintf(assignment, CF_BUFSIZE - 1, "value_%s=%.0lf", eventname, entry.Q.q);
            }
            else
            {
                snprintf(assignment, CF_BUFSIZE - 1, "value_%s=%.2lf", eventname, entry.Q.q);
            }

            AppendItem(classlist, assignment, NULL);

            snprintf(assignment, CF_BUFSIZE - 1, "av_%s=%.2lf", eventname, entry.Q.expect);
            AppendItem(classlist, assignment, NULL);
            snprintf(assignment, CF_BUFSIZE - 1, "dev_%s=%.2lf", eventname, sqrt(entry.Q.var));
            AppendItem(classlist, assignment, NULL);
        }
    }

    DeleteDBCursor(dbcp);
    CloseDB(dbp);
}

/*****************************************************************************/
/* Clock handling                                                            */
/*****************************************************************************/

/* MB: We want to solve the geometric series problem to simulate an unbiased
   average over a grain size for long history aggregation at zero cost, i.e.
   we'd ideally like to have

  w = (1-w)^n for all n

  The true average is expensive to compute, so forget the brute force approach
  because this gives a pretty good result. The eqn above has no actual solution
  but we can approximate numerically to w = 0.01, see this test to show that
  the distribution is flat:

main ()

{ int i,j;
  double w = 0.01,wp;

for (i = 1; i < 20; i++)
   {
   printf("(");
   wp = w;

   for (j = 1; j < i; j++)
      {
      printf("%f,",wp);
      wp *= (1- w);
      }
   printf(")\n");
   }
}

*/

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static int NovaGetSlot(const char *name)
{
    int i;

    Nova_LoadSlots();

/* First try to find existing slot */
    for (i = 0; i < CF_OBSERVABLES - ob_spare; ++i)
    {
        if (SLOTS[i] && !strcmp(SLOTS[i]->name, name))
        {
            Log(LOG_LEVEL_VERBOSE, "Using slot ob_spare+%d (%d) for %s", i, i + ob_spare, name);
            return i + ob_spare;
        }
    }

/* Then find the spare one */
    for (i = 0; i < CF_OBSERVABLES - ob_spare; ++i)
    {
        if (!SLOTS[i])
        {
            Log(LOG_LEVEL_VERBOSE, "Using empty slot ob_spare+%d (%d) for %s", i, i + ob_spare, name);
            return i + ob_spare;
        }
    }

    Log(LOG_LEVEL_ERR,
          "Measurement slots are all in use - it is not helpful to measure too much, you can't usefully follow this many variables");

    return -1;
}

/*****************************************************************************/

int NovaRegisterSlot(const char *name, const char *description,
                     const char *units, double expected_minimum,
                     double expected_maximum, bool consolidable)
{
    int slot = NovaGetSlot(name);

    if (slot == -1)
    {
        return -1;
    }

    Nova_FreeSlot(SLOTS[slot - ob_spare]);
    SLOTS[slot - ob_spare] = Nova_MakeSlot(name, description, units, expected_minimum, expected_maximum, consolidable);
    Nova_DumpSlots();

    return slot;
}
