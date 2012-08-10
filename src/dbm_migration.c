#include "dbm_migration.h"

#include "lastseen.h"
#include "string_lib.h"

extern DBMigrationFunction dbm_migration_plan_bundles[];
extern DBMigrationFunction dbm_migration_plan_lastseen[];

static DBMigrationFunction *dbm_migration_plans[dbid_max] = {
    [dbid_bundles] = dbm_migration_plan_bundles,
    [dbid_lastseen] = dbm_migration_plan_lastseen
};

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
    DBMigrationFunction *plan = dbm_migration_plans[id];

    if (plan)
    {
        size_t step_version = 0;
        for (DBMigrationFunction *step = plan; *step; step++, step_version++)
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
