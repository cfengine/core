#include "cf3.defs.h"
#include "dbm_api.h"
#include "test.h"
#include "lastseen.h"

#include <setjmp.h>
#include <cmockery.h>

char CFWORKDIR[CF_BUFSIZE];

static void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/persistent_lock_test.XXXXXX");
    mkdtemp(CFWORKDIR);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

static const Event dummy_event = {
    .t = 1,
    .Q = { 2.0, 3.0, 4.0, 5.0 }
};

/*
 * Provides empty lastseen DB
 */
static DBHandle *setup(bool clean)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'/*", CFWORKDIR);
    system(cmd);

    DBHandle *db;
    OpenDB(&db, dbid_bundles);

    if (clean)
    {
        /* There is no way to disable hook in OpenDB yet, so just undo
         * everything */

        DBCursor *cursor;
        if (!NewDBCursor(db, &cursor))
        {
            return NULL;
        }

        char *key;
        void *value;
        int ksize, vsize;

        while (NextDB(db, cursor, &key, &ksize, &value, &vsize))
        {
            DBCursorDeleteEntry(cursor);
        }

        if (!DeleteDBCursor(db, cursor))
        {
            return NULL;
        }
    }

    return db;
}

static void test_no_migration(void **context)
{
    DBHandle *db = setup(true);
    CloseDB(db);

    /* Migration on empty DB should produce single "version" key */

    assert_int_equal(OpenDB(&db, dbid_lastseen), true);

    DBCursor *cursor;
    assert_int_equal(NewDBCursor(db, &cursor), true);

    char *key;
    void *value;
    int ksize, vsize;

    while (NextDB(db, cursor, &key, &ksize, &value, &vsize))
    {
        assert_int_equal(ksize, strlen("version") + 1);
        assert_string_equal(key, "version");
        assert_int_equal(vsize, 2);
        assert_string_equal(value, "1");
    }

    assert_int_equal(DeleteDBCursor(db, cursor), true);

    CloseDB(db);
}

static void test_up_to_date(void **context)
{
    /* Test that upgrade is not performed if there is already a version
     * marker */

    DBHandle *db = setup(false);
    assert_int_equal(WriteDB(db, "foo", &dummy_event, sizeof(dummy_event)), true);
    CloseDB(db);

    /* Test that manually inserted key still has unqalified name the next
       time the DB is opened, which is an indicator of the DB not being
       upgraded */

    assert_int_equal(OpenDB(&db, dbid_bundles), true);

    Event read_value;

    assert_int_equal(ReadDB(db, "foo", &read_value, sizeof(read_value)), true);
    assert_int_equal(read_value.t, 1);

    CloseDB(db);
}

void test_migrate_unqualified_names(void **state)
{
    DBHandle *db = setup(true);
    assert_int_equal(WriteDB(db, "foo", &dummy_event, sizeof(dummy_event)), true);
    assert_int_equal(WriteDB(db, "q.bar", &dummy_event, sizeof(dummy_event)), true);
    CloseDB(db);

    assert_int_equal(OpenDB(&db, dbid_bundles), true);

    /* Old entry migrated */
    assert_int_equal(HasKeyDB(db, "foo", strlen("foo") + 1), false);
    assert_int_equal(HasKeyDB(db, "default.foo", strlen("default.foo") + 1), true);
    Event read_value = { 0 };
    ReadDB(db, "default.foo", &read_value, sizeof(read_value));
    assert_memory_equal(&read_value, &dummy_event, sizeof(dummy_event));

    /* New entry preserved */
    assert_int_equal(HasKeyDB(db, "q.bar", strlen("q.bar") + 1), true);
    memset(&read_value, 0, sizeof(read_value));
    ReadDB(db, "q.bar", &read_value, sizeof(read_value));
    assert_memory_equal(&read_value, &dummy_event, sizeof(dummy_event));

    /* Version marker */
    assert_int_equal(HasKeyDB(db, "version", strlen("version") + 1), true);
    CloseDB(db);
}

int main()
{
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_no_migration),
            unit_test(test_up_to_date),
            unit_test(test_migrate_unqualified_names),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    tests_teardown();

    return ret;
}

/* STUBS */
const char *DAY_TEXT[] = {};
const char *MONTH_TEXT[] = {};

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}

void CfOut(enum cfreport level, const char *errstr, const char *fmt, ...)
{
    fprintf(stderr, "CFOUT<%d>: ", level);
    if (errstr)
    {
        fprintf(stderr, " %s: %s ", errstr, strerror(errno));
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
