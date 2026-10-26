#include <test.h>

#include <watcher.h>
#include <file_watcher.h>       /* FileWatcherStateNew(), FileWatcherCheckFileDeleted() */
#include <policy.h>             /* Promise */
#include <rlist.h>              /* RlistAppendScalar() */
#include <string_lib.h>         /* StringEqual() */

#include <stdio.h>              /* snprintf() */
#include <stdlib.h>             /* mkdtemp() */
#include <unistd.h>             /* close(), unlink(), rmdir(), symlink(), sleep() */
#include <fcntl.h>              /* open() */

/* The registry only hashes the expanded promise it is given (its promiser
 * here, as it has no constraints, comment or parent) and hands a copy of it
 * back to the WatcherEventFn, so minimal promises are enough. Their org_pp
 * tells apart the identical promises of different policies. */
static const Promise unexpanded_promise;
static const Promise expanded_promise = {
    .promiser = "test-event",
    .org_pp = &unexpanded_promise,
};

/* Returns the options of a file_deleted watcher, valid until the next call
 * (WatcherRegister() copies them) */
static WatcherOptions *FileDeletedOptions(const char *path, time_t interval)
{
    static WatcherOptions opt;

    Rlist *args = NULL;
    RlistAppendScalar(&args, path);
    opt = (WatcherOptions) {
        .state = FileWatcherStateNew(args),
        .check_callback = FileWatcherCheckFileDeleted,
        .destroy_state = FileWatcherStateDestroy,
        .interval = interval,
    };
    RlistDestroy(args);
    return &opt;
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

#define MAX_RECORDED_EVENTS 8

static int handled_event_count = 0;
static const Promise *last_event_org_promise = NULL;
static char last_event_promise_hash[CF_HOSTKEY_STRING_SIZE];
static WatcherCheckResult last_event_check = WATCHER_CHECK_NO_EVENT;

/* Promise hashes of the first MAX_RECORDED_EVENTS handled events */
static char event_promise_hashes[MAX_RECORDED_EVENTS][CF_HOSTKEY_STRING_SIZE];

static void RecordEvent(ARG_UNUSED EvalContext *ctx, const Promise *pp,
                        WatcherCheckResult check)
{
    char promise_hash[CF_HOSTKEY_STRING_SIZE];
    WatcherPromiseHash(pp, promise_hash, sizeof(promise_hash));

    if (handled_event_count < MAX_RECORDED_EVENTS)
    {
        strlcpy(event_promise_hashes[handled_event_count], promise_hash,
                sizeof(event_promise_hashes[handled_event_count]));
    }
    handled_event_count++;
    last_event_org_promise = pp->org_pp;
    strlcpy(last_event_promise_hash, promise_hash, sizeof(last_event_promise_hash));
    last_event_check = check;
}

/* Asserts that the last event was handled for the iteration `pp` */
static void AssertLastEventFor(const Promise *pp)
{
    char promise_hash[CF_HOSTKEY_STRING_SIZE];
    WatcherPromiseHash(pp, promise_hash, sizeof(promise_hash));
    assert_true(last_event_org_promise == pp->org_pp);
    assert_string_equal(last_event_promise_hash, promise_hash);
}

/* Returns how many of the recorded events were handled for the iteration `pp` */
static int RecordedEventCountFor(const Promise *pp)
{
    char promise_hash[CF_HOSTKEY_STRING_SIZE];
    WatcherPromiseHash(pp, promise_hash, sizeof(promise_hash));

    int count = 0;
    for (int i = 0; i < MIN(handled_event_count, MAX_RECORDED_EVENTS); i++)
    {
        if (StringEqual(event_promise_hashes[i], promise_hash))
        {
            count++;
        }
    }
    return count;
}

static void ResetRecordedEvents(void)
{
    handled_event_count = 0;
    last_event_org_promise = NULL;
    last_event_promise_hash[0] = '\0';
    last_event_check = WATCHER_CHECK_NO_EVENT;
    for (int i = 0; i < MAX_RECORDED_EVENTS; i++)
    {
        event_promise_hashes[i][0] = '\0';
    }
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

    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 5)));

    WatcherRegistryFinalize();
}

