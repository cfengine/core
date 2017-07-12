/*
   Copyright 2017 Northern.tech AS

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

#include <test.h>
#include <dbm_api.h>
#include <cfnet.h>
#include <sequence.h>
#include <misc_lib.h>                                          /* xsnprintf */


#include <files_changes.c>

#define NO_FILES 4

char *CHECKSUM_VALUE[NO_FILES] =
    {
        "0001",
        "0002",
        "0003",
        "0004",
    };
struct stat filestat_value;

static void test_setup(void)
{
    static char env[] = /* Needs to be static for putenv() */
        "CFENGINE_TEST_OVERRIDE_WORKDIR=/tmp/changes_migration_test.XXXXXX";

    char *workdir = strchr(env, '=') + 1; /* start of the path */
    assert(workdir - 1 && workdir[0] == '/');

    mkdtemp(workdir);
    putenv(env);
    mkdir(GetStateDir(), (S_IRWXU | S_IRWXG | S_IRWXO));

    CF_DB *db;
    assert_true(OpenDB(&db, dbid_checksums));
    // Hand crafted from the old version of NewIndexKey().
    char checksum_key[NO_FILES][30] =
        {
            { 'M','D','5','\0','\0','\0','\0','\0',
              '/','e','t','c','/','h','o','s','t','s','\0' },
            { 'M','D','5','\0','\0','\0','\0','\0',
              '/','e','t','c','/','p','a','s','s','w','d','\0' },
            { 'M','D','5','\0','\0','\0','\0','\0',
              '/','f','i','l','e','1','\0' },
            { 'M','D','5','\0','\0','\0','\0','\0',
              '/','f','i','l','e','2','\0' },
        };

    for (int c = 0; c < NO_FILES; c++)
    {
        int ksize = CHANGES_HASH_FILE_NAME_OFFSET + strlen(checksum_key[c] + CHANGES_HASH_FILE_NAME_OFFSET) + 1;
        int vsize = strlen(CHECKSUM_VALUE[c]) + 1;
        assert_true(WriteComplexKeyDB(db, checksum_key[c], ksize,
                                      CHECKSUM_VALUE[c], vsize));
    }

    CloseDB(db);

    assert_true(OpenDB(&db, dbid_filestats));

    char *filestat_key[NO_FILES] =
        {
            "/etc/hosts",
            "/etc/passwd",
            "/file1",
            "/file2",
        };
    filestat_value.st_uid = 4321;
    memset(&filestat_value, 0, sizeof(filestat_value));

    for (int c = 0; c < NO_FILES; c++)
    {
        assert_true(WriteDB(db, filestat_key[c],
                            &filestat_value, sizeof(filestat_value)));
    }

    CloseDB(db);
}

static void test_migration(void)
{
    CF_DB *db;
    Seq *list = SeqNew(NO_FILES, free);
    // Hand crafted from the new version of NewIndexKey().
    char checksum_key[NO_FILES][30] =
        {
            { 'H','_','M','D','5','\0','\0','\0','\0','\0',
              '/','e','t','c','/','h','o','s','t','s','\0' },
            { 'H','_','M','D','5','\0','\0','\0','\0','\0',
              '/','e','t','c','/','p','a','s','s','w','d','\0' },
            { 'H','_','M','D','5','\0','\0','\0','\0','\0',
              '/','f','i','l','e','1','\0' },
            { 'H','_','M','D','5','\0','\0','\0','\0','\0',
              '/','f','i','l','e','2','\0' },
        };
    char *filestat_key[NO_FILES] =
        {
            "S_/etc/hosts",
            "S_/etc/passwd",
            "S_/file1",
            "S_/file2",
        };

    // Should cause migration to happen.
    assert_true(FileChangesGetDirectoryList("/etc", list));
    assert_int_equal(SeqLength(list), 2);
    assert_string_equal(SeqAt(list, 0), "hosts");
    assert_string_equal(SeqAt(list, 1), "passwd");

    SeqClear(list);
    assert_true(FileChangesGetDirectoryList("/", list));
    assert_int_equal(SeqLength(list), 2);
    assert_string_equal(SeqAt(list, 0), "file1");
    assert_string_equal(SeqAt(list, 1), "file2");

    assert_true(OpenDB(&db, dbid_changes));
    for (int c = 0; c < NO_FILES; c++)
    {
        {
            int ksize = 2 + CHANGES_HASH_FILE_NAME_OFFSET
                + strlen(checksum_key[c] + 2 + CHANGES_HASH_FILE_NAME_OFFSET) + 1;
            int vsize = ValueSizeDB(db, checksum_key[c], ksize);
            assert_int_equal(vsize, strlen(CHECKSUM_VALUE[c]) + 1);
            char value[vsize];
            assert_true(ReadComplexKeyDB(db, checksum_key[c], ksize, value, vsize));
            assert_int_equal(memcmp(value, CHECKSUM_VALUE[c], vsize), 0);
        }

        {
            int vsize = ValueSizeDB(db, filestat_key[c], strlen(filestat_key[c]) + 1);
            assert_int_equal(vsize, sizeof(struct stat));
            char value[vsize];
            assert_true(ReadDB(db, filestat_key[c], value, vsize));
            assert_int_equal(memcmp(value, &filestat_value, vsize), 0);
        }
    }

    int db_entries = 0;
    CF_DBC *db_cursor;
    assert_true(NewDBCursor(db, &db_cursor));
    char *key, *value;
    int ksize, vsize;
    while (NextDB(db_cursor, &key, &ksize, (void **)&value, &vsize))
    {
        db_entries++;
    }
    DeleteDBCursor(db_cursor);
    // 2 x Directories ("/" and "/etc")
    // 4 x File hashes
    // 4 x File stats
    assert_int_equal(db_entries, 10);

    CloseDB(db);
}

static void test_teardown(void)
{
    DeleteDirectoryTree(GetWorkDir());
    rmdir(GetWorkDir());
}

int main()
{
    const UnitTest tests[] =
        {
            unit_test(test_setup),
            unit_test(test_migration),
            unit_test(test_teardown),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    return ret;
}
