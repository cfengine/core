#include <test.h>

#include <watcher.h>
#include <file_watcher.h>       /* FileWatcherStateNew(), FileWatcherCheckFileDeleted() */
#include <policy.h>             /* Promise */

#include <stdio.h>              /* snprintf() */
#include <stdlib.h>             /* mkdtemp() */
#include <unistd.h>             /* close(), unlink(), rmdir() */
#include <fcntl.h>              /* open() */

/* The registry only hashes the expanded promise it is given (its promiser
 * here, as it has no constraints, comment or parent) and hands the
 * unexpanded one (org_pp) back to the WatcherEventFn, so minimal promises
 * are enough. */
static const Promise unexpanded_promise;
static const Promise expanded_promise = {
    .promiser = "test-event",
    .org_pp = &unexpanded_promise,
};

static WatcherOptions FileDeletedOptions(const char *path)
{
    WatcherOptions opt = {
        .state = FileWatcherStateNew(path),
        .check_callback = FileWatcherCheckFileDeleted,
        .destroy_state = DestroyFileWatcherState,
    };
    return opt;
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

/* Waits up to tenths/10 seconds for the watcher thread to queue an event,
 * without handling it */
static bool WaitForQueuedEvent(int fd, int tenths)
{
    for (int i = 0; i < tenths; i++)
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

static int handled_event_count = 0;
static const Promise *last_event_promise = NULL;
static char last_event_promise_hash[CF_HOSTKEY_STRING_SIZE];

static void RecordEvent(ARG_UNUSED EvalContext *ctx, const Promise *pp,
                        const char *promise_hash)
{
    handled_event_count++;
    last_event_promise = pp;
    strlcpy(last_event_promise_hash, promise_hash, sizeof(last_event_promise_hash));
}

/* Asserts that the last event was handled for the iteration `pp` */
static void AssertLastEventFor(const Promise *pp)
{
    char promise_hash[CF_HOSTKEY_STRING_SIZE];
    WatcherPromiseHash(pp, promise_hash, sizeof(promise_hash));
    assert_true(last_event_promise == pp->org_pp);
    assert_string_equal(last_event_promise_hash, promise_hash);
}

static void ResetRecordedEvents(void)
{
    handled_event_count = 0;
    last_event_promise = NULL;
    last_event_promise_hash[0] = '\0';
}

static void HandleEvents(int fd)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    EventWatcherHandleEvents(NULL, RecordEvent, fd, &readfds);
}

static void test_registry_initialize_finalize(void)
{
    WatcherRegistryInitialize();
    WatcherRegistryFinalize();
}

static void test_watcher_register_single(void)
{
    WatcherRegistryInitialize();

    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event"), 5);

    WatcherRegistryFinalize();
}

static void test_watcher_register_iterations(void)
{
    WatcherRegistryInitialize();

    /* The iterations of a promise (same promise, other promisers) are
     * different watchers */
    const Promise iteration_a = { .promiser = "/a", .org_pp = &unexpanded_promise };
    const Promise iteration_b = { .promiser = "/b", .org_pp = &unexpanded_promise };
    WatcherRegister(&iteration_a, FileDeletedOptions("/nonexistent/a"), 5);
    WatcherRegister(&iteration_b, FileDeletedOptions("/nonexistent/b"), 5);

    WatcherRegistryFinalize();
}

static void test_watcher_register_identical_ignored(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    WatcherRegistryInitialize();

    /* Another promise identical to a registered one (same expanded promise,
     * so same hash) is ignored, so that the 'then' bundle isn't run twice
     * for the same event. The state of the ignored one is destroyed by the
     * registry (checked by valgrind/ASan). */
    static const Promise other_unexpanded_promise;
    const Promise other_expanded_promise = {
        .promiser = "test-event",
        .org_pp = &other_unexpanded_promise,
    };
    WatcherRegister(&expanded_promise, FileDeletedOptions(path), 1);
    WatcherRegister(&other_expanded_promise, FileDeletedOptions(path), 1);

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    ResetRecordedEvents();
    assert_int_equal(unlink(path), 0);

    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);

    /* Handled once, with the promise registered first */
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&expanded_promise);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_watcher_register_again_after_pause(void)
{
    WatcherRegistryInitialize();

    /* Pausing sets the registered watchers aside, so the same promise can be
     * registered again for the new policy, also without the event watcher
     * being initialized */
    ResetRecordedEvents();
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event"), 5);
    EventWatcherPause(NULL, RecordEvent);
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event"), 5);
    EventWatcherResume();

    /* Paused twice in a row (e.g. a failed policy reload in between), the
     * watchers set aside the first time are replaced by those registered
     * since, without leaking (checked by valgrind/ASan) */
    EventWatcherPause(NULL, RecordEvent);
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event"), 5);
    EventWatcherPause(NULL, RecordEvent);
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event"), 5);
    EventWatcherResume();

    assert_int_equal(handled_event_count, 0);

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
    ResetRecordedEvents();

    /* fd not set: must return early without touching the wakeup channel or
     * the event queue. */
    EventWatcherHandleEvents(NULL, RecordEvent, fd, &readfds);

    FD_SET(fd, &readfds);
    /* fd set but nothing queued: must drain the channel (no-op) and find
     * nothing to pop from the event queue. */
    EventWatcherHandleEvents(NULL, RecordEvent, fd, &readfds);
    assert_int_equal(handled_event_count, 0);

    /* Must wake up and stop the watcher thread by itself (IsPendingTermination()
     * is false here), join it, and finalize the watcher registry. */
    EventWatcherFinalize();
}

