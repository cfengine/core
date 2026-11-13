#include <test.h>

#include <watcher.h>
#include <file_watcher.h>       /* FileWatcherStateNew(), FileWatcherCheckFileDeleted() */
#include <policy.h>             /* Promise */
#include <rlist.h>              /* RlistAppendScalar() */
#include <string_lib.h>         /* StringEqual() */
#include <file_lib.h>           /* FileRead() */
#include <reactor_watch_backend.h>  /* REACTOR_WATCHERS_REPORT_FILE */
#include <watch_location.h>     /* MountinfoPathIsRemote(), WatchLocationSetMountTable() */

#include <stdio.h>              /* snprintf() */
#include <stdlib.h>             /* mkdtemp() */
#include <unistd.h>             /* close(), unlink(), rmdir(), symlink(), sleep() */
#include <fcntl.h>              /* open() */
#include <sys/stat.h>           /* mkdir() */
#include <pthread.h>
#include <signal.h>             /* alarm() */

/* The registry only hashes the expanded promise it is given (its promiser
 * and bundle here, as it has no constraints or comment) and hands a copy of
 * it back to the WatcherEventFn, so minimal promises are enough. Their org_pp
 * tells apart the identical promises of different policies. The bundle is
 * there for the watchers report. */
static Bundle test_bundle = { .name = "test", .ns = "default" };
static BundleSection test_section = { .parent_bundle = &test_bundle };
static const Promise unexpanded_promise;
static const Promise expanded_promise = {
    .promiser = "test-event",
    .org_pp = &unexpanded_promise,
    .parent_section = &test_section,
};

/* Checks of file_deleted watchers that told whether the file was deleted
 * (not WATCHER_CHECK_ERROR), to wait for a reference to be taken */
static int file_checks_ok = 0;

static WatcherCheckResult CountedCheckFileDeleted(void *state)
{
    const WatcherCheckResult check = FileWatcherCheckFileDeleted(state);
    if (check != WATCHER_CHECK_ERROR)
    {
        __atomic_add_fetch(&file_checks_ok, 1, __ATOMIC_SEQ_CST);
    }
    return check;
}

static int FileChecksOk(void)
{
    return __atomic_load_n(&file_checks_ok, __ATOMIC_SEQ_CST);
}

/** Waits up to 5 seconds for `count` successful file checks in total */
static void WaitForFileChecksOk(int count)
{
    for (int i = 0; i < 50 && FileChecksOk() < count; i++)
    {
        usleep(100000);
    }
    assert_true(FileChecksOk() >= count);
}

/* Returns the options of a file_deleted watcher of `path`, which is also its
 * target, so the mount table decides its lane (see FakeRemoteMount()). Valid
 * until the next call (WatcherRegister() copies them), `path` must outlive
 * WatcherRegister(). */
static WatcherOptions *FileDeletedOptions(const char *path, time_t interval)
{
    static WatcherOptions opt;

    Rlist *args = NULL;
    RlistAppendScalar(&args, path);
    opt = (WatcherOptions) {
        .state = FileWatcherStateNew(NULL, args),
        .check_callback = CountedCheckFileDeleted,
        .destroy_state = FileWatcherStateDestroy,
        .interval = interval,
        .target = { .type = REACTOR_WATCH_TARGET_FILE, .path = path },
    };
    RlistDestroy(args);
    return &opt;
}

/** Makes `mount_point` an NFS mount for WatchTargetIsRemote(), until
 * RemoveFakeRemoteMount() */
static void FakeRemoteMount(const char *mount_point)
{
    char mountinfo[PATH_MAX + 256];
    xsnprintf(mountinfo, sizeof(mountinfo),
             "22 1 8:1 / / rw - ext4 /dev/sda1 rw\n"
             "40 22 0:50 / %s rw - nfs4 srv:/export rw\n", mount_point);
    WatchLocationSetMountTable(mountinfo);
}

/** Back to the empty mount table of the tests, where everything is local */
static void RemoveFakeRemoteMount(void)
{
    WatchLocationSetMountTable("");
}

/** Polls `condition` every 0.1 s up to tenths/10 s, returns whether it held */
static bool WaitUntil(bool (*condition)(void), int tenths)
{
    for (int i = 0; i < tenths; i++)
    {
        if (condition())
        {
            return true;
        }
        usleep(100000);
    }
    return condition();
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
    const Promise iteration_a = { .promiser = "/a", .org_pp = &unexpanded_promise,
                                  .parent_section = &test_section };
    const Promise iteration_b = { .promiser = "/b", .org_pp = &unexpanded_promise,
                                  .parent_section = &test_section };
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
        .parent_section = &test_section,
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
        .parent_section = &test_section,
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
        .parent_section = &test_section,
    };
    const Promise other_expanded_promise = {
        .promiser = "test-event",
        .org_pp = &other_unexpanded_promise,
        .parent_section = &test_section,
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
        .parent_section = &test_section,
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
    const Promise iteration_a = { .promiser = "a", .org_pp = &unexpanded_promise,
                                  .parent_section = &test_section };
    const Promise iteration_b = { .promiser = "b", .org_pp = &unexpanded_promise,
                                  .parent_section = &test_section };
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
    const int checks_ok = FileChecksOk();
    assert_int_equal(unlink(loop), 0);
    WaitForFileChecksOk(checks_ok + 1);
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

    /* Without a reference, the watcher is still registered: its first
     * successful check becomes the reference */
    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1)));

    /* Its failing checks are reported once, as a failure, not as an event */
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_true(WaitForQueuedEvent(fd, 50));
    ResetRecordedEvents();
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);
    assert_int_equal(last_event_check, WATCHER_CHECK_ERROR);
    assert_false(WaitForQueuedEvent(fd, 30));

    EventWatcherFinalize();

    LogSetGlobalLevel(old_level);
    TearDownSymlinkLoop(dir, loop);
}

