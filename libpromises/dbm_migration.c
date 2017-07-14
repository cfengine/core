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

#include <dbm_migration.h>

#include <lastseen.h>
#include <string_lib.h>

extern const DBMigrationFunction dbm_migration_plan_lastseen[];

static const DBMigrationFunction *const dbm_migration_plans[dbid_max] = {
    [dbid_lastseen] = dbm_migration_plan_lastseen
};

#ifdef LMDB
bool DBMigrate(ARG_UNUSED DBHandle *db, ARG_UNUSED  dbid id)
{
    return true;
}
#else
static size_t DBVersion(DBHandle *db)
{
    char version[64];
    if (ReadDB(db, "version", version, sizeof(version)) == false)
    {
        return 0;
    }
    else
    {
        return StringToLong(version);
    }
}

bool DBMigrate(DBHandle *db, dbid id)
{
    const DBMigrationFunction *plan = dbm_migration_plans[id];

    if (plan)
    {
        size_t step_version = 0;
        for (const DBMigrationFunction *step = plan; *step; step++, step_version++)
        {
            if (step_version == DBVersion(db))
            {
                if (!(*step)(db))
                {
                    return false;
                }
            }
        }
    }
    return true;
}
#endif
