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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <dbm_migration.h>

#include <logging.h>
#include <string_lib.h>

static bool BundlesMigrationVersion0(DBHandle *db)
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
            Log(LOG_LEVEL_INFO, "BundlesMigrationVersion0: Database structure error -- zero-length key.");
            continue;
        }

        if (strchr(key, '.')) // is qualified name?
        {
            continue;
        }

        char *fqname = StringConcatenate(3, "default", ".", key);
        if (!WriteDB(db, fqname, value, vsize))
        {
            Log(LOG_LEVEL_INFO, "Unable to write version 1 bundle entry for '%s'", key);
            errors = true;
            continue;
        }

        if (!DBCursorDeleteEntry(cursor))
        {
            Log(LOG_LEVEL_INFO, "Unable to delete version 0 bundle entry for '%s'", key);
            errors = true;
        }
    }

    if (DeleteDBCursor(cursor) == false)
    {
        Log(LOG_LEVEL_ERR, "BundlesMigrationVersion0: Unable to close cursor");
        errors = true;
    }

    if ((!errors) && (!WriteDB(db, "version", "1", sizeof("1"))))
    {
        errors = true;
    }

    return !errors;
}

const DBMigrationFunction dbm_migration_plan_bundles[] =
{
    BundlesMigrationVersion0,
    NULL
};

