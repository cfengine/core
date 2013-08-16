/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
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

/***************************************************************/

struct timespec BeginMeasure()
{
    struct timespec start;

    if (clock_gettime(CLOCK_REALTIME, &start) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Clock gettime failure. (clock_gettime: %s)", GetErrorStr());
    }

    return start;
}

/***************************************************************/

void EndMeasurePromise(EvalContext *ctx, struct timespec start, Promise *pp)
{
    char id[CF_BUFSIZE], *mid = NULL;

    mid = ConstraintGetRvalValue(ctx, "measurement_class", pp, RVAL_TYPE_SCALAR);

    if (mid)
    {
        snprintf(id, CF_BUFSIZE, "%s:%s:%.100s", (char *) mid, pp->parent_promise_type->name, pp->promiser);
        if (Chop(id, CF_EXPANDSIZE) == -1)
        {
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }
        EndMeasure(id, start);
    }
}

/***************************************************************/

void EndMeasure(char *eventname, struct timespec start)
{
    struct timespec stop;
    int measured_ok = true;
    double dt;

    if (clock_gettime(CLOCK_REALTIME, &stop) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Clock gettime failure. (clock_gettime: %s)", GetErrorStr());
        measured_ok = false;
    }

    dt = (double) (stop.tv_sec - start.tv_sec) + (double) (stop.tv_nsec - start.tv_nsec) / (double) CF_BILLION;

    if (measured_ok)
    {
        NotePerformance(eventname, start.tv_sec, dt);
    }
}

/***************************************************************/

int EndMeasureValueMs(struct timespec start)
{
    struct timespec stop;
    int measured_ok = true;
    double dt;

    if (clock_gettime(CLOCK_REALTIME, &stop) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Clock gettime failure. (clock_gettime: %s)", GetErrorStr());
        measured_ok = false;
    }

    dt = (double) (stop.tv_sec - start.tv_sec) + (double) (stop.tv_nsec - start.tv_nsec) / (double) CF_BILLION;

    if (measured_ok)
    {
        return (int)(dt * 1000); // to [ms]
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
    }

    CloseDB(dbp);
}

/***************************************************************/

static bool IsContextIgnorableForReporting(const char *context_name)
{
    return (strncmp(context_name,"Min",3) == 0 || strncmp(context_name,"Hr",2) == 0 || strcmp(context_name,"Q1") == 0
     || strcmp(context_name,"Q2") == 0 || strcmp(context_name,"Q3") == 0 || strcmp(context_name,"Q4") == 0
     || strncmp(context_name,"GMT_Hr",6) == 0  || strncmp(context_name,"Yr",2) == 0
     || strncmp(context_name,"Day",3) == 0 || strcmp(context_name,"license_expired") == 0
     || strcmp(context_name,"any") == 0 || strcmp(context_name,"from_cfexecd") == 0
     || IsStrIn(context_name,MONTH_TEXT) || IsStrIn(context_name,DAY_TEXT)
     || IsStrIn(context_name,SHIFT_TEXT)) || strncmp(context_name,"Lcycle",6) == 0;
}


void NoteClassUsage(StringSetIterator context_iterator, int purge)
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    void *stored;
    char *key;
    int ksize, vsize;
    Event e, entry, newe;
    double lsea = SECONDS_PER_WEEK * 52;        /* expire after (about) a year */
    time_t now = time(NULL);
    Item *list = NULL;
    const Item *ip;
    double lastseen;
    double vtrue = 1.0;         /* end with a rough probability */

    /* Only do this for the default policy, too much "downgrading" otherwise */
    if (MINUSF)
    {
        return;
    }

    // TODO: stupid, please simplify
    {
        char *context = NULL;
        while ((context = StringSetIteratorNext(&context_iterator)))
        {
            if ((IsContextIgnorableForReporting(context)))
            {
                Log(LOG_LEVEL_DEBUG, "Ignoring class '%s' (not packing)", context);
                continue;
            }

            IdempPrependItem(&list, context, NULL);
        }
    }

    if (!OpenDB(&dbp, dbid_classes))
    {
        return;
    }

/* First record the classes that are in use */

    for (ip = list; ip != NULL; ip = ip->next)
    {
        if (ReadDB(dbp, ip->name, &e, sizeof(e)))
        {
            Log(LOG_LEVEL_DEBUG, "Found '%s' with %lf", ip->name, e.Q.expect);
            lastseen = now - e.t;
            newe.t = now;

            newe.Q = QAverage(e.Q, vtrue, 0.7);
        }
        else
        {
            lastseen = 0.0;
            newe.t = now;
            /* With no data it's 50/50 what we can say */
            newe.Q = QDefinite(0.5 * vtrue);
        }

        if (lastseen > lsea)
        {
            Log(LOG_LEVEL_DEBUG, "Class usage record '%s' expired", ip->name);
            DeleteDB(dbp, ip->name);
        }
        else
        {
            WriteDB(dbp, ip->name, &newe, sizeof(newe));
        }
    }

    CloseDB(dbp);

    if (!OpenDB(&dbp, dbid_classes))
    {
        return;
    }

/* Then update with zero the ones we know about that are not active */

    if (purge)
    {
/* Acquire a cursor for the database and downgrade classes that did not
 get defined this time*/

        if (!NewDBCursor(dbp, &dbcp))
        {
            Log(LOG_LEVEL_INFO, "Unable to scan class db");
            CloseDB(dbp);
            DeleteItemList(list);
            return;
        }

        memset(&entry, 0, sizeof(entry));

        while (NextDB(dbcp, &key, &ksize, &stored, &vsize))
        {
            time_t then;
            char eventname[CF_BUFSIZE];

            memset(eventname, 0, CF_BUFSIZE);
            strncpy(eventname, (char *) key, ksize);

            if (stored != NULL)
            {
                memcpy(&entry, stored, sizeof(entry));

                then = entry.t;
                lastseen = now - then;

                if (lastseen > lsea)
                {
                    Log(LOG_LEVEL_DEBUG, "Class usage record '%s' expired", eventname);
                    DBCursorDeleteEntry(dbcp);
                }
                else if (!IsItemIn(list, eventname))
                {
                    newe.t = then;

                    newe.Q = QAverage(entry.Q, 0, 0.5);

                    if (newe.Q.expect <= 0.0001)
                    {
                        Log(LOG_LEVEL_DEBUG, "Deleting class '%s' as %lf is zero", eventname, newe.Q.expect);
                        DBCursorDeleteEntry(dbcp);
                    }
                    else
                    {
                        Log(LOG_LEVEL_DEBUG, "Downgrading class '%s' from %lf to %lf", eventname, entry.Q.expect, newe.Q.expect);
                        DBCursorWriteEntry(dbcp, &newe, sizeof(newe));
                    }
                }
            }
        }

        DeleteDBCursor(dbcp);
    }

    CloseDB(dbp);
    DeleteItemList(list);
}