static void test_watcher_check_failure_retried_before_interval(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char loop[PATH_MAX];
    char path[PATH_MAX];
    SetUpSymlinkLoop(dir, loop, sizeof(loop), path, sizeof(path));

    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);

    /* Registered while its check fails, with an interval longer than the
     * test */
    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions(path, 300)));

    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    assert_true(WaitForQueuedEvent(fd, 50));
    ResetRecordedEvents();
    HandleEvents(fd);
    assert_int_equal(last_event_check, WATCHER_CHECK_ERROR);

    /* Once the file can be checked, the reference is taken well before the
     * interval, so a deletion checked for on a wakeup fires */
    assert_int_equal(unlink(loop), 0);
    assert_int_equal(mkdir(loop, 0700), 0);
    int file_fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert_true(file_fd >= 0);
    close(file_fd);
    WaitForFileChecksOk(FileChecksOk() + 1);
    assert_int_equal(unlink(path), 0);
    WatcherMarkAllDirty();
    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 2);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);

    EventWatcherFinalize();

    LogSetGlobalLevel(old_level);
    assert_int_equal(rmdir(loop), 0);
    assert_int_equal(rmdir(dir), 0);
}

/** Fires from the interval alone, which is shorter than the longest sleep of
 * the watcher thread, so it sleeps until the watcher is due, not just up to
 * that longest sleep */
static void test_event_watcher_fires_at_interval(void)
{
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));

    const int checks_ok = FileChecksOk();
    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions(path, 2)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    /* The check of the registration takes the reference, then the first
     * pass of the watcher thread checks again */
    WaitForFileChecksOk(checks_ok + 2);
    ResetRecordedEvents();
    assert_int_equal(unlink(path), 0);

    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);

    EventWatcherFinalize();
    assert_int_equal(rmdir(dir), 0);
}

static bool ReportMtime(const char *report_path, struct timespec *mtime)
{
    struct stat sb;
    if (stat(report_path, &sb) != 0)
    {
        return false;
    }
#if defined(HAVE_STRUCT_STAT_ST_MTIM)
    *mtime = sb.st_mtim;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC)
    *mtime = sb.st_mtimespec;
#else
    *mtime = (struct timespec) { .tv_sec = sb.st_mtime, .tv_nsec = 0 };
#endif
    return true;
}

/** The report is only rewritten when it changes, and removed when cf-reactor
 * finishes */
static void test_watchers_report_written_when_changed_removed_on_finalize(void)
{
    char workdir[] = "/tmp/watcher_test.XXXXXX";
    assert_true(mkdtemp(workdir) != NULL);
    char state_dir[64];
    xsnprintf(state_dir, sizeof(state_dir), "%s/state", workdir);
    assert_int_equal(mkdir(state_dir, 0700), 0);
    char report_path[128];
    xsnprintf(report_path, sizeof(report_path), "%s/%s", state_dir, REACTOR_WATCHERS_REPORT_FILE);
    assert_int_equal(setenv("CFENGINE_TEST_OVERRIDE_WORKDIR", workdir, 1), 0);

    WatcherRegistryInitialize();
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 300)));
    EventWatcherWakeUp();
    struct timespec first;
    assert_true(ReportMtime(report_path, &first));

    /* Same watchers, as after a policy re-read that changed nothing */
    usleep(50000);
    EventWatcherWakeUp();
    struct timespec second;
    assert_true(ReportMtime(report_path, &second));
    assert_true(first.tv_sec == second.tv_sec && first.tv_nsec == second.tv_nsec);

    /* A watcher of a bundle of another namespace, which the report tells apart */
    static Bundle other_bundle = { .name = "test", .ns = "other" };
    static BundleSection other_section = { .parent_bundle = &other_bundle };
    static const Promise other_unexpanded_promise;
    const Promise other_promise = {
        .promiser = "other-event",
        .org_pp = &other_unexpanded_promise,
        .parent_section = &other_section,
    };
    assert_true(WatcherRegister(&other_promise, FileDeletedOptions("/nonexistent/other-event", 300)));
    usleep(50000);
    EventWatcherWakeUp();
    struct timespec third;
    assert_true(ReportMtime(report_path, &third));
    assert_false(first.tv_sec == third.tv_sec && first.tv_nsec == third.tv_nsec);

    /* Sorted by bundle, whatever the order of registration */
    Writer *content = FileRead(report_path, SIZE_MAX, NULL);
    assert_true(content != NULL);
    const char *test_bundle_entry = strstr(StringWriterData(content), "\"bundle\":\"test\"");
    if (test_bundle_entry == NULL)
    {
        test_bundle_entry = strstr(StringWriterData(content), "\"bundle\": \"test\"");
    }
    const char *other_bundle_entry = strstr(StringWriterData(content), "other:test");
    assert_true(test_bundle_entry != NULL);
    assert_true(other_bundle_entry != NULL);
    assert_true(other_bundle_entry < test_bundle_entry);
    WriterClose(content);

    EventWatcherFinalize();
    assert_int_not_equal(access(report_path, F_OK), 0);

    unsetenv("CFENGINE_TEST_OVERRIDE_WORKDIR");
    assert_int_equal(rmdir(state_dir), 0);
    assert_int_equal(rmdir(workdir), 0);
}

