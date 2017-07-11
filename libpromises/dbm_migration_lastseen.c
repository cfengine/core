/*
   Copyright 2017 Northern.tech AS

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <dbm_migration.h>

#include <lastseen.h>
#include <logging.h>

typedef struct
{
    double q;
    double expect;
    double var;
} QPoint0;

#define QPOINT0_OFFSET 128

/*
 * Structure of version 0 lastseen entry:
 *
 * flag | hostkey -> address | QPoint
 *  |        |          |         \- 3*double
 *  |        |          \- 128 chars
 *  |        \- N*chars
 *  \- 1 byte, '+' or '-'
 */

static bool LastseenMigrationVersion0(DBHandle *db)
{
    bool errors = false;
    DBCursor *cursor;

    if (!NewDBCursor(db, &cursor))
    {
        return false;
    }

    char *key;
    void *value;
    int ksize, vsize;

    while (NextDB(cursor, &key, &ksize, &value, &vsize))
    {
        if (ksize == 0)
        {
            Log(LOG_LEVEL_INFO, "LastseenMigrationVersion0: Database structure error -- zero-length key.");
            continue;
        }

        /* Only look for old [+-]kH -> IP<QPoint> entries */
        if ((key[0] != '+') && (key[0] != '-'))
        {
            /* Warn about completely unexpected keys */

            if ((key[0] != 'q') && (key[0] != 'k') && (key[0] != 'a'))
            {
                Log(LOG_LEVEL_INFO, "LastseenMigrationVersion0: Malformed key found '%s'", key);
            }

            continue;
        }

        bool incoming = key[0] == '-';
        const char *hostkey = key + 1;

        /* Only migrate sane data */
        if (vsize != QPOINT0_OFFSET + sizeof(QPoint0))
        {
            Log(LOG_LEVEL_INFO,
                "LastseenMigrationVersion0: invalid value size for key '%s', entry is deleted",
                key);
            DBCursorDeleteEntry(cursor);
            continue;
        }

        /* Properly align the data */
        const char *old_data_address = (const char *) value;
        QPoint0 old_data_q;
        memcpy(&old_data_q, (const char *) value + QPOINT0_OFFSET,
               sizeof(QPoint0));

        char hostkey_key[CF_BUFSIZE];
        snprintf(hostkey_key, CF_BUFSIZE, "k%s", hostkey);

        if (!WriteDB(db, hostkey_key, old_data_address, strlen(old_data_address) + 1))
        {
            Log(LOG_LEVEL_INFO, "Unable to write version 1 lastseen entry for '%s'", key);
            errors = true;
            continue;
        }

        char address_key[CF_BUFSIZE];
        snprintf(address_key, CF_BUFSIZE, "a%s", old_data_address);

        if (!WriteDB(db, address_key, hostkey, strlen(hostkey) + 1))
        {
            Log(LOG_LEVEL_INFO, "Unable to write version 1 reverse lastseen entry for '%s'", key);
            errors = true;
            continue;
        }

        char quality_key[CF_BUFSIZE];
        snprintf(quality_key, CF_BUFSIZE, "q%c%s", incoming ? 'i' : 'o', hostkey);

        /*
           Ignore malformed connection quality data
        */

        if ((!isfinite(old_data_q.q))
            || (old_data_q.q < 0)
            || (!isfinite(old_data_q.expect))
            || (!isfinite(old_data_q.var)))
        {
            Log(LOG_LEVEL_INFO, "Ignoring malformed connection quality data for '%s'", key);
            DBCursorDeleteEntry(cursor);
            continue;
        }

        KeyHostSeen data = {
            .lastseen = (time_t)old_data_q.q,
            .Q = {
                /*
                   Previously .q wasn't stored in database, but was calculated
                   every time as a difference between previous timestamp and a
                   new timestamp. Given we don't have this information during
                   the database upgrade, just assume that last reading is an
                   average one.
                */
                .q = old_data_q.expect,
                .dq = 0,
                .expect = old_data_q.expect,
                .var = old_data_q.var,
            }
        };

        if (!WriteDB(db, quality_key, &data, sizeof(data)))
        {
            Log(LOG_LEVEL_INFO, "Unable to write version 1 connection quality key for '%s'", key);
            errors = true;
            continue;
        }

        if (!DBCursorDeleteEntry(cursor))
        {
            Log(LOG_LEVEL_INFO, "Unable to delete version 0 lastseen entry for '%s'", key);
            errors = true;
        }
    }

    if (DeleteDBCursor(cursor) == false)
    {
        Log(LOG_LEVEL_ERR, "LastseenMigrationVersion0: Unable to close cursor");
        errors = true;
    }

    if ((!errors) && (!WriteDB(db, "version", "1", sizeof("1"))))
    {
        errors = true;
    }

    return !errors;
}

const DBMigrationFunction dbm_migration_plan_lastseen[] =
{
    LastseenMigrationVersion0,
    NULL
};
