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

#include <files_lib.h>                                         /* FullWrite */
#include <misc_lib.h>                                          /* xsnprintf */


/* CopyRegularFileDisk() is the function we are testing. */
#include <files_copy.h>


/* WARNING on Solaris 11 with ZFS, stat.st_nblocks==1 if you check the file
 * right after closing it, and it changes to the right value (i.e. how many
 * 512 chunks the file has) a few seconds later!!! So we force a sync() on
 * all tests to avoid this! */
bool do_sync = true;
// TODO detect in runtime if the filesystem needs to be synced!

#define MAYBE_SYNC_NOW    if (do_sync) sync()


char TEST_DIR[] = "/tmp/files_copy_test-XXXXXX";
char TEST_SUBDIR[256];
char TEST_SRC_FILE[256];
char TEST_DST_FILE[256];
#define TEST_FILENAME "testfile"

/* Size must be enough to contain several disk blocks. */
int blk_size;                                      /* defined during init() */
#define TESTFILE_SIZE (blk_size * 8)
#define SHORT_REGION  (blk_size / 2)                /* not enough for hole */
#define LONG_REGION   (blk_size * 2)                /* hole for sure */
#define GARBAGE     "blahblehblohbluebloblebli"
#define GARBAGE_LEN (sizeof(GARBAGE) - 1)


/* Notice if even one test failed so that we don't clean up. */
#define NTESTS 8
bool test_has_run[NTESTS + 1];
bool success     [NTESTS + 1];

static void init(void)
{
    LogSetGlobalLevel(LOG_LEVEL_DEBUG);

    char *ok = mkdtemp(TEST_DIR);
    assert_int_not_equal(ok, NULL);

    /* Set blk_size */
    struct stat statbuf;
    int ret1 = stat(TEST_DIR, &statbuf);
    assert_int_not_equal(ret1, -1);

    blk_size = ST_BLKSIZE(statbuf);
    Log(LOG_LEVEL_NOTICE,
        "Running sparse file tests with blocksize=%d TESTFILE_SIZE=%d",
        blk_size, TESTFILE_SIZE);
    Log(LOG_LEVEL_NOTICE, "Temporary directory: %s", TEST_DIR);

    // /tmp/files_copy_test-XXXXXX/subdir
    xsnprintf(TEST_SUBDIR, sizeof(TEST_SUBDIR), "%s/%s",
              TEST_DIR, "subdir");
    // /tmp/files_copy_test-XXXXXX/testfile
    xsnprintf(TEST_SRC_FILE, sizeof(TEST_SRC_FILE), "%s/%s",
              TEST_DIR, TEST_FILENAME);
    // /tmp/files_copy_test-XXXXXX/subdir/testfile
    xsnprintf(TEST_DST_FILE, sizeof(TEST_DST_FILE), "%s/%s",
              TEST_SUBDIR, TEST_FILENAME);

    int ret2 = mkdir(TEST_SUBDIR, 0700);
    assert_int_equal(ret2, 0);

    test_has_run[0] = true;
    success[0]      = true;
}

static void finalise(void)
{
    /* Do not remove evidence if even one test has failed. */
    bool all_success = true;
    for (int i = 0; i < NTESTS; i++)
    {
        if (test_has_run[i])
        {
            all_success = all_success && success[i];
        }
    }
    if (!all_success)
    {
        Log(LOG_LEVEL_NOTICE,
            "Skipping cleanup of test data because of tests failing");
        return;
    }

    int ret1 = unlink(TEST_DST_FILE);
    assert_int_equal(ret1, 0);
    int ret2 = rmdir(TEST_SUBDIR);
    assert_int_equal(ret2, 0);
    int ret3 = unlink(TEST_SRC_FILE);
    assert_int_equal(ret3, 0);
    int ret5 = rmdir(TEST_DIR);
    assert_int_equal(ret5, 0);
}


static void FillBufferWithGarbage(char *buf, size_t buf_size)
{
    for (size_t i = 0; i < TESTFILE_SIZE; i += GARBAGE_LEN)
    {
        memcpy(&buf[i], GARBAGE,
               MIN(buf_size - i, GARBAGE_LEN));
    }
}

/* Fill a buffer with non-NULL garbage. */
static void WriteBufferToFile(const char *name, const void *buf, size_t count)
{
    int fd = open(name, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 0700);
    assert_int_not_equal(fd, -1);

    ssize_t written = FullWrite(fd, buf, count);
    assert_int_equal(written, count);

    int close_ret = close(fd);
    assert_int_not_equal(close_ret, -1);
}

