/*
  Copyright 2024 Northern.tech AS

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

#include <test.h>

#include <eval_context.h>       /* SetChangesChroot(), ToChangesChroot() */
#include <changes_chroot.h>     /* CHROOT_CHANGES_LIST_FILE */
#include <json.h>               /* JsonParseFile() */
#include <writer.h>             /* FileWriter() */
#include <string_sequence.h>    /* WriteLenPrefixedString() */
#include <files_lib.h>          /* MakeParentDirectory() */
#include <file_lib.h>           /* DeleteDirectoryTree() */
#include <misc_lib.h>           /* xsnprintf() */

#include <simulate_mode.h>

static char CHROOT_DIR[] = "/tmp/simulate_mode_test_chroot.XXXXXX";
static char ORIG_DIR[] = "/tmp/simulate_mode_test_orig.XXXXXX";
static char OUTPUT_FILE[PATH_MAX];

static void test_setup(void)
{
    assert_true(mkdtemp(CHROOT_DIR) != NULL);
    assert_true(mkdtemp(ORIG_DIR) != NULL);
    xsnprintf(OUTPUT_FILE, sizeof(OUTPUT_FILE), "%s/change_set.json",
              ORIG_DIR);

    SetChangesChroot(CHROOT_DIR);
    EVAL_MODE = EVAL_MODE_SIMULATE_MANIFEST;
}

static void test_teardown(void)
{
    assert_true(DeleteDirectoryTree(CHROOT_DIR));
    rmdir(CHROOT_DIR);
    assert_true(DeleteDirectoryTree(ORIG_DIR));
    rmdir(ORIG_DIR);
}

/* All tests share the one changes chroot (SetChangesChroot() can only be
 * called once), so each test starts by removing the record files and the
 * output written by the previous one. */
static void reset_records(void)
{
    unlink(ToChangesChroot(CHROOT_CHANGES_LIST_FILE));
    unlink(ToChangesChroot(CHROOT_RENAMES_LIST_FILE));
    unlink(ToChangesChroot(CHROOT_PKGS_OPS_FILE));
    unlink(OUTPUT_FILE);
}

static void write_changed_files(const char *const paths[], size_t n)
{
    FILE *file = fopen(ToChangesChroot(CHROOT_CHANGES_LIST_FILE), "w");
    assert_true(file != NULL);
    Writer *writer = FileWriter(file);
    for (size_t i = 0; i < n; i++)
    {
        assert_true(WriteLenPrefixedString(writer, paths[i]));
    }
    WriterClose(writer);
}

/* #names contains consecutive pairs of the original and the new name, just
 * like the records written by RecordFileRenamedInChroot(). */
static void write_renamed_files(const char *const names[], size_t n)
{
    FILE *file = fopen(ToChangesChroot(CHROOT_RENAMES_LIST_FILE), "w");
    assert_true(file != NULL);
    Writer *writer = FileWriter(file);
    for (size_t i = 0; i < n; i++)
    {
        assert_true(WriteLenPrefixedString(writer, names[i]));
    }
    WriterClose(writer);
}

/* #csv contains "op,name,version,architecture" records terminated by "\r\n",
 * just like the records written by RecordPkgOperationInChroot(). */
static void write_pkgs_ops(const char *csv)
{
    FILE *file = fopen(ToChangesChroot(CHROOT_PKGS_OPS_FILE), "w");
    assert_true(file != NULL);
    assert_true(fputs(csv, file) >= 0);
    fclose(file);
}

static void create_chroot_file(const char *path, const char *content,
                               mode_t mode)
{
    char chrooted[PATH_MAX];
    strlcpy(chrooted, ToChangesChroot(path), sizeof(chrooted));

    /* MakeParentDirectory() maps the path into the changes chroot on its own
     * (EVAL_MODE is one of the simulate modes here). */
    assert_true(MakeParentDirectory(path, true, NULL));

    FILE *file = fopen(chrooted, "w");
    assert_true(file != NULL);
    assert_true(fputs(content, file) >= 0);
    fclose(file);

    assert_int_equal(chmod(chrooted, mode), 0);
}