/* A watcher whose second check (the first one of the watcher thread, the
 * first being WatcherRegister()'s) blocks until ReleaseBlockedCheck(), as on
 * an unresponsive NFS mount, then reports an event. Later checks find
 * nothing. One such check at a time. */
typedef struct
{
    int checks;
    int block_on;       /* the check that blocks */
    bool in_check;
} BlockingState;

static pthread_mutex_t blocking_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t blocking_cond = PTHREAD_COND_INITIALIZER;
static bool blocked_check_entered = false;
static bool blocked_check_released = false;
static int blocking_states_destroyed = 0;

static WatcherCheckResult BlockingCheck(void *state)
{
    BlockingState *bs = state;
    bs->checks++;
    if (bs->checks != bs->block_on)
    {
        return WATCHER_CHECK_NO_EVENT;
    }

    /* Like a hard NFS mount, not cancellable, see StoppableThreadStop() */
    int old_cancel_state;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_cancel_state);
    pthread_mutex_lock(&blocking_mutex);
    bs->in_check = true;
    blocked_check_entered = true;
    pthread_cond_broadcast(&blocking_cond);
    while (!blocked_check_released)
    {
        pthread_cond_wait(&blocking_cond, &blocking_mutex);
    }
    bs->in_check = false;
    pthread_mutex_unlock(&blocking_mutex);
    pthread_setcancelstate(old_cancel_state, NULL);
    return WATCHER_CHECK_EVENT;
}

static void BlockingStateDestroy(void *state)
{
    BlockingState *bs = state;
    /* Never while its check runs */
    assert_false(bs->in_check);
    pthread_mutex_lock(&blocking_mutex);
    blocking_states_destroyed++;
    pthread_mutex_unlock(&blocking_mutex);
    free(bs);
}

/** Options of a blocking watcher whose check number `block_on` blocks, of the
 * file at `path` (only for the lane it gets, see WatchTargetIsRemote()), or
 * NULL. Its check number 1 is WatcherRegister()'s, unless remote. */
static WatcherOptions *BlockingOptions(const char *path, int block_on)
{
    static WatcherOptions opt;
    BlockingState *bs = xcalloc(1, sizeof(BlockingState));
    bs->block_on = block_on;
    opt = (WatcherOptions) {
        .state = bs,
        .check_callback = BlockingCheck,
        .destroy_state = BlockingStateDestroy,
        .interval = 300,
        .target = { .type = REACTOR_WATCH_TARGET_FILE, .path = path },
    };
    return &opt;
}

static void ResetBlockedCheck(void)
{
    pthread_mutex_lock(&blocking_mutex);
    blocked_check_entered = false;
    blocked_check_released = false;
    blocking_states_destroyed = 0;
    pthread_mutex_unlock(&blocking_mutex);
}

static void WaitForBlockedCheck(void)
{
    pthread_mutex_lock(&blocking_mutex);
    while (!blocked_check_entered)
    {
        pthread_cond_wait(&blocking_cond, &blocking_mutex);
    }
    pthread_mutex_unlock(&blocking_mutex);
}

static void ReleaseBlockedCheck(void)
{
    pthread_mutex_lock(&blocking_mutex);
    blocked_check_released = true;
    pthread_cond_broadcast(&blocking_cond);
    pthread_mutex_unlock(&blocking_mutex);
}

static int BlockingStatesDestroyed(void)
{
    pthread_mutex_lock(&blocking_mutex);
    const int destroyed = blocking_states_destroyed;
    pthread_mutex_unlock(&blocking_mutex);
    return destroyed;
}

static bool OneBlockingStateDestroyed(void)
{
    return (BlockingStatesDestroyed() >= 1);
}

static void *ReleaseBlockedCheckLater(ARG_UNUSED void *arg)
{
    usleep(500000);
    ReleaseBlockedCheck();
    return NULL;
}

static time_t ElapsedSecs(const struct timespec *start)
{
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    return end.tv_sec - start->tv_sec;
}

