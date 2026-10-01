#include <test.h>

#include <watcher.h>
#include <file_watcher.h>       /* FileWatcherStateNew() */
#include <policy.h>             /* Promise */
#include <logging_priv.h>       /* LoggingPrivContext, LoggingPrivSetContext() */

#include <string.h>             /* strstr() */
#include <stdio.h>              /* snprintf() */
#include <stdlib.h>             /* mkdtemp() */
#include <unistd.h>             /* close(), unlink(), rmdir() */
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
                       ARG_UNUSED const char *promiser)
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

    assert_true(WatcherRegister("test-event", "test:test-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/test-event"), &dummy_promise, 5));

    WatcherRegistryFinalize();
}

static void test_watcher_register_duplicate_ignored(void)
{
    WatcherRegistryInitialize();

    assert_true(WatcherRegister("dup-event", "test:dup-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/dup-event-a"), &dummy_promise, 5));

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
    const bool registered = WatcherRegister("dup-event", "test:dup-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/dup-event-b"), &dummy_promise, 5);

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
    assert_true(WatcherRegister("/a", "test:/a", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/a"), &dummy_promise, 5));
    assert_true(WatcherRegister("/b", "test:/b", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/b"), &dummy_promise, 5));
    assert_true(WatcherRegister("/a", "test:/a", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/a"), &other_promise, 5));

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

static const Promise *last_event_promise = NULL;

static void RecordEvent(ARG_UNUSED EvalContext *ctx, const Promise *pp,
                        ARG_UNUSED const char *promiser)
{
    handled_event_count++;
    last_event_promise = pp;
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
    assert_true(WatcherRegister("pause-event", "test:pause-event", EVENT_FILE_DELETED, FileWatcherStateNew(path), &dummy_promise, 1));

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_int_equal(unlink(path), 0);

    /* Wait for the watcher thread to queue the event (it checks every
     * second), without handling it */
    bool queued = false;
    for (int i = 0; !queued && i < 50; i++)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
        queued = (select(fd + 1, &readfds, NULL, NULL, &timeout) > 0);
    }
    assert_true(queued);

    /* Pausing handles the queued event, with the promise of its watcher, so
     * that the registry can be cleared without losing it */
    handled_event_count = 0;
    last_event_promise = NULL;
    EventWatcherPause(NULL, RecordEvent);
    assert_int_equal(handled_event_count, 1);
    assert_true(last_event_promise == &dummy_promise);

    WatcherRegistryClear();
    EventWatcherResume();

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_state_kept_across_reload(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    assert_true(mkdtemp(dir) != NULL);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/file", dir);
    int file_fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert_true(file_fd >= 0);
    close(file_fd);

    WatcherRegistryInitialize();
    assert_true(WatcherRegister("reload-event", "test:reload-event", EVENT_FILE_DELETED, FileWatcherStateNew(path), &dummy_promise, 1));

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    /* The file is deleted during a policy reload, while no events are
     * checked for. The watcher registered again from the new policy has a
     * state created after the deletion, which on its own would never see the
     * file deleted. */
    handled_event_count = 0;
    EventWatcherPause(NULL, CountEvent);
    assert_int_equal(unlink(path), 0);
    WatcherRegistryClear();
    static const Promise reloaded_promise;
    assert_true(WatcherRegister("reload-event", "test:reload-event", EVENT_FILE_DELETED, FileWatcherStateNew(path), &reloaded_promise, 1));
    EventWatcherResume();

    /* It takes over the state of the watcher of the previous policy, so the
     * deletion is detected as soon as the events are checked for again */
    bool queued = false;
    for (int i = 0; !queued && i < 50; i++)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
        queued = (select(fd + 1, &readfds, NULL, NULL, &timeout) > 0);
    }
    assert_true(queued);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    last_event_promise = NULL;
    EventWatcherHandleEvents(NULL, RecordEvent, fd, &readfds);
    assert_int_equal(handled_event_count, 1);
    assert_true(last_event_promise == &reloaded_promise);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_state_not_kept_for_other_key(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    assert_true(mkdtemp(dir) != NULL);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/file", dir);
    int file_fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert_true(file_fd >= 0);
    close(file_fd);

    WatcherRegistryInitialize();
    assert_true(WatcherRegister("other-key-event", "test:other-key-event then a", EVENT_FILE_DELETED,
                                FileWatcherStateNew(path), &dummy_promise, 1));

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    /* Same as above, but the events promise changed (e.g. its 'then'), so
     * the watcher of the new policy has another key and starts over with a
     * state that never saw the file */
    handled_event_count = 0;
    EventWatcherPause(NULL, CountEvent);
    assert_int_equal(unlink(path), 0);
    WatcherRegistryClear();
    assert_true(WatcherRegister("other-key-event", "test:other-key-event then b", EVENT_FILE_DELETED,
                                FileWatcherStateNew(path), &dummy_promise, 1));
    EventWatcherResume();

    /* The watcher thread checks every second, give it a few checks */
    bool queued = false;
    for (int i = 0; !queued && i < 25; i++)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
        queued = (select(fd + 1, &readfds, NULL, NULL, &timeout) > 0);
    }
    assert_false(queued);
    assert_int_equal(handled_event_count, 0);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
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
        unit_test(test_event_watcher_state_kept_across_reload),
        unit_test(test_event_watcher_state_not_kept_for_other_key),
    };

    return run_tests(tests);
}
