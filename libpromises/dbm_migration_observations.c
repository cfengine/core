/*
  Copyright 2026 Northern.tech AS

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

#include <dbm_api.h>
#include <logging.h>

/* Number of measurement slots (CF_OBSERVABLES) before ENT-6511 raised it from
 * 100 to 300. A record written by an agent from before that change is this many
 * slots long. */
#define CF_OBSERVABLES_BEFORE_ENT_6511 100

typedef struct
{
    time_t last_seen;
    QPoint Q[CF_OBSERVABLES_BEFORE_ENT_6511];
} AveragesBeforeEnt6511;

/*
 * The observations (cf_observations.lmdb) and history (history.lmdb) databases
 * store fixed-size Averages records keyed by time. Raising CF_OBSERVABLES grows
 * that struct, so a record written by an older agent is shorter than the current
 * struct. cf-monitord's own read path zero-extends short records, and it
 * overwrites the observations records on its next cycle, but the history records
 * are never rewritten. Migrate every old-size record to the current size,
 * zero-filling the added slots, so all records on disk share one layout.
 */
static bool MeasurementsMigrationVersion0(DBHandle *db)
{
    DBCursor *cursor;
    if (!NewDBCursor(db, &cursor))
    {
        Log(LOG_LEVEL_ERR,
            "Unable to create database cursor during measurement DB migration");
        return false;
    }

    char *key;
    void *value;
    int key_size, value_size;

    while (NextDB(cursor, &key, &key_size, &value, &value_size))
    {
        /* Only fixed-size Averages records written before CF_OBSERVABLES was
         * raised need expanding. Scalar bookkeeping keys (e.g. "DATABASE_AGE"
         * and "version") are a different size and are left untouched. */
        if (value_size != (int) sizeof(AveragesBeforeEnt6511))
        {
            continue;
        }

        /* Copy the old, shorter record into a full-size, zeroed struct so the
         * slots added by the larger CF_OBSERVABLES read back as zero. */
        Averages expanded;
        memset(&expanded, 0, sizeof(expanded));
        memcpy(&expanded, value, sizeof(AveragesBeforeEnt6511));

        // This will overwrite the entry
        if (!DBCursorWriteEntry(cursor, &expanded, sizeof(expanded)))
        {
            Log(LOG_LEVEL_ERR,
                "Unable to expand measurement record for key '%s' during migration",
                key);
            DeleteDBCursor(cursor);
            return false;
        }
    }

    if (!DeleteDBCursor(cursor))
    {
        Log(LOG_LEVEL_ERR,
            "Unable to close cursor during measurement DB migration");
        return false;
    }

    if (!WriteDB(db, "version", "1", sizeof("1")))
    {
        Log(LOG_LEVEL_ERR,
            "Failed to update version number during measurement DB migration");
        return false;
    }

    Log(LOG_LEVEL_INFO, "Migrated measurement database to version 1");
    return true;
}

const DBMigrationFunction dbm_migration_plan_observations[] =
{
    MeasurementsMigrationVersion0,
    NULL
};
