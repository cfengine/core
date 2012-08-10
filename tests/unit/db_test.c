#include "test.h"

#include "cf3.defs.h"
#include "dbm_api.h"


char CFWORKDIR[CF_BUFSIZE] = "/tmp";

void test_iter_modify_entry(void **state)
{
    /* Test that deleting entry under cursor does not interrupt iteration */

    unlink("/tmp/cf_classes.qdbm");
    unlink("/tmp/cf_classes.tcdb");

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

    unlink("/tmp/cf_classes.qdbm");
    unlink("/tmp/cf_classes.tcdb");

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

    unlink("/tmp/cf_classes.tcdb");
    unlink("/tmp/cf_classes.qdbm");

    CreateGarbage("/tmp/cf_classes.tcdb");
    CreateGarbage("/tmp/cf_classes.qdbm");

    CF_DB *db;
    assert_int_equal(OpenDB(&db, dbid_classes), true);
    CloseDB(db);
}

int main()
{
    const UnitTest tests[] =
        {
            unit_test(test_iter_modify_entry),
            unit_test(test_iter_delete_entry),
            unit_test(test_recreate),
        };

    PRINT_TEST_BANNER();
    return run_tests(tests);
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

