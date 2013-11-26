/*
   Copyright (C) CFEngine AS

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
#include <file_lib.h>

#include <bool.h>

#define TEMP_DIR "/tmp"
#define TEST_FILE "file_lib_test.txt"
#define TEST_LINK "file_lib_test.link"
#define TEST_SUBDIR "file_lib_test.sub"
#define TEST_SUBSUBDIR "file_lib_test.sub/sub"
#define TEST_STRING "BLUE balloon"
#define TEST_SUBSTRING "YELLOW balloon"
#define TEST_SUBSUBSTRING "RED balloon"

// These are just a way to pass parameters into test_switch_symlink().
// Since it can be called from CFEngine code, we need to do it like this.
// The way COUNTDOWN works is that it counts down towards zero for each
// component in the path passed to safe_open(). When it reaches zero,
// the symlink will be inserted at that moment.
int TEST_SYMLINK_COUNTDOWN = 0;
const char *TEST_SYMLINK_NAME = "";
const char *TEST_SYMLINK_TARGET = "";
bool TEST_SYMLINK_NONROOT = false;

void test_switch_symlink()
{
    if (--TEST_SYMLINK_COUNTDOWN == 0) {
        rmdir(TEST_SYMLINK_NAME);
        unlink(TEST_SYMLINK_NAME);
        assert_int_equal(symlink(TEST_SYMLINK_TARGET, TEST_SYMLINK_NAME), 0);
        // If we already are root, we must force the link to be non-root,
        // otherwise the test may have no purpose.
        if (TEST_SYMLINK_NONROOT && getuid() == 0)
        {
            // 100 exists in most installations, but it doesn't really matter.
            assert_int_equal(lchown(TEST_SYMLINK_NAME, 100, 100), 0);
        }
    }
}

void chdir_or_exit(const char *path)
{
    if (chdir(path) < 0)
    {
        // Don't risk writing into folders we shouldn't. Just bail.
        exit(1);
    }
}

void setup_tempfiles()
{
    chdir_or_exit(TEMP_DIR);
    mkdir(TEST_SUBDIR, 0755);
    mkdir(TEST_SUBSUBDIR, 0755);
    unlink(TEST_FILE);
    unlink(TEST_LINK);
    unlink(TEST_SUBSUBDIR "/" TEST_FILE);
    unlink(TEST_SUBDIR "/" TEST_FILE);
    int fd = open(TEMP_DIR "/" TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int write_result = write(fd, TEST_STRING, strlen(TEST_STRING));
    close(fd);
    fd = open(TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write_result = write(fd, TEST_SUBSTRING, strlen(TEST_SUBSTRING));
    close(fd);
    fd = open(TEMP_DIR "/" TEST_SUBSUBDIR "/" TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write_result = write(fd, TEST_SUBSUBSTRING, strlen(TEST_SUBSUBSTRING));
    close(fd);

    (void)write_result;
}

void check_contents(int fd, const char *str)
{
    char buf[strlen(str) + 1];
    assert_int_equal(read(fd, buf, strlen(str)), strlen(str));
    buf[strlen(str)] = '\0';
    assert_string_equal(buf, str);
}

void test_safe_open_currentdir()
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);
}

void test_safe_open_subdir()
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_SUBDIR "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);
}

void test_safe_open_subsubdir()
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_SUBSUBDIR "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSUBSTRING);
    close(fd);
}

void test_safe_open_updir()
{
    setup_tempfiles();

    chdir_or_exit(TEST_SUBDIR);

    int fd;
    assert_true((fd = safe_open("../" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);
}

void test_safe_open_upupdir()
{
    setup_tempfiles();

    chdir_or_exit(TEST_SUBSUBDIR);

    int fd;
    assert_true((fd = safe_open("../../" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);
}

void test_safe_open_generic_relative_dir()
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEST_SUBSUBDIR "/../" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);
}

void test_safe_open_generic_absolute_dir()
{
    setup_tempfiles();

    int fd;
    assert_true((fd = safe_open(TEMP_DIR "/"
                                TEST_SUBDIR "/../"
                                TEST_SUBSUBDIR "/../"
                                TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);
}

void test_safe_open_unsafe_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    TEST_SYMLINK_NONROOT = true;
    test_switch_symlink();

    assert_true(safe_open(TEMP_DIR "/" TEST_LINK, O_RDONLY) < 0);
    assert_int_equal(errno, EACCES);
}

void test_safe_open_safe_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_NONROOT = false;
    test_switch_symlink();

    int fd;
    assert_true((fd = safe_open(TEMP_DIR "/" TEST_LINK, O_RDONLY)) >= 0);
    check_contents(fd, TEST_STRING);
    close(fd);
}

void test_safe_open_unsafe_inserted_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    TEST_SYMLINK_NONROOT = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_LINK, O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);
}

void test_safe_open_safe_inserted_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_NONROOT = false;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_LINK, O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);
}

void test_safe_open_unsafe_switched_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    TEST_SYMLINK_NONROOT = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_FILE, O_RDONLY) < 0);
    assert_int_equal(errno, EACCES);
}

void test_safe_open_safe_switched_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 2;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_SUBDIR "/" TEST_FILE;
    TEST_SYMLINK_NONROOT = false;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    int fd;
    assert_true((fd = safe_open(TEMP_DIR "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);
}

void test_safe_open_unsafe_dir_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc";
    TEST_SYMLINK_NONROOT = true;
    test_switch_symlink();

    assert_true(safe_open(TEMP_DIR "/" TEST_LINK "/passwd", O_RDONLY) < 0);
    assert_int_equal(errno, EACCES);
}

void test_safe_open_safe_dir_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEST_SUBDIR;
    TEST_SYMLINK_NONROOT = false;
    test_switch_symlink();

    int fd;
    assert_true((fd = safe_open(TEST_LINK "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);
}

void test_safe_open_unsafe_inserted_dir_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc";
    TEST_SYMLINK_NONROOT = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_LINK "/passwd", O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);
}

void test_safe_open_safe_inserted_dir_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEST_SUBDIR;
    TEST_SYMLINK_NONROOT = false;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_LINK "/" TEST_FILE, O_RDONLY) < 0);
    assert_int_equal(errno, ENOENT);
}

void test_safe_open_unsafe_switched_dir_symlink()
{
    setup_tempfiles();

    assert_int_equal(mkdir(TEMP_DIR "/" TEST_LINK, 0755), 0);

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = "/etc";
    TEST_SYMLINK_NONROOT = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_LINK "/passwd", O_RDONLY) < 0);
    assert_int_equal(errno, EACCES);
}

void test_safe_open_safe_switched_dir_symlink()
{
    setup_tempfiles();

    assert_int_equal(mkdir(TEMP_DIR "/" TEST_LINK, 0755), 0);

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEST_SUBDIR;
    TEST_SYMLINK_NONROOT = false;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    int fd;
    assert_true((fd = safe_open(TEST_LINK "/" TEST_FILE, O_RDONLY)) >= 0);
    check_contents(fd, TEST_SUBSTRING);
    close(fd);
}

void test_safe_open_create_inserted_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_LINK;
    TEST_SYMLINK_TARGET = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_NONROOT = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_LINK, O_RDONLY | O_CREAT, 0644) < 0);
    assert_int_equal(errno, EACCES);
}

void test_safe_open_create_switched_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/passwd";
    TEST_SYMLINK_NONROOT = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_FILE, O_RDONLY | O_CREAT, 0644) < 0);
    assert_int_equal(errno, EACCES);
}

void test_safe_open_dangling_symlink()
{
    setup_tempfiles();

    TEST_SYMLINK_COUNTDOWN = 1;
    TEST_SYMLINK_NAME = TEMP_DIR "/" TEST_FILE;
    TEST_SYMLINK_TARGET = "/etc/file-that-for-sure-does-not-exist";
    TEST_SYMLINK_NONROOT = true;
    // Not calling this function will call it right in the middle of the
    // safe_open() instead.
    //test_switch_symlink();

    assert_true(safe_open(TEST_FILE, O_RDONLY | O_CREAT, 0644) < 0);
    assert_int_equal(errno, EACCES);
}

void test_safe_open_root()
{
    int fd;
    struct stat statbuf;
    assert_true((fd = safe_open("/", O_RDONLY)) >= 0);
    assert_int_equal(fchdir(fd), 0);
    assert_int_equal(stat("etc", &statbuf), 0);
    close(fd);
}

void test_safe_fopen()
{
    setup_tempfiles();

    FILE *fptr;

    char buf = 'a';

    assert_true(fptr = safe_fopen(TEST_FILE, "r"));
    assert_int_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_not_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen(TEST_FILE, "a"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen(TEST_FILE, "r+"));
    assert_int_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen(TEST_FILE, "a+"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen(TEST_FILE, "w"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    assert_true(fptr = safe_fopen(TEST_FILE, "w+"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_false(fptr = safe_fopen(TEST_FILE, "r"));

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen(TEST_FILE, "a"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen(TEST_FILE, "w"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_true(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_false(fptr = safe_fopen(TEST_FILE, "r+"));

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen(TEST_FILE, "a+"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);

    unlink(TEST_FILE);
    assert_true(fptr = safe_fopen(TEST_FILE, "w+"));
    assert_int_not_equal(fread(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    assert_int_equal(fwrite(&buf, 1, 1, fptr), 1);
    assert_false(ferror(fptr));
    clearerr(fptr);
    fclose(fptr);
}

int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
        {
            unit_test(test_safe_open_currentdir),
            unit_test(test_safe_open_subdir),
            unit_test(test_safe_open_subsubdir),
            unit_test(test_safe_open_updir),
            unit_test(test_safe_open_upupdir),
            unit_test(test_safe_open_generic_relative_dir),
            unit_test(test_safe_open_generic_absolute_dir),
            unit_test(test_safe_open_unsafe_symlink),
            unit_test(test_safe_open_safe_symlink),
            unit_test(test_safe_open_unsafe_inserted_symlink),
            unit_test(test_safe_open_safe_inserted_symlink),
            unit_test(test_safe_open_unsafe_switched_symlink),
            unit_test(test_safe_open_safe_switched_symlink),
            unit_test(test_safe_open_unsafe_dir_symlink),
            unit_test(test_safe_open_safe_dir_symlink),
            unit_test(test_safe_open_unsafe_inserted_dir_symlink),
            unit_test(test_safe_open_safe_inserted_dir_symlink),
            unit_test(test_safe_open_unsafe_switched_dir_symlink),
            unit_test(test_safe_open_safe_switched_dir_symlink),
            unit_test(test_safe_open_create_inserted_symlink),
            unit_test(test_safe_open_create_switched_symlink),
            unit_test(test_safe_open_dangling_symlink),
            unit_test(test_safe_open_root),

            unit_test(test_safe_fopen),
        };

    int ret = run_tests(tests);

    return ret;
}