static JsonElement *WriteAndParseChanges(void)
{
    assert_true(WriteChangesJson(OUTPUT_FILE));

    JsonElement *json = NULL;
    assert_int_equal(JsonParseFile(OUTPUT_FILE, 1024 * 1024, &json),
                     JSON_PARSE_OK);
    assert_true(json != NULL);
    return json;
}

/* Reads the raw bytes of the output file. The encoding tests below need
 * them because JsonParseFile() decodes a "\u00XX" escape into the raw byte
 * 0xXX, so it cannot tell a (wrongly) escaped byte from a raw one. */
static void read_output_file_raw(char *buf, size_t buf_size)
{
    FILE *file = fopen(OUTPUT_FILE, "r");
    assert_true(file != NULL);
    size_t n_read = fread(buf, 1, buf_size - 1, file);
    fclose(file);
    assert_true(n_read > 0);
    buf[n_read] = '\0';
}

static void test_empty_change_set(void)
{
    reset_records();

    JsonElement *json = WriteAndParseChanges();

    assert_int_equal(
        JsonPrimitiveGetAsInteger(JsonObjectGet(json, "format_version")), 1);
    assert_string_equal(JsonObjectGetAsString(json, "simulate_mode"),
                        "manifest");
    assert_int_equal(JsonLength(JsonObjectGetAsArray(json, "files")), 0);
    assert_int_equal(JsonLength(JsonObjectGetAsArray(json, "renames")), 0);
    assert_int_equal(JsonLength(JsonObjectGetAsArray(json, "packages")), 0);

    JsonDestroy(json);
}

static void test_simulate_mode_string(void)
{
    reset_records();

    EVAL_MODE = EVAL_MODE_SIMULATE_DIFF;
    JsonElement *json = WriteAndParseChanges();
    assert_string_equal(JsonObjectGetAsString(json, "simulate_mode"), "diff");
    JsonDestroy(json);
    EVAL_MODE = EVAL_MODE_SIMULATE_MANIFEST;
}

static void test_created_file(void)
{
    reset_records();

    const char *const path = "/simulate-test/created-file";
    create_chroot_file(path, "Hello, CFEngine!\n", 0640);
    write_changed_files(&path, 1);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *files = JsonObjectGetAsArray(json, "files");
    assert_int_equal(JsonLength(files), 1);

    struct stat st;
    assert_int_equal(lstat(ToChangesChroot(path), &st), 0);

    JsonElement *file_info = JsonArrayGetAsObject(files, 0);
    assert_string_equal(JsonObjectGetAsString(file_info, "path"), path);
    assert_string_equal(JsonObjectGetAsString(file_info, "change"), "created");
    assert_string_equal(JsonObjectGetAsString(file_info, "type"),
                        "regular file");
    assert_string_equal(JsonObjectGetAsString(file_info, "permissions"),
                        "0640");
    assert_int_equal(
        JsonPrimitiveGetAsInteger(JsonObjectGet(file_info, "uid")),
        (long) st.st_uid);
    assert_int_equal(
        JsonPrimitiveGetAsInteger(JsonObjectGet(file_info, "gid")),
        (long) st.st_gid);
    assert_int_equal(
        JsonPrimitiveGetAsInteger(JsonObjectGet(file_info, "size")),
        (long) strlen("Hello, CFEngine!\n"));
    assert_string_equal(
        JsonObjectGetAsString(file_info, "sha256"),
        "9be7023e1f91bae9d1f734b49c579cc2091c71924ae7494c9a5a3a8006527615");

    JsonDestroy(json);
}

static void test_deleted_file(void)
{
    reset_records();

    /* Recorded as changed, but neither the file nor its in-chroot copy
     * exists. */
    const char *const path = "/simulate-test/deleted-file";
    write_changed_files(&path, 1);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *files = JsonObjectGetAsArray(json, "files");
    assert_int_equal(JsonLength(files), 1);

    JsonElement *file_info = JsonArrayGetAsObject(files, 0);
    assert_string_equal(JsonObjectGetAsString(file_info, "path"), path);
    assert_string_equal(JsonObjectGetAsString(file_info, "change"), "deleted");

    /* Nothing to describe when the file would no longer exist. */
    assert_true(JsonObjectGet(file_info, "type") == NULL);
    assert_true(JsonObjectGet(file_info, "size") == NULL);
    assert_true(JsonObjectGet(file_info, "sha256") == NULL);

    JsonDestroy(json);
}

