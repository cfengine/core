#include <stdlib.h>
#include <sys/stat.h>
#include <test.h>
#include <known_dirs.h>

#include <cf3.defs.h>
#include <dbm_api.h>
#include <file_lib.h>
#include <files_copy.h>
#include <misc_lib.h>                                          /* xsnprintf */


char CFWORKDIR[CF_BUFSIZE];

void tests_setup(void)
{
    static char env[] = /* Needs to be static for putenv() */
        "CFENGINE_TEST_OVERRIDE_WORKDIR=/tmp/db_test.XXXXXX";

    char *workdir = strchr(env, '=') + 1; /* start of the path */
    assert(workdir - 1 && workdir[0] == '/');

    mkdtemp(workdir);
    strlcpy(CFWORKDIR, workdir, CF_BUFSIZE);
    putenv(env);
    mkdir(GetStateDir(), (S_IRWXU | S_IRWXG | S_IRWXO));
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
    xsnprintf(tcdb_db, CF_BUFSIZE, "%s/cf_classes.tcdb", GetStateDir());
    CreateGarbage(tcdb_db);
#endif
#ifdef HAVE_LIBQDBM
    char qdbm_db[CF_BUFSIZE];
    xsnprintf(qdbm_db, CF_BUFSIZE, "%s/cf_classes.qdbm", GetStateDir());
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

void test_old_workdir_db_location(void)
{
#ifndef LMDB
    // We manipulate the LMDB file name directly. Not adapted to the others.
    return;
#endif

    CF_DB *db;

    char *state_dir;

    xasprintf(&state_dir, "%s%cstate", GetWorkDir(), FILE_SEPARATOR);

    if (strcmp(GetStateDir(), state_dir) != 0)
    {
        // Test only works when statedir is $(workdir)/state.
        free(state_dir);
        return;
    }

    assert_true(OpenDB(&db, dbid_lastseen));
    assert_true(WriteDB(db, "key", "first_value", strlen("first_value") + 1));
    CloseDB(db);

    char *old_db, *orig_db, *new_db;
    // Due to caching of the path we need to use a different db when opening the
    // second time, otherwise the path is not rechecked.
    xasprintf(&orig_db, "%s%ccf_lastseen.lmdb", GetStateDir(), FILE_SEPARATOR);
    xasprintf(&old_db, "%s%ccf_audit.lmdb", GetWorkDir(), FILE_SEPARATOR);
    xasprintf(&new_db, "%s%ccf_audit.lmdb", GetStateDir(), FILE_SEPARATOR);

    // Copy database to old location.
    assert_true(CopyRegularFileDisk(orig_db, old_db));

    // Change content.
    assert_true(OpenDB(&db, dbid_lastseen));
    assert_true(WriteDB(db, "key", "second_value", strlen("second_value") + 1));
    CloseDB(db);

    // Copy database to new location.
    assert_true(CopyRegularFileDisk(orig_db, new_db));

    char value[CF_BUFSIZE];

    // Old location should take precedence.
    assert_true(OpenDB(&db, dbid_audit));
    assert_true(ReadDB(db, "key", value, sizeof(value)));
    assert_string_equal(value, "first_value");
    CloseDB(db);

    free(state_dir);
    free(old_db);
    free(orig_db);
    free(new_db);
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
            unit_test(test_old_workdir_db_location),
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