static const Promise blocking_promise = {
    .promiser = "blocking-event",
    .org_pp = &unexpanded_promise,
    .parent_section = &test_section,
};

/** A blocked check doesn't block the main thread registering other watchers or
 * waking the watcher thread up, and its event fires once it returns */
static void test_blocked_check_does_not_block_registry(void)
{
    alarm(60); /* a deadlock fails the test instead of hanging it */
    ResetBlockedCheck();
    ResetRecordedEvents();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions(NULL, 2)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 300)));
    EventWatcherWakeUp();
    assert_true(ElapsedSecs(&start) < 2);

    assert_false(WaitForQueuedEvent(fd, 5));
    ReleaseBlockedCheck();
    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&blocking_promise);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);

    EventWatcherFinalize();
    assert_int_equal(BlockingStatesDestroyed(), 1);
    alarm(0);
}

/** Pausing, re-reading a policy without the watcher, and resuming don't wait
 * for its blocked check. The event it then observes is dropped with the
 * watcher, which is only freed once its check is over. */
static void test_blocked_check_watcher_removed(void)
{
    alarm(60);
    ResetBlockedCheck();
    ResetRecordedEvents();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions(NULL, 2)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    EventWatcherPause(NULL, RecordEvent);
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions("/nonexistent/test-event", 300)));
    EventWatcherResume();
    assert_true(ElapsedSecs(&start) < 2);
    assert_int_equal(BlockingStatesDestroyed(), 0);

    ReleaseBlockedCheck();
    assert_true(WaitUntil(OneBlockingStateDestroyed, 50));
    assert_int_equal(BlockingStatesDestroyed(), 1);
    assert_false(WaitForQueuedEvent(fd, 10));
    assert_int_equal(handled_event_count, 0);

    EventWatcherFinalize();
    alarm(0);
}

static void HandleEventsSince(int fd)
{
    assert_true(WaitForQueuedEvent(fd, 50));
    HandleEvents(fd);
}

/** Handles events for up to tenths/10 s, until `count` were handled in total.
 * A wakeup may be left from events already handled (a lane pushes an event,
 * then notifies, so a pop in between leaves the wakeup), so a wakeup doesn't
 * mean an event. */
static void HandleEventsUntil(int fd, int count, int tenths)
{
    for (int i = 0; i < tenths && handled_event_count < count; i++)
    {
        if (WaitForQueuedEvent(fd, 1))
        {
            HandleEvents(fd);
        }
    }
    assert_int_equal(handled_event_count, count);
}

/** Asserts that no event is handled within tenths/10 s, see
 * HandleEventsUntil() */
static void AssertNoEventHandled(int fd, int tenths)
{
    const int count = handled_event_count;
    for (int i = 0; i < tenths; i++)
    {
        if (WaitForQueuedEvent(fd, 1))
        {
            HandleEvents(fd);
        }
    }
    assert_int_equal(handled_event_count, count);
}

/** Re-reads the policy with an identical blocking watcher, i.e. a new
 * unexpanded promise but the same expanded one, and returns the latter */
static void ReReadWithIdenticalBlockingWatcher(const Promise *unexpanded, Promise *expanded)
{
    *expanded = (Promise) {
        .promiser = "blocking-event",
        .org_pp = unexpanded,
        .parent_section = &test_section,
    };
    EventWatcherPause(NULL, RecordEvent);
    assert_true(WatcherRegister(expanded, BlockingOptions(NULL, 2)));
    EventWatcherResume();
}

/** The identical watcher of the re-read policy is registered without waiting
 * for the blocked check: it shares the state, and so the event that check
 * observed. Its own state is destroyed right away. */
static void test_blocked_check_state_taken_over(void)
{
    alarm(60);
    ResetBlockedCheck();
    ResetRecordedEvents();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions(NULL, 2)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    static const Promise new_unexpanded_promise;
    Promise new_blocking_promise;
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    ReReadWithIdenticalBlockingWatcher(&new_unexpanded_promise, &new_blocking_promise);
    assert_true(ElapsedSecs(&start) < 2);

    assert_false(WaitForQueuedEvent(fd, 5));
    assert_int_equal(BlockingStatesDestroyed(), 1);

    ReleaseBlockedCheck();
    HandleEventsSince(fd);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&new_blocking_promise);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);
    assert_int_equal(BlockingStatesDestroyed(), 1);

    /* Fires once */
    assert_false(WaitForQueuedEvent(fd, 15));

    EventWatcherFinalize();
    assert_int_equal(BlockingStatesDestroyed(), 2);
    alarm(0);
}

/** Re-read twice while the check is blocked: the watcher of the last policy
 * gets the event, the one in between is dropped, and their own states are
 * destroyed right away */
