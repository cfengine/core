#include <stdlib.h>
#include <sys/stat.h>
#include <test.h>
#include <known_dirs.h>

#include <cf3.defs.h>
#include <dbm_api.h>
#include <string_lib.h>                                       /* StringEqual */
#include <file_lib.h>                                  /* DeleteDirectoryTree */

#ifndef LMDB

/* The measurement DB migration only exists for the LMDB backend (see
 * dbm_migration.c); with other backends DBMigrate() is a no-op. */
int main(void)
{
    return 0;
}

#else

/* The Averages layout from before ENT-6511 raised CF_OBSERVABLES from 100 to
 * 300. A record written by such an agent is this size on disk. */
#define CF_OBSERVABLES_BEFORE_ENT_6511 100

typedef struct
{
    time_t last_seen;
    QPoint Q[CF_OBSERVABLES_BEFORE_ENT_6511];
} AveragesBeforeEnt6511;

char CFWORKDIR[CF_BUFSIZE];

void tests_setup(void)
{
    static char env[] = /* Needs to be static for putenv() */
        "CFENGINE_TEST_OVERRIDE_WORKDIR=/tmp/observations_migration_test.XXXXXX";

    char *workdir = strchr(env, '=') + 1; /* start of the path */
    assert(workdir - 1 && workdir[0] == '/');

    mkdtemp(workdir);
    strlcpy(CFWORKDIR, workdir, CF_BUFSIZE);
    putenv(env);
    mkdir(GetStateDir(), (S_IRWXU | S_IRWXG | S_IRWXO));
}

static void tests_teardown(void)
{
    /* DeleteDirectoryTree() empties the tree but keeps the top directory, so
     * rmdir() the now-empty workdir afterwards. */
    DeleteDirectoryTree(CFWORKDIR);
    rmdir(CFWORKDIR);
}

/*
 * Provides an observations DB in a pre-migration (version 0) state: OpenDB()
 * runs the migration and writes a "version" marker, so wipe every entry to get
 * back to how an old, never-migrated database looks on disk.
 */
static DBHandle *setup_unversioned(void)
{
    /* Empties the state dir (keeps the dir itself) so OpenDB() starts fresh. */
    assert_true(DeleteDirectoryTree(GetStateDir()));

    DBHandle *db;
    assert_true(OpenDB(&db, dbid_observations));

    DBCursor *cursor;
    assert_true(NewDBCursor(db, &cursor));

    char *key;
    void *value;
    int ksize, vsize;
    while (NextDB(cursor, &key, &ksize, &value, &vsize))
    {
        DBCursorDeleteEntry(cursor);
    }
    assert_true(DeleteDBCursor(cursor));

    return db;
}

/* Returns the on-disk size of a record, or -1 if the key is absent. */
static int record_size(DBHandle *db, const char *want_key)
{
    DBCursor *cursor;
    assert_true(NewDBCursor(db, &cursor));

    char *key;
    void *value;
    int ksize, vsize;
    int found = -1;
    while (NextDB(cursor, &key, &ksize, &value, &vsize))
    {
        if (StringEqual(key, want_key))
        {
            found = vsize;
        }
    }
    assert_true(DeleteDBCursor(cursor));
    return found;
}

