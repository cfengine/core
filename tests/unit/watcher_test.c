#include <test.h>

#include <watcher.h>
#include <file_watcher.h>       /* FileWatcherStateNew() */
#include <cf3.defs.h>           /* Rval */
#include <logging_priv.h>       /* LoggingPrivContext, LoggingPrivSetContext() */

#include <string.h>             /* strstr() */

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

static void test_registry_initialize_finalize(void)
{
    WatcherRegistryInitialize();
    WatcherRegistryFinalize();
}

static void test_watcher_register_single(void)
{
    WatcherRegistryInitialize();

    /* WatcherRegister() only deep-copies the rval, it never resolves the
     * bundle it names, so the bundle doesn't need to exist. */
    const Rval bundle = { .item = (char *) "test_bundle", .type = RVAL_TYPE_SCALAR };

    assert_true(WatcherRegister("test-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/test-event"), bundle, 5));

    WatcherRegistryFinalize();
}

static void test_watcher_register_duplicate_key_ignored(void)
{
    WatcherRegistryInitialize();

    const Rval bundle_a = { .item = (char *) "bundle_a", .type = RVAL_TYPE_SCALAR };
    const Rval bundle_b = { .item = (char *) "bundle_b", .type = RVAL_TYPE_SCALAR };

    assert_true(WatcherRegister("dup-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/dup-event-a"), bundle_a, 5));

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

    /* Registering the same key again must be rejected: an error is logged
     * (not silently swallowed) and the first registration is kept, not
     * replaced. */
    const bool registered = WatcherRegister("dup-event", EVENT_FILE_DELETED, FileWatcherStateNew("/nonexistent/dup-event-b"), bundle_b, 5);

    LogSetGlobalLevel(old_level);
    LoggingPrivSetContext(NULL);

    assert_false(registered);
    assert_int_equal(captured_err_count, 1);
    assert_true(strstr(captured_err_message, "dup-event") != NULL);
    assert_true(strstr(captured_err_message, "already registered") != NULL);

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
    /* fd not set: must return early without touching the wakeup channel or
     * the event queue. */
    EventWatcherHandleEvents(fd, &readfds);

    FD_SET(fd, &readfds);
    /* fd set but nothing queued: must drain the channel (no-op) and find
     * nothing to pop from the event queue. */
    EventWatcherHandleEvents(fd, &readfds);

    /* Must wake up and stop the watcher thread by itself (IsPendingTermination()
     * is false here), join it, and finalize the watcher registry. */
    EventWatcherFinalize();
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_registry_initialize_finalize),
        unit_test(test_watcher_register_single),
        unit_test(test_watcher_register_duplicate_key_ignored),
        unit_test(test_event_watcher_lifecycle),
    };

    return run_tests(tests);
}
