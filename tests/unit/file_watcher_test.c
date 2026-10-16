#include <test.h>

#include <file_watcher.h>       /* FileWatcherStateNew(), FileWatcherCheckFileDeleted() */
#include <rlist.h>              /* RlistAppendScalar() */
#include <logging_priv.h>       /* LoggingPrivContext, LoggingPrivSetContext() */

#include <string.h>             /* strstr() */
#include <stdio.h>              /* snprintf() */
#include <stdlib.h>             /* mkdtemp() */
#include <unistd.h>             /* close(), unlink(), rmdir(), symlink() */
#include <fcntl.h>              /* open() */

static int captured_err_count = 0;
static char captured_err_message[256];

static char *CaptureErrorLogHook(ARG_UNUSED LoggingPrivContext *pctx, LogLevel level, const char *message)
{
    if (level == LOG_LEVEL_ERR)
    {
        captured_err_count++;
        strlcpy(captured_err_message, message, sizeof(captured_err_message));
    }
    return (char *) message;
}

/* Returns a new state of a file_deleted watcher of the file at path */
static void *NewFileDeletedState(const char *path)
{
    Rlist *args = NULL;
    RlistAppendScalar(&args, path);
    void *state = FileWatcherStateNew(args);
    RlistDestroy(args);
    return state;
}

/* Creates the file 'file' in a new temporary directory dir */
static void CreateTempFile(char *dir, char *path, size_t path_size)
{
    assert_true(mkdtemp(dir) != NULL);
    snprintf(path, path_size, "%s/file", dir);
    int file_fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert_true(file_fd >= 0);
    close(file_fd);
}

/* Sets up dir/loop, a symlink to itself, so that lstat() of dir/loop/file
 * fails with ELOOP (even as root), i.e. whether the file was deleted can't
 * be told. Unlinking the loop makes lstat() work again (ENOENT). */
static void SetUpSymlinkLoop(char *dir, char *loop, size_t loop_size, char *path, size_t path_size)
{
    assert_true(mkdtemp(dir) != NULL);
    snprintf(loop, loop_size, "%s/loop", dir);
    assert_int_equal(symlink(loop, loop), 0);
    snprintf(path, path_size, "%s/file", loop);
}

static void TearDownSymlinkLoop(const char *dir, const char *loop)
{
    unlink(loop);
    assert_int_equal(rmdir(dir), 0);
}

static void test_check_file_deleted_transitions(void)
{
    char dir[] = "/tmp/file_watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    void *state = NewFileDeletedState(path);

    /* Present (the first check is the reference) */
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);

    /* Deleted: the event happens once, not while the file stays absent */
    assert_int_equal(unlink(path), 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_EVENT);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);

    /* Created again, then deleted again: the event happens again */
    int file_fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert_true(file_fd >= 0);
    close(file_fd);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_EVENT);

    FileWatcherStateDestroy(state);
    assert_int_equal(rmdir(dir), 0);
}

static void test_check_file_deleted_absent_at_start(void)
{
    char dir[] = "/tmp/file_watcher_test.XXXXXX";
    char path[PATH_MAX];
    assert_true(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/file", dir);

    void *state = NewFileDeletedState(path);

    /* The first check is the reference, at which the file is absent */
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);

    /* Still absent: never seen, so not deleted */
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);

    /* Seen once created, so deleted afterwards */
    int file_fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert_true(file_fd >= 0);
    close(file_fd);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_EVENT);

    FileWatcherStateDestroy(state);
    assert_int_equal(rmdir(dir), 0);
}

static void test_check_file_deleted_dangling_symlink(void)
{
    char dir[] = "/tmp/file_watcher_test.XXXXXX";
    char path[PATH_MAX];
    char link[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));
    snprintf(link, sizeof(link), "%s/link", dir);
    assert_int_equal(symlink(path, link), 0);

    void *state = NewFileDeletedState(link);

    /* The first check is the reference */
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);

    /* Symlinks are not followed: the link is not deleted when its target
     * is, only when the link itself is */
    assert_int_equal(unlink(path), 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(unlink(link), 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_EVENT);

    FileWatcherStateDestroy(state);
    assert_int_equal(rmdir(dir), 0);
}

static void test_check_file_deleted_error(void)
{
    char dir[] = "/tmp/file_watcher_test.XXXXXX";
    char loop[PATH_MAX];
    char path[PATH_MAX];
    SetUpSymlinkLoop(dir, loop, sizeof(loop), path, sizeof(path));

    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    void *state = NewFileDeletedState(path);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_ERROR);
    FileWatcherStateDestroy(state);

    LogSetGlobalLevel(old_level);
    TearDownSymlinkLoop(dir, loop);
}

static void test_check_file_deleted_logs_error_once(void)
{
    char dir[] = "/tmp/file_watcher_test.XXXXXX";
    char loop[PATH_MAX];
    char path[PATH_MAX];
    SetUpSymlinkLoop(dir, loop, sizeof(loop), path, sizeof(path));

    captured_err_count = 0;
    captured_err_message[0] = '\0';
    LoggingPrivContext log_ctx = {
        .log_hook = CaptureErrorLogHook,
        .force_hook_level = LOG_LEVEL_ERR,
    };
    LoggingPrivSetContext(&log_ctx);
    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    /* The failure of the first check is logged */
    void *state = NewFileDeletedState(path);
    assert_int_equal(captured_err_count, 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_ERROR);
    assert_int_equal(captured_err_count, 1);
    assert_true(strstr(captured_err_message, "Unable to lstat") != NULL);

    /* The following failures are not logged again */
    for (int i = 0; i < 3; i++)
    {
        assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_ERROR);
    }
    assert_int_equal(captured_err_count, 1);

    /* Once lstat() works again, the next failure is logged again */
    assert_int_equal(unlink(loop), 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(symlink(loop, loop), 0);
    assert_int_equal(FileWatcherCheckFileDeleted(state), WATCHER_CHECK_ERROR);
    assert_int_equal(captured_err_count, 2);

    FileWatcherStateDestroy(state);

    LogSetGlobalLevel(old_level);
    LoggingPrivSetContext(NULL);
    TearDownSymlinkLoop(dir, loop);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_check_file_deleted_transitions),
        unit_test(test_check_file_deleted_absent_at_start),
        unit_test(test_check_file_deleted_dangling_symlink),
        unit_test(test_check_file_deleted_error),
        unit_test(test_check_file_deleted_logs_error_once),
    };

    return run_tests(tests);
}
