#include <test.h>

#include <watcher.h>
#include <file_watcher.h>       /* FileWatcherStateNew() */
#include <policy.h>             /* Promise */
#include <logging_priv.h>       /* LoggingPrivContext, LoggingPrivSetContext() */

#include <string.h>             /* strstr() */
#include <stdio.h>              /* snprintf() */
#include <stdlib.h>             /* mkdtemp() */
#include <unistd.h>             /* close(), unlink(), rmdir(), symlink(), sleep() */
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

/* The registry never dereferences the promise it is given, it only hands it
 * back to the WatcherEventFn, so a dummy one is enough. */
static const Promise dummy_promise;

static int handled_event_count = 0;

static void CountEvent(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Promise *pp,
                       ARG_UNUSED const char *promiser, ARG_UNUSED WatcherCheckResult check)
{
    handled_event_count++;
}

static void test_registry_initialize_finalize(void)
{
    WatcherRegistryInitialize();
    WatcherRegistryFinalize();
}

static void test_watcher_register_single(void)
{
    WatcherRegistryInitialize();

    assert_true(WatcherRegister("test-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/test-event"), &dummy_promise, 5));

    WatcherRegistryFinalize();
}

static void test_watcher_register_duplicate_ignored(void)
{
    WatcherRegistryInitialize();

    assert_true(WatcherRegister("dup-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/dup-event-a"), &dummy_promise, 5));

    captured_err_count = 0;
    captured_err_message[0] = '\0';
    /* force_hook_level makes the hook still see the error while the console
     * level is lowered, so the expected error isn't printed. */
    LoggingPrivContext log_ctx = {
        .log_hook = CaptureErrorLogHook,
        .force_hook_level = LOG_LEVEL_ERR,
    };
    LoggingPrivSetContext(&log_ctx);
    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    /* Registering the same promise and promiser again must be rejected: an error is logged
     * (not silently swallowed) and the first registration is kept, not
     * replaced. */
    const bool registered = WatcherRegister("dup-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/dup-event-b"), &dummy_promise, 5);

    LogSetGlobalLevel(old_level);
    LoggingPrivSetContext(NULL);

    assert_false(registered);
    assert_int_equal(captured_err_count, 1);
    assert_true(strstr(captured_err_message, "dup-event") != NULL);
    assert_true(strstr(captured_err_message, "already registered") != NULL);

    WatcherRegistryFinalize();
}