static void test_blocked_check_state_taken_over_after_two_rereads(void)
{
    alarm(60);
    ResetBlockedCheck();
    ResetRecordedEvents();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions(NULL, 2)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    static const Promise first_unexpanded_promise;
    static const Promise second_unexpanded_promise;
    Promise first_promise;
    Promise second_promise;
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    ReReadWithIdenticalBlockingWatcher(&first_unexpanded_promise, &first_promise);
    ReReadWithIdenticalBlockingWatcher(&second_unexpanded_promise, &second_promise);
    assert_true(ElapsedSecs(&start) < 2);
    assert_int_equal(BlockingStatesDestroyed(), 2);

    ReleaseBlockedCheck();
    HandleEventsSince(fd);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&second_promise);
    assert_int_equal(last_event_check, WATCHER_CHECK_EVENT);
    assert_int_equal(BlockingStatesDestroyed(), 2);
    assert_false(WaitForQueuedEvent(fd, 15));

    EventWatcherFinalize();
    assert_int_equal(BlockingStatesDestroyed(), 3);
    alarm(0);
}

/** Finalized while paused, before the blocked check is over: every state is
 * destroyed once (twice would fail BlockingStateDestroy() or valgrind), the
 * shared one by the last of the watcher thread and the two watchers to drop
 * it */
static void test_blocked_check_finalized_before_handoff(void)
{
    alarm(60);
    ResetBlockedCheck();
    ResetRecordedEvents();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions(NULL, 2)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    static const Promise new_unexpanded_promise;
    const Promise new_blocking_promise = {
        .promiser = "blocking-event",
        .org_pp = &new_unexpanded_promise,
        .parent_section = &test_section,
    };
    EventWatcherPause(NULL, RecordEvent);
    assert_true(WatcherRegister(&new_blocking_promise, BlockingOptions(NULL, 2)));

    /* Finalizing joins the watcher thread, so release the check meanwhile */
    pthread_t releaser;
    assert_int_equal(pthread_create(&releaser, NULL, ReleaseBlockedCheckLater, NULL), 0);
    EventWatcherFinalize();
    assert_int_equal(pthread_join(releaser, NULL), 0);
    assert_int_equal(BlockingStatesDestroyed(), 2);
    assert_int_equal(handled_event_count, 0);
    alarm(0);
}

/* A mount table in the format of /proc/self/mountinfo, with the escaped
 * mount point "/mnt/my share", stacked mounts and fuse */
static const char *const MOUNTINFO =
    "22 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw,errors=remount-ro\n"
    "40 22 0:50 / /mnt rw,relatime shared:30 - nfs4 srv:/export rw,vers=4.2\n"
    "41 40 8:2 / /mnt/local rw,relatime shared:31 - ext4 /dev/sda2 rw\n"
    "42 22 0:51 / /mnt/my\\040share rw,relatime shared:32 - cifs //srv/share rw\n"
    "43 22 0:52 / /home/user/sshfs rw,nosuid,nodev,relatime shared:33 - fuse.sshfs u@h:/ rw\n"
    "44 22 0:53 / /data rw,relatime - nfs srv:/data rw\n"
    "45 22 8:3 / /data rw,relatime - xfs /dev/sda3 rw\n"
    "46 22 0:54 /sub /srv/nfs rw,relatime master:7 shared:9 - nfs srv:/sub rw\n"
    "garbage line\n"
    "47 22 0:55 / /broken rw,relatime\n";

static void test_mountinfo_path_classification(void)
{
    /* The longest mount point containing the path decides */
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/etc/passwd"));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/file"));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/mnt/local/file"));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/mnt/local"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/localfile"));

    /* Whole components only */
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/mntx/file"));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/srv/nfs2/file"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/srv/nfs/file"));

    /* Escaped mount point */
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/my share/file"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/my/file")); /* under /mnt */
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/home/user/file"));

    /* fuse counts as remote */
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/home/user/sshfs/file"));

    /* The last of stacked mounts hides the others */
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/data/file"));

    /* Repeated and trailing slashes don't hide a mount point */
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "//mnt//file"));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/mnt/local/"));

    /* Nor do "." and "..", collapsed lexically */
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/local/../file"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/./file"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/etc/../mnt/file"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/../../mnt"));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/mnt/.."));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/mnt/../etc/file"));
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "/mnt/./local/./file"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/.local"));
    assert_true(MountinfoPathIsRemote(MOUNTINFO, "/mnt/..local"));

    /* Relative paths and empty tables are local */
    assert_false(MountinfoPathIsRemote(MOUNTINFO, "mnt/file"));
    assert_false(MountinfoPathIsRemote("", "/mnt/file"));
}

static void test_mountinfo_fs_types(void)
{
    const char *const remote[] = { "nfs", "nfs4", "cifs", "smb3", "smbfs", "9p", "fuse.sshfs",
                                   "ceph", "glusterfs", "afs", "lustre", "fuse", "fuse.rclone",
                                   "ocfs2", "gfs2", "virtiofs", "beegfs", "panfs", "ncpfs" };
    const char *const local[] = { "ext4", "xfs", "btrfs", "tmpfs", "overlay", "nfsd", "fusectl" };
    char mountinfo[256];
    for (size_t i = 0; i < sizeof(remote) / sizeof(remote[0]); i++)
    {
        xsnprintf(mountinfo, sizeof(mountinfo), "40 22 0:50 / /m rw - %s src rw\n", remote[i]);
        assert_true(MountinfoPathIsRemote(mountinfo, "/m/file"));
    }
    for (size_t i = 0; i < sizeof(local) / sizeof(local[0]); i++)
    {
        xsnprintf(mountinfo, sizeof(mountinfo), "40 22 0:50 / /m rw - %s src rw\n", local[i]);
        assert_false(MountinfoPathIsRemote(mountinfo, "/m/file"));
    }
}