static void test_migrate_expands_old_record(void)
{
    DBHandle *db = setup_unversioned();

    /* A measurement record in the old, shorter (100-slot) layout ... */
    AveragesBeforeEnt6511 old;
    memset(&old, 0, sizeof(old));
    old.last_seen = 1234567;
    old.Q[0].q = 1.0;
    old.Q[0].expect = 2.0;
    old.Q[0].var = 3.0;
    old.Q[0].dq = 4.0;
    old.Q[CF_OBSERVABLES_BEFORE_ENT_6511 - 1].q = 5.0;
    old.Q[CF_OBSERVABLES_BEFORE_ENT_6511 - 1].expect = 6.0;
    old.Q[CF_OBSERVABLES_BEFORE_ENT_6511 - 1].var = 7.0;
    old.Q[CF_OBSERVABLES_BEFORE_ENT_6511 - 1].dq = 8.0;
    assert_true(WriteDB(db, "Mon_Hr12_Q1", &old, sizeof(old)));

    /* ... and a scalar bookkeeping record that must NOT be touched. */
    double age = 42.0;
    assert_true(WriteDB(db, "DATABASE_AGE", &age, sizeof(age)));

    CloseDB(db);

    /* Reopening runs the migration (no version marker present yet). */
    assert_true(OpenDB(&db, dbid_observations));

    /* The measurement record is now the full, current size ... */
    assert_int_equal(record_size(db, "Mon_Hr12_Q1"), (int) sizeof(Averages));
    /* ... the scalar record is left at its original size ... */
    assert_int_equal(record_size(db, "DATABASE_AGE"), (int) sizeof(double));
    /* ... and the version marker was written. */
    assert_true(HasKeyDB(db, "version", strlen("version") + 1));

    /* The original slots are preserved and the new slots read back as zero. */
    Averages migrated;
    memset(&migrated, 0, sizeof(migrated));
    assert_true(ReadDB(db, "Mon_Hr12_Q1", &migrated, sizeof(migrated)));
    assert_int_equal(migrated.last_seen, 1234567);
    assert_double_close(migrated.Q[0].q, 1.0);
    assert_double_close(migrated.Q[0].dq, 4.0);
    assert_double_close(migrated.Q[CF_OBSERVABLES_BEFORE_ENT_6511 - 1].q, 5.0);
    assert_double_close(migrated.Q[CF_OBSERVABLES_BEFORE_ENT_6511 - 1].dq, 8.0);
    assert_double_close(migrated.Q[CF_OBSERVABLES_BEFORE_ENT_6511].q, 0.0);
    assert_double_close(migrated.Q[CF_OBSERVABLES - 1].var, 0.0);

    double read_age = 0.0;
    assert_true(ReadDB(db, "DATABASE_AGE", &read_age, sizeof(read_age)));
    assert_double_close(read_age, 42.0);

    CloseDB(db);
}

static void test_current_record_untouched(void)
{
    /* A record already at the current size must not be altered by the
     * migration (the size-exact check must ignore it). */
    DBHandle *db = setup_unversioned();

    Averages current;
    memset(&current, 0, sizeof(current));
    current.last_seen = 7654321;
    current.Q[CF_OBSERVABLES - 1].q = 99.0;
    assert_true(WriteDB(db, "Tue_Hr06_Q2", &current, sizeof(current)));
    CloseDB(db);

    assert_true(OpenDB(&db, dbid_observations));

    assert_int_equal(record_size(db, "Tue_Hr06_Q2"), (int) sizeof(Averages));

    Averages read;
    memset(&read, 0, sizeof(read));
    assert_true(ReadDB(db, "Tue_Hr06_Q2", &read, sizeof(read)));
    assert_int_equal(read.last_seen, 7654321);
    assert_double_close(read.Q[CF_OBSERVABLES - 1].q, 99.0);

    CloseDB(db);
}

static void test_migration_is_idempotent(void)
{
    /* Once migrated, reopening must not run the migration again nor disturb
     * the data. */
    DBHandle *db = setup_unversioned();

    AveragesBeforeEnt6511 old;
    memset(&old, 0, sizeof(old));
    old.last_seen = 111;
    assert_true(WriteDB(db, "Wed_Hr18_Q3", &old, sizeof(old)));
    CloseDB(db);

    /* First open migrates. */
    assert_true(OpenDB(&db, dbid_observations));
    assert_int_equal(record_size(db, "Wed_Hr18_Q3"), (int) sizeof(Averages));
    CloseDB(db);

    /* Second open is a no-op. */
    assert_true(OpenDB(&db, dbid_observations));
    assert_int_equal(record_size(db, "Wed_Hr18_Q3"), (int) sizeof(Averages));

    char version[8];
    assert_true(ReadDB(db, "version", version, sizeof(version)));
    assert_string_equal(version, "1");

    CloseDB(db);
}

int main(void)
{
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_migrate_expands_old_record),
            unit_test(test_current_record_untouched),
            unit_test(test_migration_is_idempotent),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    tests_teardown();
    return ret;
}

/* STUBS */

/* OpenDB() pulls in libpromises DB code that references FatalError(), but the
 * real implementation lives in the daemons, not the test binary. Provide a stub
 * so the test links; the migration paths under test never call it, so reaching
 * it is itself a test failure. */
void FatalError(ARG_UNUSED char *s, ...)
{
    fail();
    exit(42);
}

#endif // LMDB
