#include <test.h>

#include <cf3.defs.h>
#include <dbm_api.h>
#include <misc_lib.h>                                          /* xsnprintf */


char CFWORKDIR[CF_BUFSIZE];

void tests_setup(void)
{
    xsnprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/db_test.XXXXXX");
    mkdtemp(CFWORKDIR);
}

void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    xsnprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    system(cmd);
}

void test_open_close(void)
{
    // Test that we can simply open and close a database without doing anything.
    CF_DB *db;
    assert_int_equal(OpenDB(&db, dbid_classes), true);
    CloseDB(db);
}

void test_read_write(void)
{
    // Test that we can do normal reads and write, and that the values are
    // reflected in the database.
    CF_DB *db;
    char value[CF_BUFSIZE];
    strcpy(value, "myvalue");
    int vsize = strlen(value) + 1;

    assert_int_equal(OpenDB(&db, dbid_classes), true);

    assert_int_equal(ReadDB(db, "written_entry", &value, vsize), false);
    assert_string_equal(value, "myvalue");
    assert_int_equal(WriteDB(db, "written_entry", value, vsize), true);
    strcpy(value, "");
    assert_int_equal(ReadDB(db, "written_entry", &value, vsize), true);
    assert_string_equal(value, "myvalue");

    CloseDB(db);

    // Check also after we reopen the database.
    assert_int_equal(OpenDB(&db, dbid_classes), true);
    strcpy(value, "");
    assert_int_equal(ReadDB(db, "written_entry", &value, vsize), true);
    assert_string_equal(value, "myvalue");

    CloseDB(db);
}

void test_iter_modify_entry(void)
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

    assert_int_equal(NextDB(cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DBCursorWriteEntry(cursor, "eee", 3), true);

    assert_int_equal(NextDB(cursor, &key, &ksize, &value, &vsize), true);
    assert_int_equal(NextDB(cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DeleteDBCursor(cursor), true);

    CloseDB(db);
}


void test_iter_delete_entry(void)
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

    assert_int_equal(NextDB(cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DBCursorDeleteEntry(cursor), true);

    assert_int_equal(NextDB(cursor, &key, &ksize, &value, &vsize), true);
    assert_int_equal(NextDB(cursor, &key, &ksize, &value, &vsize), true);

    assert_int_equal(DeleteDBCursor(cursor), true);

    CloseDB(db);
}

#if defined(HAVE_LIBTOKYOCABINET) || defined(HAVE_LIBQDBM) || defined(HAVE_LIBLMDB)
static void CreateGarbage(const char *filename)
{
    FILE *fh = fopen(filename, "w");
    for(int i = 0; i < 2; ++i)
    {
        fwrite("some garbage!", 14, 1, fh);
    }
    fclose(fh);
}
#endif /* HAVE_LIBTOKYOCABINET || HAVE_LIBQDBM */

void test_recreate(void)
{
    /* Test that recreating database works properly */
#ifdef HAVE_LIBTOKYOCABINET
    char tcdb_db[CF_BUFSIZE];
    xsnprintf(tcdb_db, CF_BUFSIZE, "%s/cf_classes.tcdb", CFWORKDIR);
    CreateGarbage(tcdb_db);
#endif
#ifdef HAVE_LIBQDBM
    char qdbm_db[CF_BUFSIZE];
    xsnprintf(qdbm_db, CF_BUFSIZE, "%s/cf_classes.qdbm", CFWORKDIR);
    CreateGarbage(qdbm_db);
#endif
#ifdef HAVE_LIBLMDB
    char lmdb_db[CF_BUFSIZE];
    xsnprintf(lmdb_db, CF_BUFSIZE, "%s/cf_classes.lmdb", CFWORKDIR);
    CreateGarbage(lmdb_db);
#endif

    CF_DB *db;
    assert_int_equal(OpenDB(&db, dbid_classes), true);
    CloseDB(db);
}

int main()
{
    PRINT_TEST_BANNER();
    tests_setup();

    const UnitTest tests[] =
        {
            unit_test(test_open_close),
            unit_test(test_read_write),
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

void FatalError(ARG_UNUSED char *s, ...)
{
    fail();
    exit(42);
}


