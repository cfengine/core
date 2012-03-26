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

/*****************************************************************************/
/*                                                                           */
/* File: instrumentation.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include <math.h>
#include "dbm_api.h"

static void NotePerformance(char *eventname, time_t t, double value);
static void UpdateLastSawHost(char *rkey, char *ipaddress);
static void PurgeMultipleIPReferences(CF_DB *dbp, char *rkey, char *ipaddress);

/* Alter this code at your peril. Berkeley DB is very sensitive to errors. */

/***************************************************************/

struct timespec BeginMeasure()
{
    struct timespec start;

    if (clock_gettime(CLOCK_REALTIME, &start) == -1)
    {
        CfOut(cf_verbose, "clock_gettime", "Clock gettime failure");
    }

    return start;
}

/***************************************************************/

void EndMeasurePromise(struct timespec start, Promise *pp)
{
    char id[CF_BUFSIZE], *mid = NULL;

    mid = GetConstraintValue("measurement_class", pp, CF_SCALAR);

    if (mid)
    {
        snprintf(id, CF_BUFSIZE, "%s:%s:%.100s", (char *) mid, pp->agentsubtype, pp->promiser);
        Chop(id);
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
        CfOut(cf_verbose, "clock_gettime", "Clock gettime failure");
        measured_ok = false;
    }

    dt = (double) (stop.tv_sec - start.tv_sec) + (double) (stop.tv_nsec - start.tv_nsec) / (double) CF_BILLION;

    if (measured_ok)
    {
        NotePerformance(eventname, start.tv_sec, dt);
    }
}

/***************************************************************/

static void NotePerformance(char *eventname, time_t t, double value)
{
    CF_DB *dbp;
    Event e, newe;
    double lastseen;
    int lsea = SECONDS_PER_WEEK;
    time_t now = time(NULL);

    CfDebug("PerformanceEvent(%s,%.1f s)\n", eventname, value);

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
        CfDebug("Performance record %s expired\n", eventname);
        DeleteDB(dbp, eventname);
    }
    else
    {
        CfOut(cf_verbose, "", "Performance(%s): time=%.4lf secs, av=%.4lf +/- %.4lf\n", eventname, value, newe.Q.expect,
              sqrt(newe.Q.var));
        WriteDB(dbp, eventname, &newe, sizeof(newe));
    }

    CloseDB(dbp);
}

/***************************************************************/

void NoteClassUsage(AlphaList baselist, int purge)
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

    AlphaListIterator it = AlphaListIteratorInit(&baselist);

    for (ip = AlphaListIteratorNext(&it); ip != NULL; ip = AlphaListIteratorNext(&it))
    {
        if (IGNORECLASS(ip->name))
        {
            CfDebug("Ignoring class %s (not packing)", ip->name);
            continue;
        }

        IdempPrependItem(&list, ip->name, NULL);
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
            CfDebug("FOUND %s with %lf\n", ip->name, e.Q.expect);
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
            CfDebug("Class usage record %s expired\n", ip->name);
            DeleteDB(dbp, ip->name);
        }
        else
        {
            WriteDB(dbp, ip->name, &newe, sizeof(newe));
        }
    }

/* Then update with zero the ones we know about that are not active */

    if (purge)
    {
/* Acquire a cursor for the database and downgrade classes that did not
 get defined this time*/

        if (!NewDBCursor(dbp, &dbcp))
        {
            CfOut(cf_inform, "", " !! Unable to scan class db");
            CloseDB(dbp);
            DeleteItemList(list);
            return;
        }

        memset(&entry, 0, sizeof(entry));

        while (NextDB(dbp, dbcp, &key, &ksize, &stored, &vsize))
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
                    CfDebug("Class usage record %s expired\n", eventname);
                    DBCursorDeleteEntry(dbcp);
                }
                else if (!IsItemIn(list, eventname))
                {
                    newe.t = then;

                    newe.Q = QAverage(entry.Q, 0, 0.5);

                    if (newe.Q.expect <= 0.0001)
                    {
                        CfDebug("Deleting class %s as %lf is zero\n", eventname, newe.Q.expect);
                        DBCursorDeleteEntry(dbcp);
                    }
                    else
                    {
                        CfDebug("Downgrading class %s from %lf to %lf\n", eventname, entry.Q.expect, newe.Q.expect);
                        DBCursorWriteEntry(dbcp, &newe, sizeof(newe));
                    }
                }
            }
        }

        DeleteDBCursor(dbp, dbcp);
    }

    CloseDB(dbp);
    DeleteItemList(list);
}

