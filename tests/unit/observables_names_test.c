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

#include <platform.h>
#include <test.h>

#include <definitions.h>           /* CF_BUFSIZE, CF_MAXVARSIZE */
#include <db_structs.h>            /* observables_max, observable_strings */
#include <observables.h>           /* CF_OBSERVABLES, GetObservableNames */
#include <file_lib.h>              /* DeleteDirectoryTree, safe_fopen */
#include <string_lib.h>            /* StringEqual */
#include <misc_lib.h>              /* xsnprintf */

/* cf-check's built-in list ends with a pseudo-entry named "spare", so
 * observables_max is one more than the real named count. */
#define LAST_NAMED_OBSERVABLE (observables_max - 2)
#define FIRST_SPARE_SLOT      (observables_max - 1)

/* Offsets from the first spare slot: the slot the test ts_key names, and the
 * one whose line it corrupts. */
#define CUSTOM_SLOT_OFFSET    8
#define MALFORMED_SLOT_OFFSET 20

static char TESTDIR[CF_BUFSIZE];

/**
 * dump.c indexes and free()s every entry, so a hole is an invalid free, not just
 * a missing name.
 */
static void assert_all_names_present(char **names)
{
    assert_true(names != NULL);
    for (int i = 0; i < CF_OBSERVABLES; i++)
    {
        assert_true(names[i] != NULL);
    }
}

static void free_names(char **names)
{
    for (int i = 0; i < CF_OBSERVABLES; i++)
    {
        free(names[i]);
    }
    free(names);
}

/**
 * Writes a ts_key as Nova_DumpSlots() does. Fewer than CF_OBSERVABLES lines
 * gives the short file an older, smaller-CF_OBSERVABLES agent left behind.
 */
static void write_ts_key(
    char *path, size_t path_size, const char *name, int lines, bool with_malformed)
{
    xsnprintf(path, path_size, "%s/%s", TESTDIR, name);

    FILE *f = safe_fopen(path, "w");
    assert_true(f != NULL);

    for (int i = 0; i < lines; i++)
    {
        if (i < observables_max)
        {
            fprintf(f, "%d,%s,Built-in observable %d\n", i, observable_strings[i], i);
        }
        else if (i == FIRST_SPARE_SLOT + CUSTOM_SLOT_OFFSET)
        {
            fprintf(f, "%d,test_measurement,A test measurement,units,0.000,100.000,1\n", i);
        }
        else if (with_malformed && i == FIRST_SPARE_SLOT + MALFORMED_SLOT_OFFSET)
        {
            fprintf(f, "%d no commas here at all\n", i);
        }
        else
        {
            fprintf(f, "%d,spare,unused\n", i);
        }
    }

    fclose(f);
}

/* Before the fix, named observables came back as "spare[<index>]" and every
 * spare slot shared the name "spare", collapsing the tail into one JSON key. */
static void test_named_slots_report_their_name(void)
{
    char path[CF_BUFSIZE];
    write_ts_key(path, sizeof(path), "ts_key_full", CF_OBSERVABLES, false);
    char **names = GetObservableNames(path);
    assert_all_names_present(names);

    /* Built-ins report the name ts_key gives them. */
    assert_true(StringEqual(names[0], observable_strings[0]));
    assert_true(StringEqual(names[LAST_NAMED_OBSERVABLE],
                            observable_strings[LAST_NAMED_OBSERVABLE]));

    /* A custom measurement reports its name, not a placeholder. */
    const int custom = FIRST_SPARE_SLOT + CUSTOM_SLOT_OFFSET;
    assert_true(StringEqual(names[custom], "test_measurement"));

    /* Nothing keeps the bare "spare" that used to collapse them. */
    for (int i = 0; i < CF_OBSERVABLES; i++)
    {
        assert_false(StringEqual(names[i], "spare"));
    }

    free_names(names);
}

/* The names become JSON keys, so two spares sharing one lose a slot. */
static void test_spare_slots_are_numbered_and_distinct(void)
{
    char path[CF_BUFSIZE];
    write_ts_key(path, sizeof(path), "ts_key_spares", CF_OBSERVABLES, false);
    char **names = GetObservableNames(path);
    assert_all_names_present(names);

    char expected[CF_MAXVARSIZE];
    const int first_spare = FIRST_SPARE_SLOT;
    const int last_spare = CF_OBSERVABLES - 1;

    xsnprintf(expected, sizeof(expected), "spare[%d]", first_spare);
    assert_true(StringEqual(names[first_spare], expected));

    xsnprintf(expected, sizeof(expected), "spare[%d]", last_spare);
    assert_true(StringEqual(names[last_spare], expected));

    assert_false(StringEqual(names[first_spare], names[last_spare]));

    free_names(names);
}

/* sscanf() may leave name untouched, so a bad line must not be read as a name. */
static void test_malformed_line_falls_back_to_numbered_spare(void)
{
    char path[CF_BUFSIZE];
    write_ts_key(path, sizeof(path), "ts_key_malformed", CF_OBSERVABLES, true);
    char **names = GetObservableNames(path);
    assert_all_names_present(names);

    const int bad = FIRST_SPARE_SLOT + MALFORMED_SLOT_OFFSET;
    char expected[CF_MAXVARSIZE];
    xsnprintf(expected, sizeof(expected), "spare[%d]", bad);
    assert_true(StringEqual(names[bad], expected));

    free_names(names);
}

/* A short ts_key must still fill every slot, or the caller free()s garbage. */
static void test_short_ts_key_fills_the_remaining_slots(void)
{
    const int short_lines = 100;
    char path[CF_BUFSIZE];
    write_ts_key(path, sizeof(path), "ts_key_short", short_lines, false);
    char **names = GetObservableNames(path);
    assert_all_names_present(names);

    char expected[CF_MAXVARSIZE];
    xsnprintf(expected, sizeof(expected), "spare[%d]", short_lines);
    assert_true(StringEqual(names[short_lines], expected));

    xsnprintf(expected, sizeof(expected), "spare[%d]", CF_OBSERVABLES - 1);
    assert_true(StringEqual(names[CF_OBSERVABLES - 1], expected));

    free_names(names);
}

/* With no ts_key, names come from the built-in table. */
static void test_missing_ts_key_uses_the_builtin_table(void)
{
    char path[CF_BUFSIZE];
    xsnprintf(path, CF_BUFSIZE, "%s/does_not_exist", TESTDIR);

    char **names = GetObservableNames(path);
    assert_all_names_present(names);

    assert_true(StringEqual(names[0], observable_strings[0]));

    char expected[CF_MAXVARSIZE];
    xsnprintf(expected, sizeof(expected), "observable[%d]", CF_OBSERVABLES - 1);
    assert_true(StringEqual(names[CF_OBSERVABLES - 1], expected));

    free_names(names);
}

int main(void)
{
    char template[] = "/tmp/observables_names_test.XXXXXX";
    assert_true(mkdtemp(template) != NULL);
    strlcpy(TESTDIR, template, CF_BUFSIZE);

    const UnitTest tests[] =
        {
            unit_test(test_named_slots_report_their_name),
            unit_test(test_spare_slots_are_numbered_and_distinct),
            unit_test(test_malformed_line_falls_back_to_numbered_spare),
            unit_test(test_short_ts_key_fills_the_remaining_slots),
            unit_test(test_missing_ts_key_uses_the_builtin_table),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    /* DeleteDirectoryTree() keeps the top directory, so rmdir() it too. */
    DeleteDirectoryTree(TESTDIR);
    rmdir(TESTDIR);

    return ret;
}
