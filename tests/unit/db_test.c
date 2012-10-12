#include "test.h"

#include "cf3.defs.h"
#include "dbm_api.h"

char CFWORKDIR[CF_BUFSIZE];

void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/db_test.XXXXXX");
    mkdtemp(CFWORKDIR);
}

void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

void test_iter_modify_entry(void **state)
{
    /* Test that deleting entry under cursor does not interrupt iteration */

    CF_DB *db;
    assert_int_equal(OpenDB(&db, dbid_classes), true);

    assert_int_equal(WriteDB(db, "foobar", "abc", 3), true);
    assert_int_equal(WriteDB(db, "bazbaz", "def", 3), true);
    assert_int_equal(WriteDB(db, "booo", "ghi", 3), true);

    CF_DBC *cursor;
    assert_int_equal(NewDBCursor(db, &cursor), true);

    char *key;
    int ksize;
    void *value;
    int vsize;

    assert_int_equal(NextDB(db, cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DBCursorWriteEntry(cursor, "eee", 3), true);

    assert_int_equal(NextDB(db, cursor, &key, &ksize, &value, &vsize), true);
    assert_int_equal(NextDB(db, cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DeleteDBCursor(db, cursor), true);

    CloseDB(db);
}


void test_iter_delete_entry(void **state)
{
    /* Test that deleting entry under cursor does not interrupt iteration */

    CF_DB *db;
    assert_int_equal(OpenDB(&db, dbid_classes), true);

    assert_int_equal(WriteDB(db, "foobar", "abc", 3), true);
    assert_int_equal(WriteDB(db, "bazbaz", "def", 3), true);
    assert_int_equal(WriteDB(db, "booo", "ghi", 3), true);

    CF_DBC *cursor;
    assert_int_equal(NewDBCursor(db, &cursor), true);

    char *key;
    int ksize;
    void *value;
    int vsize;

    assert_int_equal(NextDB(db, cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DBCursorDeleteEntry(cursor), true);

    assert_int_equal(NextDB(db, cursor, &key, &ksize, &value, &vsize), true);
    assert_int_equal(NextDB(db, cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DeleteDBCursor(db, cursor), true);

    CloseDB(db);
}

static void CreateGarbage(const char *filename)
{
    FILE *fh = fopen(filename, "w");
    for(int i = 0; i < 1000; ++i)
    {
        fwrite("some garbage!", 14, 1, fh);
    }
    fclose(fh);
}

void test_recreate(void **state)
{
    /* Test that recreating database works properly */

    char tcdb_db[CF_BUFSIZE];
    snprintf(tcdb_db, CF_BUFSIZE, "%s/cf_classes.tcdb", CFWORKDIR);
    CreateGarbage(tcdb_db);

    char qdbm_db[CF_BUFSIZE];
    snprintf(qdbm_db, CF_BUFSIZE, "%s/cf_classes.qdbm", CFWORKDIR);
    CreateGarbage(qdbm_db);

    CF_DB *db;
    assert_int_equal(OpenDB(&db, dbid_classes), true);
    CloseDB(db);
}

int main()
{
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_iter_modify_entry),
            unit_test(test_iter_delete_entry),
            unit_test(test_recreate),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    tests_teardown();
    return ret;
}

/* STUBS */

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

const char *DAY_TEXT[] = {};
const char *MONTH_TEXT[] = {};