static void test_path_is_unc(void)
{
    assert_true(WatchPathIsUNC("\\\\server\\share\\file"));
    assert_true(WatchPathIsUNC("//server/share/file"));
    assert_true(WatchPathIsUNC("\\\\?\\UNC\\server\\share\\file"));
    assert_true(WatchPathIsUNC("\\\\?\\unc\\server\\share\\file"));
    assert_false(WatchPathIsUNC("\\\\?\\C:\\file"));
    assert_false(WatchPathIsUNC("\\\\.\\pipe\\name"));
    assert_false(WatchPathIsUNC("C:\\file"));
    assert_false(WatchPathIsUNC("\\file"));
    assert_false(WatchPathIsUNC("/etc/file"));
    assert_false(WatchPathIsUNC("\\\\"));
}

static void test_watch_target_is_remote(void)
{
    /* Only files can be remote */
    const ReactorWatchTarget users = { .type = REACTOR_WATCH_TARGET_USERS };
    assert_false(WatchTargetIsRemote(&users));
    const ReactorWatchTarget no_path = { .type = REACTOR_WATCH_TARGET_FILE };
    assert_false(WatchTargetIsRemote(&no_path));

#ifdef __MINGW32__
    const ReactorWatchTarget unc = { .type = REACTOR_WATCH_TARGET_FILE,
                                     .path = "\\\\server\\share\\file" };
    assert_true(WatchTargetIsRemote(&unc));
#else
    WatchLocationSetMountTable(MOUNTINFO);
    const ReactorWatchTarget remote = { .type = REACTOR_WATCH_TARGET_FILE, .path = "/mnt/file" };
    assert_true(WatchTargetIsRemote(&remote));
    const ReactorWatchTarget local = { .type = REACTOR_WATCH_TARGET_FILE, .path = "/etc/file" };
    assert_false(WatchTargetIsRemote(&local));

    /* Without a mount table, everything is local */
    WatchLocationSetMountTable("");
    assert_false(WatchTargetIsRemote(&remote));

    /* The host's own, whatever it says, read and parsed once until forgotten */
    WatchLocationSetMountTable(NULL);
    const bool remote_on_host = WatchTargetIsRemote(&local);
    assert_true(WatchTargetIsRemote(&local) == remote_on_host);
    WatchLocationForgetMountTable();
    assert_true(WatchTargetIsRemote(&local) == remote_on_host);

    RemoveFakeRemoteMount();
#endif
}

static const Promise remote_promise = {
    .promiser = "remote-event",
    .org_pp = &unexpanded_promise,
    .parent_section = &test_section,
};

/** A remote watcher's first check is on its lane, not in WatcherRegister(),
 * and becomes the reference */
static void test_remote_watcher_reference_taken_on_its_lane(void)
{
    alarm(60);
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));
    FakeRemoteMount(dir);
    ResetRecordedEvents();
    ResetBlockedCheck();

    WatcherRegistryInitialize();
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));

    /* It would block the registration if checked there */
    char blocking_path[PATH_MAX];
    xsnprintf(blocking_path, sizeof(blocking_path), "%s/blocking", dir);
    const int checks_ok = FileChecksOk();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions(blocking_path, 1)));
    assert_true(WatcherRegister(&remote_promise, FileDeletedOptions(path, 300)));
    assert_int_equal(FileChecksOk(), checks_ok);
    EventWatcherWakeUp();
    WaitForBlockedCheck();
    ReleaseBlockedCheck();
    HandleEventsSince(fd);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&blocking_promise);

    /* The file existed at the first check, its deletion fires */
    WaitForFileChecksOk(checks_ok + 1);
    assert_int_equal(unlink(path), 0);
    WatcherMarkAllDirty();
    HandleEventsSince(fd);
    assert_int_equal(handled_event_count, 2);
    AssertLastEventFor(&remote_promise);

    EventWatcherFinalize();
    RemoveFakeRemoteMount();
    assert_int_equal(rmdir(dir), 0);
    alarm(0);
}

/** A remote watcher's check blocked on an unresponsive server neither delays
 * the events of the local watchers nor the registration of other remote
 * ones */
static void test_blocked_remote_check_does_not_delay_local(void)
{
    alarm(60);
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));
    FakeRemoteMount("/remote");
    ResetRecordedEvents();
    ResetBlockedCheck();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions("/remote/blocking", 1)));
    assert_true(WatcherRegister(&expanded_promise, FileDeletedOptions(path, 1)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    assert_int_equal(unlink(path), 0);
    assert_true(WaitForQueuedEvent(fd, 20));
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&expanded_promise);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    assert_true(WatcherRegister(&remote_promise, FileDeletedOptions("/remote/other", 300)));
    EventWatcherWakeUp();
    assert_true(ElapsedSecs(&start) < 2);

    ReleaseBlockedCheck();
    HandleEventsSince(fd);
    assert_int_equal(handled_event_count, 2);
    AssertLastEventFor(&blocking_promise);

    EventWatcherFinalize();
    assert_int_equal(BlockingStatesDestroyed(), 1);
    RemoveFakeRemoteMount();
    assert_int_equal(rmdir(dir), 0);
    alarm(0);
}