static void test_watcher_register_iterations(void)
{
    WatcherRegistryInitialize();

    /* The iterations of a promise (same promise, other promisers) are
     * different watchers */
    const Promise iteration_a = { .promiser = "/a", .org_pp = &unexpanded_promise };
    const Promise iteration_b = { .promiser = "/b", .org_pp = &unexpanded_promise };
    WatcherRegister(&iteration_a, FileDeletedOptions("/nonexistent/a", 5));
    WatcherRegister(&iteration_b, FileDeletedOptions("/nonexistent/b", 5));

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
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1)));
    assert_true(WatcherRegister(&other_expanded_promise, FileDeletedOptions(path, 1)));

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
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 5));
    EventWatcherPause(NULL, RecordEvent);
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 5));
    EventWatcherResume();

    /* Paused twice in a row (e.g. a failed policy reload in between), the
     * watchers set aside the first time are replaced by those registered
     * since, without leaking (checked by valgrind/ASan) */
    EventWatcherPause(NULL, RecordEvent);
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 5));
    EventWatcherPause(NULL, RecordEvent);
    WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 5));
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
    WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1));

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_int_equal(unlink(path), 0);

    /* Wait for the watcher thread to queue the event (it checks every
     * second), without handling it */
    assert_true(WaitForQueuedEvent(fd, 50));

    /* Pausing handles the queued event, with the promise of its watcher, so
     * that the watchers can be set aside without losing it */
    ResetRecordedEvents();
    EventWatcherPause(NULL, RecordEvent);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&expanded_promise);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);

    EventWatcherResume();

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_idle_woken_by_wake_up(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    /* Without watchers, the watcher thread sleeps until woken up */
    WatcherRegistryInitialize();
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    sleep(1);

    /* Registering a watcher doesn't wake it up, EventWatcherWakeUp() must */
    WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1));
    assert_int_equal(unlink(path), 0);
    assert_false(WaitForQueuedEvent(fd, 20));
    EventWatcherWakeUp();
    assert_true(WaitForQueuedEvent(fd, 50));

    ResetRecordedEvents();
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_idle_woken_by_resume(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    WatcherRegistryInitialize();
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    sleep(1);

    /* Registering doesn't wake the watcher thread up, resuming must */
    EventWatcherPause(NULL, RecordEvent);
    WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1));
    assert_int_equal(unlink(path), 0);
    assert_false(WaitForQueuedEvent(fd, 20));
    EventWatcherResume();
    assert_true(WaitForQueuedEvent(fd, 50));

    ResetRecordedEvents();
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_state_kept_across_pause(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    WatcherRegistryInitialize();
    WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1));

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
    WatcherRegister(&new_expanded_promise, FileDeletedOptions(path, 1));
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
    WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1));

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
    WatcherRegister(&new_expanded_promise, FileDeletedOptions(path, 1));
    WatcherRegister(&other_expanded_promise, FileDeletedOptions(path, 1));
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
    WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1));

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
    WatcherRegister(&other_expanded_promise, FileDeletedOptions(path, 1));
    EventWatcherResume();

    /* The watcher thread checks at least every second */
    assert_false(WaitForQueuedEvent(fd, 30));
    assert_int_equal(handled_event_count, 0);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static void test_event_watcher_several_events(void)
{
    char dir_a[] = "/tmp/watcher_test.XXXXXX";
    char dir_b[] = "/tmp/watcher_test.XXXXXX";
    char path_a[PATH_MAX];
    char path_b[PATH_MAX];
    CreateTempFile(dir_a, path_a, sizeof(path_a));
    CreateTempFile(dir_b, path_b, sizeof(path_b));

    WatcherRegistryInitialize();
    const Promise iteration_a = { .promiser = "a", .org_pp = &unexpanded_promise };
    const Promise iteration_b = { .promiser = "b", .org_pp = &unexpanded_promise };
    WatcherRegister(&iteration_a, FileDeletedOptions(path_a, 1));
    WatcherRegister(&iteration_b, FileDeletedOptions(path_b, 1));

    /* Both deleted before the first pass of the watcher thread, so both
     * events are queued before the wakeup, and handled together */
    assert_int_equal(unlink(path_a), 0);
    assert_int_equal(unlink(path_b), 0);

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    ResetRecordedEvents();
    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 2);
    assert_int_equal(RecordedEventCountFor(&iteration_a), 1);
    assert_int_equal(RecordedEventCountFor(&iteration_b), 1);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);

    /* Each fires once */
    assert_false(WaitForQueuedEvent(fd, 20));

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir_a), 0);
    assert_int_equal(rmdir(dir_b), 0);
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

static void test_event_watcher_check_failure_reported_once(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char loop[PATH_MAX];
    char path[PATH_MAX];
    SetUpSymlinkLoop(dir, loop, sizeof(loop), path, sizeof(path));

    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    /* Registered while lstat() works, failing from then on */
    WatcherRegistryInitialize();
    assert_int_equal(unlink(loop), 0);
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1)));
    assert_int_equal(symlink(loop, loop), 0);

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    /* The failing check is reported as a failure, not as an event */
    assert_true(WaitForQueuedEvent(fd, 50));
    ResetRecordedEvents();
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&expanded_promise);
    assert_int_equal(last_event_check, WATCHER_CHECK_ERROR);

    /* The following failing checks (every second) are not reported again */
    assert_false(WaitForQueuedEvent(fd, 30));

    /* Once a check succeeds again, the next failure is reported again */
    assert_int_equal(unlink(loop), 0);
    sleep(2);
    assert_int_equal(symlink(loop, loop), 0);
    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 2);
    assert_int_equal(last_event_check, WATCHER_CHECK_ERROR);

    EventWatcherFinalize();

    LogSetGlobalLevel(old_level);
    TearDownSymlinkLoop(dir, loop);
}

static void test_watcher_register_check_failure(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char loop[PATH_MAX];
    char path[PATH_MAX];
    SetUpSymlinkLoop(dir, loop, sizeof(loop), path, sizeof(path));

    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    /* Without a reference, the watcher is not registered (its state is
     * destroyed by the registry, checked by valgrind/ASan) */
    WatcherRegistryInitialize();
    assert_false(WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1)));

    /* So the watcher thread doesn't check it, and doesn't report its failure */
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_false(WaitForQueuedEvent(fd, 20));

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
        unit_test(test_watcher_register_iterations),
        unit_test(test_watcher_register_identical_ignored),
        unit_test(test_watcher_register_again_after_pause),
        unit_test(test_event_watcher_lifecycle),
        unit_test(test_event_watcher_pause_handles_queued_events),
        unit_test(test_event_watcher_idle_woken_by_wake_up),
        unit_test(test_event_watcher_idle_woken_by_resume),
        unit_test(test_event_watcher_state_kept_across_pause),
        unit_test(test_event_watcher_state_kept_identical_ignored),
        unit_test(test_event_watcher_state_not_kept_for_other_promise),
        unit_test(test_event_watcher_several_events),
        unit_test(test_event_watcher_check_failure_reported_once),
        unit_test(test_watcher_register_check_failure),
    };

    return run_tests(tests);
}
