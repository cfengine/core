/*
  Copyright 2022 Northern.tech AS

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

#include <dbm_migration.h>

#include <lastseen.h>
#include <logging.h>
#include <string_lib.h>

typedef struct
{
    double q;
    double expect;
    double var;
} QPoint0;

typedef struct
{
    time_t lastseen;
    QPoint Q; // Average time between connections (rolling weighted average)
} KeyHostSeen1;

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
    /* For some reason DB migration for LMDB was disabled in 2014 (in commit
     * 8611970bfa33be7b3cf0724eb684833e08582850). Unfortunately there is no
     * mention as to why this was done. Maybe it was not working?
     *
     * However, we're re-enabling it now (10 years later). Since this
     * migration function has not been active for the last 10 years, the
     * safest thing is to remove the migration logic, and only update the
     * version number.
     *
     * If you have not upgraded CFEngine in the last 10 years, this will be
     * the least of your problems.
     */
    return WriteDB(db, "version", "1", sizeof("1"));
}

static bool LastseenMigrationVersion1(DBHandle *db)
{
    DBCursor *cursor;
    if (!NewDBCursor(db, &cursor))
    {
        Log(LOG_LEVEL_ERR,
            "Unable to create database cursor during lastseen migration");
        return false;
    }

    char *key;
    void *value;
    int key_size, value_size;

    // Iterate through all key-value pairs
    while (NextDB(cursor, &key, &key_size, &value, &value_size))
    {
        if (key_size == 0)
        {
            Log(LOG_LEVEL_WARNING,
                "Found zero-length key during lastseen migration");
            continue;
        }

        // Only look for old KeyHostSeen entries
        if (key[0] != 'q')
        {
            // Warn about completely unexpected keys
            if ((key[0] != 'k') && (key[0] != 'a') && !StringEqualN(key, "version", key_size))
            {
                Log(LOG_LEVEL_WARNING,
                    "Found unexpected key '%s' during lastseen migration. "
                    "Only expecting 'version' or 'k', 'a' and 'q[i|o]' prefixed keys.",
                    key);
            }
            continue;
        }

        KeyHostSeen1 *old_value = value;
        KeyHostSeen new_value = {
            .acknowledged = true, // We don't know, assume yes
            .lastseen = old_value->lastseen,
            .Q = old_value->Q,
        };

        // This will overwrite the entry
        if (!DBCursorWriteEntry(cursor, &new_value, sizeof(new_value)))
        {
            Log(LOG_LEVEL_ERR,
                "Unable to write version 2 of entry for key '%s' during lastseen migration.",
                key);
            return false;
        }
    }

    if (!DeleteDBCursor(cursor))
    {
        Log(LOG_LEVEL_ERR, "Unable to close cursor during lastseen migration");
        return false;
    }

    if (!WriteDB(db, "version", "2", sizeof("2")))
    {
        Log(LOG_LEVEL_ERR, "Failed to update version number during lastseen migration");
        return false;
    }

    Log(LOG_LEVEL_INFO, "Migrated lastseen database from version 1 to 2");
    return true;
}

const DBMigrationFunction dbm_migration_plan_lastseen[] =
{
    LastseenMigrationVersion0,
    LastseenMigrationVersion1,
    NULL
};