/***************************************************************/
/* Last saw handling                                           */
/***************************************************************/

void LastSaw(char *username, char *ipaddress, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum roles role)
{
    char databuf[CF_BUFSIZE];
    char *mapip;

    if (strlen(ipaddress) == 0)
    {
        CfOut(cf_inform, "", "LastSeen registry for empty IP with role %d", role);
        return;
    }

    ThreadLock(cft_output);

    switch (role)
    {
    case cf_accept:
        snprintf(databuf, CF_BUFSIZE - 1, "-%s", HashPrint(CF_DEFAULT_DIGEST, digest));
        break;
    case cf_connect:
        snprintf(databuf, CF_BUFSIZE - 1, "+%s", HashPrint(CF_DEFAULT_DIGEST, digest));
        break;
    }

    ThreadUnlock(cft_output);

    mapip = MapAddress(ipaddress);

    UpdateLastSawHost(databuf, mapip);
}

/*****************************************************************************/

static void UpdateLastSawHost(char *rkey, char *ipaddress)
{
    CF_DB *dbp = NULL;
    KeyHostSeen q, newq;
    double lastseen, delta2;
    time_t now = time(NULL);
    char timebuf[26];


    if (!OpenDB(&dbp, dbid_lastseen))
    {
        CfOut(cf_inform, "", " !! Unable to open last seen db");
        return;
    }

    if (ReadDB(dbp, rkey, &q, sizeof(q)))
    {
        lastseen = (double) now - q.Q.q;

        if (q.Q.q <= 0)
        {
            lastseen = 300;
            q.Q = QDefinite(0.0);
        }

        newq.Q.q = (double) now;
        newq.Q.dq = newq.Q.q - q.Q.q;
        newq.Q.expect = GAverage(lastseen, q.Q.expect, 0.4);
        delta2 = (lastseen - q.Q.expect) * (lastseen - q.Q.expect);
        newq.Q.var = GAverage(delta2, q.Q.var, 0.4);
        strncpy(newq.address, ipaddress, CF_ADDRSIZE - 1);
    }
    else
    {
        lastseen = 0.0;
        newq.Q.q = (double) now;
        newq.Q.dq = 0;
        newq.Q.expect = 0.0;
        newq.Q.var = 0.0;
        strncpy(newq.address, ipaddress, CF_ADDRSIZE - 1);
    }

    if (strcmp(rkey + 1, PUBKEY_DIGEST) == 0)
    {
        Item *ip;
        int match = false;

        for (ip = IPADDRESSES; ip != NULL; ip = ip->next)
        {
            if (strcmp(VIPADDRESS, ip->name) == 0)
            {
                match = true;
                break;
            }
            if (strcmp(ipaddress, ip->name) == 0)
            {
                match = true;
                break;
            }
        }

        if (!match)
        {
            CfOut(cf_verbose, "", " ! Not updating last seen, as this appears to be a host with a duplicate key");
            CloseDB(dbp);

            return;
        }
    }

    CfOut(cf_verbose, "", " -> Last saw %s (alias %s) at %s\n", rkey, ipaddress, cf_strtimestamp_local(now, timebuf));

    PurgeMultipleIPReferences(dbp, rkey, ipaddress);

    WriteDB(dbp, rkey, &newq, sizeof(newq));

    CloseDB(dbp);
}

/*****************************************************************************/

