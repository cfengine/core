/*
  Copyright 2026 Northern.tech AS

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

#include <stdlib.h>
#include <stddef.h>                                            /* offsetof */
#include <sys/stat.h>
#include <test.h>
#include <known_dirs.h>

#include <cf3.defs.h>
#include <dbm_api.h>
#include <monitoring_read.h>
#include <file_lib.h>                                  /* DeleteDirectoryTree */
#include <misc_lib.h>                                          /* xsnprintf */

/* The custom slot this test registers, offset from the first spare slot. */
#define TEST_CUSTOM_SLOT_OFFSET 9

/* Stale read-buffer contents, distinct from zero and from any stored value. */
#define STALE 12345.0

char CFWORKDIR[CF_BUFSIZE];

/* The size of a record holding slots 0..last_slot inclusive. */
static size_t size_through_slot(int last_slot)
{
    return offsetof(Averages, Q) + (size_t) (last_slot + 1) * sizeof(QPoint);
}

void tests_setup(void)
{
    static char env[] = /* Needs to be static for putenv() */
        "CFENGINE_TEST_OVERRIDE_WORKDIR=/tmp/observations_storage_test.XXXXXX";

    char *workdir = strchr(env, '=') + 1; /* start of the path */
    assert(workdir - 1 && workdir[0] == '/');

    mkdtemp(workdir);
    strlcpy(CFWORKDIR, workdir, CF_BUFSIZE);
    putenv(env);
    mkdir(GetStateDir(), (S_IRWXU | S_IRWXG | S_IRWXO));
}

static void tests_teardown(void)
{
    /* DeleteDirectoryTree() keeps the top directory, so rmdir() it too. */
    DeleteDirectoryTree(CFWORKDIR);
    rmdir(CFWORKDIR);
}

/**
 * Writes a ts_key as Nova_DumpSlots() does, registering one custom measurement
 * so the in-use range reaches past the built-in observables.
 */
static void write_ts_key_with_one_custom_slot(void)
{
    char filename[CF_BUFSIZE];
    xsnprintf(filename, CF_BUFSIZE, "%s%cts_key", GetStateDir(), FILE_SEPARATOR);

    FILE *f = safe_fopen(filename, "w");
    assert_true(f != NULL);

    for (int i = 0; i < CF_OBSERVABLES; i++)
    {
        if (i == ob_spare + TEST_CUSTOM_SLOT_OFFSET)
        {
            fprintf(f, "%d,test_measurement,A test measurement,units,0.000,100.000,1\n", i);
        }
        else
        {
            fprintf(f, "%d,spare,unused\n", i);
        }
    }

    fclose(f);
}

static void test_used_size_without_custom_slots(void)
{
    /* No ts_key yet, so only the built-in observables are in use. */
    assert_int_equal(AveragesUsedSize(), size_through_slot(ob_spare - 1));

    /* The point of the change: less than a full-size record. */
    assert_true(AveragesUsedSize() < sizeof(Averages));
}

static void test_used_size_covers_highest_registered_slot(void)
{
    write_ts_key_with_one_custom_slot();

    /* Through the registered slot and no further. */
    assert_int_equal(
        AveragesUsedSize(),
        size_through_slot(ob_spare + TEST_CUSTOM_SLOT_OFFSET));
    assert_true(AveragesUsedSize() < sizeof(Averages));
}

static void test_short_record_reads_back_zero_extended(void)
{
    /* Stored slots must read back intact and the unstored tail as zero, even
     * when the caller's buffer held something else first. */
    const time_t when = 1234567890;
    const int custom_slot = ob_spare + TEST_CUSTOM_SLOT_OFFSET;
    const size_t used = AveragesUsedSize();

    CF_DB *db;
    assert_true(OpenDB(&db, dbid_observations));

    Averages written;
    memset(&written, 0, sizeof(written));
    written.last_seen = when;
    written.Q[0].q = 1.0;
    written.Q[0].expect = 2.0;
    written.Q[0].var = 3.0;
    written.Q[0].dq = 4.0;
    written.Q[custom_slot].q = 5.0;
    written.Q[custom_slot].dq = 6.0;

    char timekey[CF_MAXVARSIZE];
    MakeTimekey(when, timekey);
    assert_true(WriteDB(db, timekey, &written, used));

    /* Stale doubles, not a memset byte pattern: 0xAA-filled bytes read back as
     * ~1e-103, which assert_double_close() cannot tell from zero. */
    Averages read_back;
    read_back.last_seen = -1;
    for (int i = 0; i < CF_OBSERVABLES; i++)
    {
        read_back.Q[i].q = STALE;
        read_back.Q[i].expect = STALE;
        read_back.Q[i].var = STALE;
        read_back.Q[i].dq = STALE;
    }
    assert_true(GetRecordForTime(db, when, &read_back));

    assert_int_equal(read_back.last_seen, when);
    assert_double_close(read_back.Q[0].q, 1.0);
    assert_double_close(read_back.Q[0].expect, 2.0);
    assert_double_close(read_back.Q[0].var, 3.0);
    assert_double_close(read_back.Q[0].dq, 4.0);
    assert_double_close(read_back.Q[custom_slot].q, 5.0);
    assert_double_close(read_back.Q[custom_slot].dq, 6.0);

    /* Never stored. */
    assert_double_close(read_back.Q[custom_slot + 1].q, 0.0);
    assert_double_close(read_back.Q[CF_OBSERVABLES - 1].q, 0.0);
    assert_double_close(read_back.Q[CF_OBSERVABLES - 1].dq, 0.0);

    CloseDB(db);
}

int main(void)
{
    tests_setup();

    /* Order matters: the first test needs no ts_key, the rest need the custom
     * slot the second one registers. */
    const UnitTest tests[] =
        {
            unit_test(test_used_size_without_custom_slots),
            unit_test(test_used_size_covers_highest_registered_slot),
            unit_test(test_short_record_reads_back_zero_extended),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    tests_teardown();
    return ret;
}