static void test_modified_file(void)
{
    reset_records();

    char path[PATH_MAX];
    xsnprintf(path, sizeof(path), "%s/modified-file", ORIG_DIR);

    FILE *file = fopen(path, "w");
    assert_true(file != NULL);
    assert_true(fputs("contents before the run\n", file) >= 0);
    fclose(file);

    create_chroot_file(path, "new contents after the run\n", 0644);
    const char *paths[] = { path };
    write_changed_files(paths, 1);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *files = JsonObjectGetAsArray(json, "files");
    assert_int_equal(JsonLength(files), 1);

    JsonElement *file_info = JsonArrayGetAsObject(files, 0);
    assert_string_equal(JsonObjectGetAsString(file_info, "path"), path);
    assert_string_equal(JsonObjectGetAsString(file_info, "change"),
                        "modified");

    /* The digest must be of the would-be contents, not the current ones. */
    assert_string_equal(
        JsonObjectGetAsString(file_info, "sha256"),
        "6ba024f3c03f13f9a8c1bb444829640c68553009facb418bb4552dd7ddebe427");

    JsonDestroy(json);
}

static void test_duplicate_records(void)
{
    reset_records();

    /* Files changed multiple times during a run are recorded multiple times,
     * but must only be reported once. */
    const char *const path = "/simulate-test/created-file";
    create_chroot_file(path, "Hello, CFEngine!\n", 0640);
    const char *const paths[] = { path, path, path };
    write_changed_files(paths, 3);

    JsonElement *json = WriteAndParseChanges();
    assert_int_equal(JsonLength(JsonObjectGetAsArray(json, "files")), 1);

    JsonDestroy(json);
}

static void test_special_characters_in_path(void)
{
    reset_records();

    const char *const path = "/simulate-test/w\xC3\xA9" "ird \"file\" \\ name";
    create_chroot_file(path, "", 0600);
    write_changed_files(&path, 1);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *files = JsonObjectGetAsArray(json, "files");
    assert_int_equal(JsonLength(files), 1);

    /* The path must survive JSON escaping and parsing untouched. */
    JsonElement *file_info = JsonArrayGetAsObject(files, 0);
    assert_string_equal(JsonObjectGetAsString(file_info, "path"), path);
    assert_string_equal(JsonObjectGetAsString(file_info, "change"), "created");

    JsonDestroy(json);

    /* The UTF-8 character must appear in the document as its raw bytes --
     * as per-byte "\u00XX" escapes, a conformant JSON parser would decode
     * it as two wrong characters (the parser above reverses such escapes,
     * so it cannot detect them). */
    char raw[4096];
    read_output_file_raw(raw, sizeof(raw));
    assert_true(strstr(raw, "w\xC3\xA9" "ird") != NULL);
    assert_true(strstr(raw, "\\u00c3") == NULL);
}

static void test_invalid_utf8_in_path(void)
{
    reset_records();

    /* File names are not guaranteed to be valid UTF-8. A byte that is not
     * part of a valid UTF-8 sequence has to stay escaped as "\u00XX" --
     * raw, it would make the whole document invalid UTF-8. No chroot file
     * is created here (the file system may refuse such a name), so the
     * file is reported as deleted, which is enough to get the path into
     * the document. */
    const char *const path = "/simulate-test/latin1-\xE9-name";
    write_changed_files(&path, 1);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *files = JsonObjectGetAsArray(json, "files");
    assert_int_equal(JsonLength(files), 1);
    assert_string_equal(
        JsonObjectGetAsString(JsonArrayGetAsObject(files, 0), "path"), path);
    JsonDestroy(json);

    char raw[4096];
    read_output_file_raw(raw, sizeof(raw));
    assert_true(strstr(raw, "\\u00e9") != NULL);
}

