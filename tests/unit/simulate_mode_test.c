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

#include <test.h>

#include <eval_context.h>       /* SetChangesChroot(), ToChangesChroot() */
#include <changes_chroot.h>     /* CHROOT_PKGS_OPS_FILE */
#include <file_lib.h>           /* DeleteDirectoryTree() */

#include <simulate_mode.h>

static char CHROOT_DIR[] = "/tmp/simulate_mode_test_chroot.XXXXXX";

/* #csv contains "op,name,version,architecture" records terminated by "\r\n",
 * just like the records written by RecordPkgOperationInChroot(). */
static void write_pkgs_ops(const char *csv)
{
    FILE *file = fopen(ToChangesChroot(CHROOT_PKGS_OPS_FILE), "w");
    assert_true(file != NULL);
    assert_true(fputs(csv, file) >= 0);
    fclose(file);
}

/* Both DiffPkgOperations() and ManifestPkgOperations() print their reports
 * with puts(), so capture stdout into #output while calling #fn. */
static bool call_with_captured_stdout(bool (*fn)(void), char *output, size_t output_size)
{
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    assert_true(saved_stdout != -1);

    char out_file[] = "/tmp/simulate_mode_test_out.XXXXXX";
    int out_fd = mkstemp(out_file);
    assert_true(out_fd != -1);
    assert_true(dup2(out_fd, STDOUT_FILENO) != -1);

    const bool ret = fn();

    fflush(stdout);
    assert_true(dup2(saved_stdout, STDOUT_FILENO) != -1);
    close(saved_stdout);

    assert_true(lseek(out_fd, 0, SEEK_SET) != -1);
    const ssize_t n_read = read(out_fd, output, output_size - 1);
    assert_true(n_read >= 0);
    output[n_read] = '\0';
    close(out_fd);
    unlink(out_file);

    return ret;
}

/* A recorded removal followed by a recorded installation of the same package
 * is a net installation, so the removal must not be reported. */
static void test_diff_install_cancels_removal(void)
{
    write_pkgs_ops("r,foo,,\r\n"
                   "i,foo,1.2.3,\r\n");

    char output[4096];
    assert_true(call_with_captured_stdout(&DiffPkgOperations, output, sizeof(output)));

    assert_true(strstr(output, "Package 'foo [1.2.3]' would be installed") != NULL);
    assert_true(strstr(output, "would be removed") == NULL);

    unlink(ToChangesChroot(CHROOT_PKGS_OPS_FILE));
}

static void test_manifest_install_cancels_removal(void)
{
    write_pkgs_ops("r,foo,,\r\n"
                   "i,foo,1.2.3,\r\n");

    char output[4096];
    assert_true(call_with_captured_stdout(&ManifestPkgOperations, output, sizeof(output)));

    assert_true(strstr(output, "Package 'foo [1.2.3]' would be present") != NULL);
    assert_true(strstr(output, "would be absent") == NULL);

    unlink(ToChangesChroot(CHROOT_PKGS_OPS_FILE));
}

int main()
{
    PRINT_TEST_BANNER();

    assert_true(mkdtemp(CHROOT_DIR) != NULL);
    SetChangesChroot(CHROOT_DIR);

    const UnitTest tests[] =
    {
        unit_test(test_diff_install_cancels_removal),
        unit_test(test_manifest_install_cancels_removal),
    };

    int ret = run_tests(tests);

    DeleteDirectoryTree(CHROOT_DIR);
    rmdir(CHROOT_DIR);

    return ret;
}