/** The identical watcher of a re-read policy on the other lane (the file
 * system got mounted meanwhile) gets a fresh state rather than that of the
 * old watcher, which is about another file system, so it doesn't wait for a
 * check of the old state stuck on it. The event that check then observes is
 * dropped with it. */
static void test_watcher_moved_to_other_lane_starts_afresh(void)
{
    alarm(60);
    char dir[] = "/tmp/watcher_test.XXXXXX";
    char path[PATH_MAX];
    CreateTempFile(dir, path, sizeof(path));
    char blocking_path[PATH_MAX];
    xsnprintf(blocking_path, sizeof(blocking_path), "%s/blocking", dir);
    ResetBlockedCheck();
    ResetRecordedEvents();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions(blocking_path, 2)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    /* Same promise, so same hash, watching a file now on NFS */
    FakeRemoteMount(dir);
    static const Promise new_unexpanded_promise;
    const Promise new_blocking_promise = {
        .promiser = "blocking-event",
        .org_pp = &new_unexpanded_promise,
        .parent_section = &test_section,
    };
    const int checks_ok = FileChecksOk();
    EventWatcherPause(NULL, RecordEvent);
    assert_true(WatcherRegister(&new_blocking_promise, FileDeletedOptions(path, 300)));
    EventWatcherResume();

    /* Its reference is taken on the remote lane, then a deletion fires while
     * the old check is still blocked */
    WaitForFileChecksOk(checks_ok + 1);
    assert_int_equal(unlink(path), 0);
    WatcherMarkAllDirty();
    HandleEventsSince(fd);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&new_blocking_promise);
    assert_int_equal(BlockingStatesDestroyed(), 0);

    /* The old state goes once its check is over, with its event */
    ReleaseBlockedCheck();
    assert_true(WaitUntil(OneBlockingStateDestroyed, 50));
    AssertNoEventHandled(fd, 10);

    EventWatcherFinalize();
    assert_int_equal(BlockingStatesDestroyed(), 1);
    RemoveFakeRemoteMount();
    assert_int_equal(rmdir(dir), 0);
    alarm(0);
}

/* A watcher whose checks find an event while switch_events > 0, one per
 * check */
static int switch_events = 0;

static WatcherCheckResult SwitchCheck(ARG_UNUSED void *state)
{
    int left = __atomic_load_n(&switch_events, __ATOMIC_SEQ_CST);
    while (left > 0)
    {
        if (__atomic_compare_exchange_n(&switch_events, &left, left - 1, false,
                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        {
            return WATCHER_CHECK_EVENT;
        }
    }
    return WATCHER_CHECK_NO_EVENT;
}

static bool SwitchEventsObserved(void)
{
    return (__atomic_load_n(&switch_events, __ATOMIC_SEQ_CST) == 0);
}

/** Makes the next check of the switch watcher find an event, and waits up to
 * 3 s for a check to have found it */
static void SwitchEvent(void)
{
    __atomic_store_n(&switch_events, 1, __ATOMIC_SEQ_CST);
    WatcherMarkAllDirty();
    assert_true(WaitUntil(SwitchEventsObserved, 30));
}

static WatcherOptions *SwitchOptions(void)
{
    static WatcherOptions opt;
    opt = (WatcherOptions) {
        .state = xcalloc(1, 1),
        .check_callback = SwitchCheck,
        .destroy_state = free,
        .interval = 300,
    };
    return &opt;
}

static const Promise switch_promise = {
    .promiser = "switch-event",
    .org_pp = &unexpanded_promise,
    .parent_section = &test_section,
};

/** Re-reads the policy with an identical switch watcher, i.e. a new
 * unexpanded promise but the same expanded one, and returns the latter */
static void ReReadWithIdenticalSwitchWatcher(const Promise *unexpanded, Promise *expanded)
{
    *expanded = (Promise) {
        .promiser = "switch-event",
        .org_pp = unexpanded,
        .parent_section = &test_section,
    };
    EventWatcherPause(NULL, RecordEvent);
    assert_true(WatcherRegister(expanded, SwitchOptions()));
    EventWatcherResume();
}

/** Starts the event watcher with the switch watcher. Returns the fd. */
static int StartWithSwitchWatcher(void)
{
    __atomic_store_n(&switch_events, 0, __ATOMIC_SEQ_CST);
    ResetRecordedEvents();
    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&switch_promise, SwitchOptions()));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    EventWatcherWakeUp();
    return fd;
}

/** An event observed while the bundles of the previous one haven't run yet is
 * held, and runs once they have */