static void test_write_failure(void)
{
    reset_records();

    /* A failed write must be reported to the caller (the agent then exits
     * non-zero) -- a consumer must not see a successful run without the
     * document it asked for. */
    char bad_output[PATH_MAX];
    xsnprintf(bad_output, sizeof(bad_output), "%s/no-such-dir/change_set.json",
              ORIG_DIR);
    assert_false(WriteChangesJson(bad_output));
}

static void test_overwrite_output_file(void)
{
    reset_records();

    /* An existing output file is replaced by the new document. */
    FILE *file = fopen(OUTPUT_FILE, "w");
    assert_true(file != NULL);
    assert_true(fputs("not a JSON document", file) >= 0);
    fclose(file);

    JsonElement *json = WriteAndParseChanges();
    JsonDestroy(json);
}

#ifndef __MINGW32__
static void test_output_symlink_not_followed(void)
{
    reset_records();

    /* If the output path is a symbolic link, the document must replace the
     * link instead of being written through it. */
    char link_target[PATH_MAX];
    xsnprintf(link_target, sizeof(link_target), "%s/link-target", ORIG_DIR);
    FILE *file = fopen(link_target, "w");
    assert_true(file != NULL);
    assert_true(fputs("do not overwrite\n", file) >= 0);
    fclose(file);
    assert_int_equal(symlink(link_target, OUTPUT_FILE), 0);

    JsonElement *json = WriteAndParseChanges();
    JsonDestroy(json);

    /* The output file is a regular file now... */
    struct stat st;
    assert_int_equal(lstat(OUTPUT_FILE, &st), 0);
    assert_true(S_ISREG(st.st_mode));

    /* ...and the former link target is untouched. */
    char buf[64] = {0};
    file = fopen(link_target, "r");
    assert_true(file != NULL);
    assert_true(fread(buf, 1, sizeof(buf) - 1, file) > 0);
    fclose(file);
    assert_string_equal(buf, "do not overwrite\n");
    unlink(link_target);
}
#endif  /* !__MINGW32__ */

#ifndef __MINGW32__
static void test_created_symlink(void)
{
    reset_records();

    const char *const target = "/simulate-test/link-target";
    const char *const path = "/simulate-test/created-link";
    create_chroot_file(target, "target contents\n", 0644);

    /* Links created in the chroot point to the chrooted target paths (they
     * are created as if the chroot was the root of the file system). */
    char chrooted_target[PATH_MAX];
    strlcpy(chrooted_target, ToChangesChroot(target), sizeof(chrooted_target));
    char chrooted_link[PATH_MAX];
    strlcpy(chrooted_link, ToChangesChroot(path), sizeof(chrooted_link));
    assert_int_equal(symlink(chrooted_target, chrooted_link), 0);

    write_changed_files(&path, 1);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *files = JsonObjectGetAsArray(json, "files");
    assert_int_equal(JsonLength(files), 1);

    JsonElement *file_info = JsonArrayGetAsObject(files, 0);
    assert_string_equal(JsonObjectGetAsString(file_info, "change"), "created");
    assert_string_equal(JsonObjectGetAsString(file_info, "type"),
                        "symbolic link");

    /* The target must be reported as a path outside of the chroot. */
    assert_string_equal(JsonObjectGetAsString(file_info, "target"), target);

    JsonDestroy(json);
}
#endif  /* !__MINGW32__ */

static void test_renamed_files(void)
{
    reset_records();

    const char *const names[] = {
        "/simulate-test/old-name", "/simulate-test/new-name",
        "/simulate-test/old-name-2", "/simulate-test/new-name-2",
    };
    write_renamed_files(names, 4);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *renames = JsonObjectGetAsArray(json, "renames");
    assert_int_equal(JsonLength(renames), 2);

    JsonElement *rename = JsonArrayGetAsObject(renames, 0);
    assert_string_equal(JsonObjectGetAsString(rename, "old_name"), names[0]);
    assert_string_equal(JsonObjectGetAsString(rename, "new_name"), names[1]);

    rename = JsonArrayGetAsObject(renames, 1);
    assert_string_equal(JsonObjectGetAsString(rename, "old_name"), names[2]);
    assert_string_equal(JsonObjectGetAsString(rename, "new_name"), names[3]);

    JsonDestroy(json);
}