static void test_watcher_register_same_promiser_of_other_promise(void)
{
    WatcherRegistryInitialize();

    /* Watchers are identified by their promise and promiser: iterations of
     * a promise (same promise, other promisers), and promises with the same
     * promiser (other promises), are all different watchers */
    static const Promise other_promise;
    assert_true(WatcherRegister("/a", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/a"), &dummy_promise, 5));
    assert_true(WatcherRegister("/b", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/b"), &dummy_promise, 5));
    assert_true(WatcherRegister("/a", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/a"), &other_promise, 5));

    WatcherRegistryFinalize();
}

static void test_event_watcher_lifecycle(void)
{
    WatcherRegistryInitialize();

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_true(fd >= 0);

    fd_set readfds;
    FD_ZERO(&readfds);
    handled_event_count = 0;

    /* fd not set: must return early without touching the wakeup channel or
     * the event queue. */
    EventWatcherHandleEvents(NULL, CountEvent, fd, &readfds);

    FD_SET(fd, &readfds);
    /* fd set but nothing queued: must drain the channel (no-op) and find
     * nothing to pop from the event queue. */
    EventWatcherHandleEvents(NULL, CountEvent, fd, &readfds);
    assert_int_equal(handled_event_count, 0);

    /* Must wake up and stop the watcher thread by itself (IsPendingTermination()
     * is false here), join it, and finalize the watcher registry. */
    EventWatcherFinalize();
}

/* Waits up to timeout_secs for the watcher thread to signal a queued event
 * on fd, without handling it */
static bool WaitForQueuedEvent(int fd, int timeout_secs)
{
    for (int i = 0; i < timeout_secs * 10; i++)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
        if (select(fd + 1, &readfds, NULL, NULL, &timeout) > 0)
        {
            return true;
        }
    }
    return false;
}

static void HandleEvents(int fd, WatcherEventFn on_event)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    EventWatcherHandleEvents(NULL, on_event, fd, &readfds);
}

static const Promise *last_event_promise = NULL;

static WatcherCheckResult last_event_check = WATCHER_CHECK_NO_EVENT;

static void RecordEvent(ARG_UNUSED EvalContext *ctx, const Promise *pp,
                        ARG_UNUSED const char *promiser, WatcherCheckResult check)
{
    handled_event_count++;
    last_event_promise = pp;
    last_event_check = check;
}

static void test_event_watcher_pause_handles_queued_events(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    assert_true(mkdtemp(dir) != NULL);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/file", dir);
    int file_fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert_true(file_fd >= 0);
    close(file_fd);

    WatcherRegistryInitialize();
    assert_true(WatcherRegister("pause-event", EVENT_FILE_DELETED, FileWatcherStateNew(path), &dummy_promise, 1));

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_int_equal(unlink(path), 0);

    /* Wait for the watcher thread to queue the event (it checks every
     * second), without handling it */
    assert_true(WaitForQueuedEvent(fd, 5));

    /* Pausing handles the queued event, with the promise of its watcher, so
     * that the registry can be cleared without losing it */
    handled_event_count = 0;
    last_event_promise = NULL;
    EventWatcherPause(NULL, RecordEvent);
    assert_int_equal(handled_event_count, 1);
    assert_true(last_event_promise == &dummy_promise);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);

    WatcherRegistryClear();
    EventWatcherResume();

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
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

static void test_check_file_deleted_error(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char loop[PATH_MAX];
    char path[PATH_MAX];
    SetUpSymlinkLoop(dir, loop, sizeof(loop), path, sizeof(path));

    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    void *state = FileWatcherStateNew(path);
    assert_int_equal(CheckFileDeleted(state), WATCHER_CHECK_ERROR);
    DestroyFileWatcherState(state);

    LogSetGlobalLevel(old_level);
    TearDownSymlinkLoop(dir, loop);
}

static void test_check_file_deleted_logs_error_once(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
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

    /* The failure at registration is logged */
    void *state = FileWatcherStateNew(path);
    assert_int_equal(captured_err_count, 1);
    assert_true(strstr(captured_err_message, "Unable to lstat") != NULL);

    /* The following failures are not logged again */
    for (int i = 0; i < 3; i++)
    {
        assert_int_equal(CheckFileDeleted(state), WATCHER_CHECK_ERROR);
    }
    assert_int_equal(captured_err_count, 1);

    /* Once lstat() works again, the next failure is logged again */
    assert_int_equal(unlink(loop), 0);
    assert_int_equal(CheckFileDeleted(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(symlink(loop, loop), 0);
    assert_int_equal(CheckFileDeleted(state), WATCHER_CHECK_ERROR);
    assert_int_equal(captured_err_count, 2);

    DestroyFileWatcherState(state);

    LogSetGlobalLevel(old_level);
    LoggingPrivSetContext(NULL);
    TearDownSymlinkLoop(dir, loop);
}

static void test_event_watcher_check_failure_reported_once(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char loop[PATH_MAX];
    char path[PATH_MAX];
    SetUpSymlinkLoop(dir, loop, sizeof(loop), path, sizeof(path));

    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    WatcherRegistryInitialize();
    assert_true(WatcherRegister("failing-event", EVENT_FILE_DELETED, FileWatcherStateNew(path), &dummy_promise, 1));

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    /* The failing check is reported as a failure, not as an event */
    assert_true(WaitForQueuedEvent(fd, 5));
    handled_event_count = 0;
    last_event_check = WATCHER_CHECK_NO_EVENT;
    HandleEvents(fd, RecordEvent);
    assert_int_equal(handled_event_count, 1);
    assert_int_equal(last_event_check, WATCHER_CHECK_ERROR);

    /* The following failing checks (every second) are not reported again */
    assert_false(WaitForQueuedEvent(fd, 3));

    /* Once a check succeeds again, the next failure is reported again */
    assert_int_equal(unlink(loop), 0);
    sleep(2);
    assert_int_equal(symlink(loop, loop), 0);
    assert_true(WaitForQueuedEvent(fd, 5));
    HandleEvents(fd, RecordEvent);
    assert_int_equal(handled_event_count, 2);
    assert_int_equal(last_event_check, WATCHER_CHECK_ERROR);

    EventWatcherFinalize();

    LogSetGlobalLevel(old_level);
    TearDownSymlinkLoop(dir, loop);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_registry_initialize_finalize),
        unit_test(test_watcher_register_single),
        unit_test(test_watcher_register_duplicate_ignored),
        unit_test(test_watcher_register_same_promiser_of_other_promise),
        unit_test(test_event_watcher_lifecycle),
        unit_test(test_event_watcher_pause_handles_queued_events),
        unit_test(test_check_file_deleted_error),
        unit_test(test_check_file_deleted_logs_error_once),
        unit_test(test_event_watcher_check_failure_reported_once),
    };

    return run_tests(tests);
}