static void test_event_held_while_pending_runs_after(void)
{
    alarm(60);
    const int fd = StartWithSwitchWatcher();

    SwitchEvent();
    assert_true(WaitForQueuedEvent(fd, 20));
    SwitchEvent();  /* held, as the first one is pending */
    HandleEvents(fd);
    assert_int_equal(handled_event_count, 1);

    HandleEventsUntil(fd, 2, 50);
    AssertNoEventHandled(fd, 5);

    EventWatcherFinalize();
    alarm(0);
}

/** So it does if the policy is re-read before: the held event is in the
 * state the identical watcher of the new policy shares, and runs with its
 * promise */
static void test_event_held_while_pending_survives_reread(void)
{
    alarm(60);
    const int fd = StartWithSwitchWatcher();

    SwitchEvent();
    assert_true(WaitForQueuedEvent(fd, 20));
    SwitchEvent();  /* held, as the first one is pending */

    /* Pausing runs the first one */
    static const Promise new_unexpanded_promise;
    Promise new_switch_promise;
    ReReadWithIdenticalSwitchWatcher(&new_unexpanded_promise, &new_switch_promise);
    assert_int_equal(handled_event_count, 1);
    AssertLastEventFor(&switch_promise);

    HandleEventsUntil(fd, 2, 50);
    AssertLastEventFor(&new_switch_promise);
    AssertNoEventHandled(fd, 5);

    EventWatcherFinalize();
    alarm(0);
}

/** The limit on how often the bundles run is kept across a policy re-read:
 * re-reading doesn't let a watcher that hit it run again right away. The
 * event held meanwhile runs once the delay is over. */
static void test_trigger_limit_survives_reread(void)
{
    alarm(60);
    const int fd = StartWithSwitchWatcher();

    /* MAX_TRIGGERS_PER_WINDOW (10) runs in a row */
    for (int i = 1; i <= 10; i++)
    {
        SwitchEvent();
        HandleEventsUntil(fd, i, 50);
    }

    /* The next one is delayed by MIN_TRIGGER_BACKOFF_SECS (10) */
    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_ERR);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    SwitchEvent();
    AssertNoEventHandled(fd, 10);
    LogSetGlobalLevel(old_level);

    static const Promise new_unexpanded_promise;
    Promise new_switch_promise;
    ReReadWithIdenticalSwitchWatcher(&new_unexpanded_promise, &new_switch_promise);
    AssertNoEventHandled(fd, 20);

    HandleEventsUntil(fd, 11, 100);
    AssertLastEventFor(&new_switch_promise);
    assert_true(ElapsedSecs(&start) >= 9);

    EventWatcherFinalize();
    alarm(0);
}

/** Finalizing doesn't hang on a remote check that never returns: the lane is
 * left behind with what it uses. Last, as the registry is leaked. */
static void test_finalize_with_stuck_remote_check(void)
{
    alarm(60);
    FakeRemoteMount("/remote");
    ResetBlockedCheck();
    ResetRecordedEvents();

    WatcherRegistryInitialize();
    assert_true(WatcherRegister(&blocking_promise, BlockingOptions("/remote/blocking", 1)));
    int fd = -1;
    assert_true(EventWatcherInitialize(&fd));
    WaitForBlockedCheck();

    const LogLevel old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    EventWatcherFinalize();
    assert_true(ElapsedSecs(&start) < 8); /* the 5 s timeout, not forever */
    LogSetGlobalLevel(old_level);
    assert_int_equal(BlockingStatesDestroyed(), 0);

    /* The left behind lane may finish later, on what it was left */
    ReleaseBlockedCheck();
    sleep(1);
    WatchLocationSetMountTable(NULL);
    alarm(0);
}

int main()
{
    PRINT_TEST_BANNER();

    /* Everything is local, unless a test fakes a remote mount */
    WatchLocationSetMountTable("");

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
        unit_test(test_watcher_check_failure_retried_before_interval),
        unit_test(test_event_watcher_fires_at_interval),
        unit_test(test_watchers_report_written_when_changed_removed_on_finalize),
        unit_test(test_blocked_check_does_not_block_registry),
        unit_test(test_blocked_check_watcher_removed),
        unit_test(test_blocked_check_state_taken_over),
        unit_test(test_blocked_check_state_taken_over_after_two_rereads),
        unit_test(test_blocked_check_finalized_before_handoff),
        unit_test(test_event_held_while_pending_runs_after),
        unit_test(test_event_held_while_pending_survives_reread),
        unit_test(test_trigger_limit_survives_reread),
        unit_test(test_mountinfo_path_classification),
        unit_test(test_mountinfo_fs_types),
        unit_test(test_path_is_unc),
        unit_test(test_watch_target_is_remote),
        unit_test(test_remote_watcher_reference_taken_on_its_lane),
        unit_test(test_blocked_remote_check_does_not_delay_local),
        unit_test(test_watcher_moved_to_other_lane_starts_afresh),
        /* Last, see there */
        unit_test(test_finalize_with_stuck_remote_check),
    };

    return run_tests(tests);
}