static void check_single_pkg_operation(const char *csv, const char *operation,
                                       const char *name, const char *version,
                                       const char *architecture)
{
    reset_records();
    write_pkgs_ops(csv);

    JsonElement *json = WriteAndParseChanges();
    JsonElement *packages = JsonObjectGetAsArray(json, "packages");
    assert_int_equal(JsonLength(packages), 1);

    JsonElement *op_info = JsonArrayGetAsObject(packages, 0);
    assert_string_equal(JsonObjectGetAsString(op_info, "operation"),
                        operation);
    assert_string_equal(JsonObjectGetAsString(op_info, "name"), name);
    if (version != NULL)
    {
        assert_string_equal(JsonObjectGetAsString(op_info, "version"),
                            version);
    }
    else
    {
        assert_true(JsonObjectGet(op_info, "version") == NULL);
    }
    if (architecture != NULL)
    {
        assert_string_equal(JsonObjectGetAsString(op_info, "architecture"),
                            architecture);
    }
    else
    {
        assert_true(JsonObjectGet(op_info, "architecture") == NULL);
    }

    JsonDestroy(json);
}

static void check_empty_pkg_operations(const char *csv)
{
    reset_records();
    write_pkgs_ops(csv);

    JsonElement *json = WriteAndParseChanges();
    assert_int_equal(JsonLength(JsonObjectGetAsArray(json, "packages")), 0);
    JsonDestroy(json);
}

static void test_pkg_operations(void)
{
    check_single_pkg_operation("i,pkg,1.0,x86_64\r\n",
                               "install", "pkg", "1.0", "x86_64");
    check_single_pkg_operation("r,pkg,,\r\n",
                               "remove", "pkg", NULL, NULL);

    /* A newer version of the same package wins. */
    check_single_pkg_operation("i,pkg,1.0,\r\ni,pkg,2.0,\r\n",
                               "install", "pkg", "2.0", NULL);

    /* An 'absent' operation with a version that doesn't match the version
     * that would be installed would fail to remove the package. */
    check_single_pkg_operation("i,pkg,2.0,\r\na,pkg,1.0,\r\n",
                               "install", "pkg", "2.0", NULL);
}

static void test_pkg_operations_cancel(void)
{
    /* A 'present' operation after a 'remove' operation means the package
     * would be installed back, so there is no net change. */
    check_empty_pkg_operations("r,pkg,,\r\np,pkg,,\r\n");

    /* An 'absent' operation with a matching (or no) version after an
     * 'install' operation cancels the installation. */
    check_empty_pkg_operations("i,pkg,2.0,\r\na,pkg,2.0,\r\n");
    check_empty_pkg_operations("i,pkg,2.0,\r\na,pkg,,\r\n");

    /* An 'install' operation after a 'remove' operation cancels the
     * removal -- the net result is just the installation. */
    check_single_pkg_operation("r,pkg,,\r\ni,pkg,1.0,\r\n",
                               "install", "pkg", "1.0", NULL);
}

int main()
{
    const UnitTest tests[] =
        {
            unit_test(test_setup),
            unit_test(test_empty_change_set),
            unit_test(test_simulate_mode_string),
            unit_test(test_created_file),
            unit_test(test_deleted_file),
            unit_test(test_modified_file),
            unit_test(test_duplicate_records),
            unit_test(test_special_characters_in_path),
            unit_test(test_invalid_utf8_in_path),
#ifndef __MINGW32__
            unit_test(test_created_symlink),
#endif
            unit_test(test_renamed_files),
            unit_test(test_pkg_operations),
            unit_test(test_pkg_operations_cancel),
            unit_test(test_write_failure),
            unit_test(test_overwrite_output_file),
#ifndef __MINGW32__
            unit_test(test_output_symlink_not_followed),
#endif
            unit_test(test_teardown),
        };

    PRINT_TEST_BANNER();
    int ret = run_tests(tests);

    return ret;
}
