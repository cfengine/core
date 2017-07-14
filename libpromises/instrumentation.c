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

#include <instrumentation.h>

#include <dbm_api.h>
#include <files_names.h>
#include <item_lib.h>
#include <string_lib.h>
#include <policy.h>

#include <math.h>

static void NotePerformance(char *eventname, time_t t, double value);

/* Alter this code at your peril. Berkeley DB is very sensitive to errors. */

bool TIMING = false;

/***************************************************************/

struct timespec BeginMeasure()
{
    struct timespec start = { 0 };

    if (clock_gettime(CLOCK_REALTIME, &start) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Clock gettime failure. (clock_gettime: %s)", GetErrorStr());
    }
    else if (TIMING)
    {
        Log(LOG_LEVEL_VERBOSE, "T: Starting measuring time");
    }

    return start;
}

/***************************************************************/

void EndMeasurePromise(struct timespec start, const Promise *pp)
{
    char id[CF_BUFSIZE], *mid = NULL;

    if (TIMING)
    {
        Log(LOG_LEVEL_VERBOSE, "\n");
        Log(LOG_LEVEL_VERBOSE, "T: .........................................................");
        Log(LOG_LEVEL_VERBOSE, "T: Promise timing summary for %s", pp->promiser);
    }

    mid = PromiseGetConstraintAsRval(pp, "measurement_class", RVAL_TYPE_SCALAR);

    if (mid)
    {
        snprintf(id, CF_BUFSIZE, "%s:%s:%.100s", mid, pp->parent_promise_type->name, pp->promiser);
        Chop(id, CF_EXPANDSIZE);
        EndMeasure(id, start);
    }
    else
    {
        if (TIMING)
        {
            Log(LOG_LEVEL_VERBOSE, "T: No measurement_class attribute set in action body");
        }
        EndMeasure(NULL, start);
    }

    if (TIMING)
    {
        Log(LOG_LEVEL_VERBOSE, "T: .........................................................");
    }
}

/***************************************************************/

void EndMeasure(char *eventname, struct timespec start)
{
    struct timespec stop;

    if (clock_gettime(CLOCK_REALTIME, &stop) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Clock gettime failure. (clock_gettime: %s)", GetErrorStr());
    }
    else
    {
        double dt = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (double) CF_BILLION;

        if (eventname)
        {
            NotePerformance(eventname, start.tv_sec, dt);
        }
        else if (TIMING)
        {
            Log(LOG_LEVEL_VERBOSE, "T: This execution measured %lf seconds (use measurement_class to track)", dt);
        }
    }
}

/***************************************************************/

int EndMeasureValueMs(struct timespec start)
{
    struct timespec stop;

    if (clock_gettime(CLOCK_REALTIME, &stop) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Clock gettime failure. (clock_gettime: %s)", GetErrorStr());
    }
    else
    {
        double dt = ((stop.tv_sec - start.tv_sec) * 1e3 + /* 1 s = 1e3 ms */
                     (stop.tv_nsec - start.tv_nsec) / 1e6); /* 1e6 ns = 1 ms */
        return (int)dt;
    }

    return -1;
}

/***************************************************************/

static void NotePerformance(char *eventname, time_t t, double value)
{
    CF_DB *dbp;
    Event e, newe;
    double lastseen;
    int lsea = SECONDS_PER_WEEK;
    time_t now = time(NULL);

    if (!OpenDB(&dbp, dbid_performance))
    {
        return;
    }

    if (ReadDB(dbp, eventname, &e, sizeof(e)))
    {
        lastseen = now - e.t;
        newe.t = t;

        newe.Q = QAverage(e.Q, value, 0.3);

        /* Have to kickstart variance computation, assume 1% to start  */

        if (newe.Q.var <= 0.0009)
        {
            newe.Q.var = newe.Q.expect / 100.0;
        }
    }
    else
    {
        lastseen = 0.0;
        newe.t = t;
        newe.Q.q = value;
        newe.Q.dq = 0;
        newe.Q.expect = value;
        newe.Q.var = 0.001;
    }

    if (lastseen > (double) lsea)
    {
        Log(LOG_LEVEL_DEBUG, "Performance record '%s' expired", eventname);
        DeleteDB(dbp, eventname);
    }
    else
    {
        WriteDB(dbp, eventname, &newe, sizeof(newe));

        if (TIMING)
        {
            Log(LOG_LEVEL_VERBOSE, "T: This measurement event, alias '%s', measured at time %s\n", eventname, ctime(&newe.t));
            Log(LOG_LEVEL_VERBOSE, "T:   Last measured %lf seconds ago\n", lastseen);
            Log(LOG_LEVEL_VERBOSE, "T:   This execution measured %lf seconds\n", newe.Q.q);
            Log(LOG_LEVEL_VERBOSE, "T:   Average execution time %lf +/- %lf seconds\n", newe.Q.expect, sqrt(newe.Q.var));
        }
    }

    CloseDB(dbp);
}