bool RemoveHostFromLastSeen(const char *hostname, char *hostkey)
{
    char ip[CF_BUFSIZE];
    char digest[CF_BUFSIZE] = { 0 };

    if (!hostkey)
    {
        strcpy(ip, Hostname2IPString(hostname));
        IPString2KeyDigest(ip, digest);
    }
    else
    {
        snprintf(digest, sizeof(digest), "%s", hostkey);
    }

    CF_DB *dbp;
    char key[CF_BUFSIZE];

    if (!OpenDB(&dbp, dbid_lastseen))
    {
        CfOut(cf_error, "", " !! Unable to open last seen DB");
        return false;
    }

    snprintf(key, CF_BUFSIZE, "-%s", digest);
    DeleteComplexKeyDB(dbp, key, strlen(key) + 1);
    snprintf(key, CF_BUFSIZE, "+%s", digest);
    DeleteComplexKeyDB(dbp, key, strlen(key) + 1);

    CloseDB(dbp);
    return true;
}

/*****************************************************************************/

static void PurgeMultipleIPReferences(CF_DB *dbp, char *rkey, char *ipaddress)
{
    CF_DBC *dbcp;
    KeyHostSeen q;
    double lastseen, lsea = LASTSEENEXPIREAFTER;
    void *stored;
    char *key;
    time_t now = time(NULL);
    int qsize, ksize, update_address, keys_match;

// This is an expensive call, but it is the price we pay for consistency
// Make sure we only call it if we have to

    if (!NewDBCursor(dbp, &dbcp))
    {
        CfOut(cf_inform, "", " !! Unable to scan the last seen db");
        return;
    }

    while (NextDB(dbp, dbcp, &key, &ksize, &stored, &qsize))
    {
        keys_match = false;

        if (strcmp(key + 1, rkey + 1) == 0)
        {
            keys_match = true;
        }

        memcpy(&q, stored, sizeof(q));

        lastseen = (double) now - q.Q.q;

        if (lastseen > lsea)
        {
            CfOut(cf_verbose, "", " -> Last-seen record for %s expired after %.1lf > %.1lf hours\n", key,
                  lastseen / 3600, lsea / 3600);
            DBCursorDeleteEntry(dbcp);
            continue;
        }

        // Avoid duplicate address/key pairs

        if (keys_match && strcmp(q.address, ipaddress) != 0)
        {
            CfOut(cf_verbose, "",
                  " ! Synchronizing %s's address as this host %s seems to have moved from location %s to %s", key, rkey,
                  q.address, ipaddress);
            strcpy(q.address, ipaddress);
            update_address = true;
        }
        else if (!keys_match && strcmp(q.address, ipaddress) == 0)
        {
            CfOut(cf_verbose, "", " ! Updating %s's address (%s) as this host %s seems to have gone off line", key,
                  ipaddress, rkey);
            strcpy(q.address, CF_UNKNOWN_IP);
            update_address = true;
        }
        else
        {
            update_address = false;
        }

        if (update_address)
        {
            DBCursorWriteEntry(dbcp, &q, sizeof(q));
        }
    }

    DeleteDBCursor(dbp, dbcp);
}

/*****************************************************************************/
/* Toolkit                                                                   */
/*****************************************************************************/

double GAverage(double anew, double aold, double p)
/* return convex mixture - p is the trust/confidence in the new value */
{
    return (p * anew + (1.0 - p) * aold);
}

/*
 * expected(Q) = p*Q_new + (1-p)*expected(Q)
 * variance(Q) = p*(Q_new - expected(Q))^2 + (1-p)*variance(Q)
 */
QPoint QAverage(QPoint old, double new_q, double p)
{
    QPoint new = {
        .q = new_q,
    };

    double devsquare = (new.q - old.expect) * (new.q - old.expect);

    new.dq = fabs(new.q - old.q);
    new.expect = GAverage(new.q, old.expect, p);
    new.var = GAverage(devsquare, old.var, p);

    return new;
}

QPoint QDefinite(double q)
{
    return (QPoint) { .q = q, .dq = 0.0, .expect = q, .var = 0.0 };
}