static bool CompareFileToBuffer(const char *filename,
                                const void *buf, size_t buf_size)
{
    FILE *f = fopen(filename, "rb");
    assert_int_not_equal(f, NULL);

    size_t total = 0;
    char filebuf[DEV_BSIZE];
    size_t n;
    while ((n = fread(filebuf, 1, sizeof(filebuf), f)) != 0)
    {
        bool differ = (total + n > buf_size);

        differ = differ || (memcmp(filebuf, buf + total, n) != 0);

        if (differ)
        {
            Log(LOG_LEVEL_DEBUG,
                "file differs from buffer at pos %zu len %zu",
                total, n);
            fclose(f);
            return false;
        }

        total += n;
    }

    bool error_happened = (ferror(f) != 0);
    assert_false(error_happened);

    if (total != buf_size)
    {
        Log(LOG_LEVEL_DEBUG, "filesize:%zu buffersize:%zu ==> differ",
            total, buf_size);
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

static bool FileIsSparse(const char *filename)
{
    MAYBE_SYNC_NOW;

    struct stat statbuf;
    int ret = stat(filename, &statbuf);
    assert_int_not_equal(ret, -1);

    Log(LOG_LEVEL_DEBUG,
        " st_size=%ju ST_NBLOCKS=%ju ST_BLKSIZE=%ju DEV_BSIZE=%ju",
        (uint64_t) statbuf.st_size,
        (uint64_t) ST_NBLOCKS(statbuf), (uint64_t) ST_BLKSIZE(statbuf),
        (uint64_t) DEV_BSIZE);

    if (statbuf.st_size <= ST_NBLOCKS(statbuf) * DEV_BSIZE)
    {
        Log(LOG_LEVEL_DEBUG, "File is probably non-sparse");
        return false;
    }
    else
    {
        /* We definitely know the file is sparse, since the allocated bytes
         * are less than the real size. */
        Log(LOG_LEVEL_DEBUG, "File is definitely sparse");
        return true;
    }
}

const char *srcfile = TEST_SRC_FILE;
const char *dstfile = TEST_DST_FILE;

static void test_sparse_files_1(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "No zeros in the file, the output file must be non-sparse");

    char *buf = xmalloc(TESTFILE_SIZE);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_false(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE);
    assert_true(data_ok);

    free(buf);
    test_has_run[1] = true;
    success     [1] = true;
}

static void test_sparse_files_2(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "File starting with few zeroes, the output file must be non-sparse.");

    char *buf = xmalloc(TESTFILE_SIZE);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);
    memset(buf, 0, SHORT_REGION);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_false(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE);
    assert_true(data_ok);

    free(buf);
    test_has_run[2] = true;
    success     [2] = true;
}

static void test_sparse_files_3(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "File with few zeroes in the middle, the output file must be non-sparse");

    char *buf = xmalloc(TESTFILE_SIZE);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);
    memset(&buf[TESTFILE_SIZE / 2], 0, SHORT_REGION);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_false(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE);
    assert_true(data_ok);

    free(buf);
    test_has_run[3] = true;
    success     [3] = true;
}

static void test_sparse_files_4(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "File ending with few zeroes, the output file must be non-sparse");

    char *buf = xmalloc(TESTFILE_SIZE);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);
    memset(&buf[TESTFILE_SIZE - SHORT_REGION], 0, SHORT_REGION);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_false(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE);
    assert_true(data_ok);

    free(buf);
    test_has_run[4] = true;
    success     [4] = true;
}

static void test_sparse_files_5(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "File starting with many zeroes, the output file must be sparse");

    char *buf = xmalloc(TESTFILE_SIZE);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);
    memset(buf, 0, LONG_REGION);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_true(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE);
    assert_true(data_ok);

    free(buf);
    test_has_run[5] = true;
    success     [5] = true;
}

static void test_sparse_files_6(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "Many zeroes in the middle of the file, the output file must be sparse");

    char *buf = xmalloc(TESTFILE_SIZE);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);
    memset(&buf[TESTFILE_SIZE / 2 - 7], 0, LONG_REGION);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_true(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE);
    assert_true(data_ok);

    free(buf);
    test_has_run[6] = true;
    success     [6] = true;
}

static void test_sparse_files_7(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "File ending with many zeroes, the output file must be sparse");

    char *buf = xmalloc(TESTFILE_SIZE);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);
    memset(&buf[TESTFILE_SIZE - LONG_REGION], 0, LONG_REGION);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_true(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE);
    assert_true(data_ok);

    free(buf);
    test_has_run[7] = true;
    success     [7] = true;
}

static void test_sparse_files_8(void)
{
    Log(LOG_LEVEL_VERBOSE,
        "Special Case: File ending with few (DEV_BSIZE-1) zeroes,"
        " at block size barrier; so the last bytes written are a hole and are seek()ed,"
        " but the output file can't be sparse since the hole isn't block-sized");

    char *buf = xmalloc(TESTFILE_SIZE + DEV_BSIZE - 1);

    FillBufferWithGarbage(buf, TESTFILE_SIZE);
    memset(&buf[TESTFILE_SIZE], 0, DEV_BSIZE - 1);

    WriteBufferToFile(srcfile, buf, TESTFILE_SIZE + DEV_BSIZE - 1);

    /* ACTUAL TEST */
    bool ret = CopyRegularFileDisk(srcfile, dstfile);
    assert_true(ret);

    bool is_sparse = FileIsSparse(dstfile);
    assert_false(is_sparse);

    bool data_ok = CompareFileToBuffer(dstfile, buf, TESTFILE_SIZE + DEV_BSIZE - 1);
    assert_true(data_ok);

    free(buf);
    test_has_run[8] = true;
    success     [8] = true;
}



int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] = {
        unit_test(init),
        unit_test(test_sparse_files_1),
        unit_test(test_sparse_files_2),
        unit_test(test_sparse_files_3),
        unit_test(test_sparse_files_4),
        unit_test(test_sparse_files_5),
/* Not sure why, but on HP-UX with "vxfs" filesystem, the sparse file
 * generated in test_6 has actually greater "disk usage" than its length.
 * "du" verifies it. */
#ifndef __hpux
        unit_test(test_sparse_files_6),
#endif
        unit_test(test_sparse_files_7),
        unit_test(test_sparse_files_8),
        unit_test(finalise),
    };

    int ret = run_tests(tests);

    return ret;
}