static void test_event_watcher_pause_handles_queued_events(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    WatcherRegistryInitialize();
    WatcherRegister(&expanded_promise, FileDeletedOptions(path), 1);

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_int_equal(unlink(path), 0);

    /* Wait for the watcher thread to queue the event (it checks every
     * second), without handling it */
    assert_true(WaitForQueuedEvent(fd, 50));

    /* Pausing handles the queued event, with the unexpanded promise and the
     * promise hash of its watcher, so that the watchers can be set aside without
     * losing it */
    ResetRecordedEvents();
    EventWatcherPause(NULL, RecordEvent);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&expanded_promise);

    EventWatcherResume();

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_state_kept_across_pause(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    WatcherRegistryInitialize();
    WatcherRegister(&expanded_promise, FileDeletedOptions(path), 1);

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    ResetRecordedEvents();
    EventWatcherPause(NULL, RecordEvent);
    assert_int_equal(handled_event_count, 0);

    /* Deleted while the policy is reloaded: a new state would never see the
     * file, so only the state kept from the previous watcher detects it */
    assert_int_equal(unlink(path), 0);

    /* The identical promise of the new policy, i.e. a new unexpanded
     * promise, but the same expanded one */
    static const Promise new_unexpanded_promise;
    const Promise new_expanded_promise = {
        .promiser = "test-event",
        .org_pp = &new_unexpanded_promise,
    };
    WatcherRegister(&new_expanded_promise, FileDeletedOptions(path), 1);
    EventWatcherResume();

    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);

    /* Handled with the promise of the new policy, not the destroyed one */
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&new_expanded_promise);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_state_kept_identical_ignored(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    WatcherRegistryInitialize();
    WatcherRegister(&expanded_promise, FileDeletedOptions(path), 1);

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    ResetRecordedEvents();
    EventWatcherPause(NULL, RecordEvent);
    assert_int_equal(unlink(path), 0);

    /* Two identical promises in the new policy: the first takes over the
     * state of the previous watcher, so the deletion is still detected, and
     * the second is ignored */
    static const Promise new_unexpanded_promise;
    static const Promise other_unexpanded_promise;
    const Promise new_expanded_promise = {
        .promiser = "test-event",
        .org_pp = &new_unexpanded_promise,
    };
    const Promise other_expanded_promise = {
        .promiser = "test-event",
        .org_pp = &other_unexpanded_promise,
    };
    WatcherRegister(&new_expanded_promise, FileDeletedOptions(path), 1);
    WatcherRegister(&other_expanded_promise, FileDeletedOptions(path), 1);
    EventWatcherResume();

    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);

    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&new_expanded_promise);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_state_not_kept_for_other_promise(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    WatcherRegistryInitialize();
    WatcherRegister(&expanded_promise, FileDeletedOptions(path), 1);

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    ResetRecordedEvents();
    EventWatcherPause(NULL, RecordEvent);
    assert_int_equal(unlink(path), 0);

    /* A different promise in the new policy, even on the same file, starts
     * with a new state, which never saw the file */
    const Promise other_expanded_promise = {
        .promiser = "other-event",
        .org_pp = &unexpanded_promise,
    };
    WatcherRegister(&other_expanded_promise, FileDeletedOptions(path), 1);
    EventWatcherResume();

    /* The watcher thread checks at least every second */
    assert_false(WaitForQueuedEvent(fd, 30));
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
        unit_test(test_watcher_register_iterations),
        unit_test(test_watcher_register_identical_ignored),
        unit_test(test_watcher_register_again_after_pause),
        unit_test(test_event_watcher_lifecycle),
        unit_test(test_event_watcher_pause_handles_queued_events),
        unit_test(test_event_watcher_state_kept_across_pause),
        unit_test(test_event_watcher_state_kept_identical_ignored),
        unit_test(test_event_watcher_state_not_kept_for_other_promise),
    };

    return run_tests(tests);
}
