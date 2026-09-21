#include <test.h>

#include <stoppable_thread.h>
#include <alloc.h>
#include <threaded_queue.h>
#include <mutex.h>              /* ThreadLock(), ThreadUnlock() */

/* How long the thread routines below wait for the test to do something
 * before failing, so that a broken StoppableThread can't hang the test. */
#define TEST_TIMEOUT_SECS 10

typedef struct
{
    ThreadedQueue *started;     /* the routine pushes to it once running */
    bool should_stop_at_start;  /* StoppableThreadShouldStop() on entry */
    bool saw_stop;              /* the routine returned because of a stop */
    int iterations;
} RoutineData;

static RoutineData *RoutineDataNew(void)
{
    RoutineData *data = xcalloc(1, sizeof(RoutineData));
    data->started = ThreadedQueueNew(1, NULL);
    return data;
}

static void RoutineDataDestroy(RoutineData *data)
{
    ThreadedQueueDestroy(data->started);
    free(data);
}

/* Block until the routine has pushed to data->started. */
static void WaitForStart(RoutineData *data)
{
    void *item;
    assert_true(ThreadedQueuePop(data->started, &item, TEST_TIMEOUT_SECS));
}

/* Loops until stopped, sleeping for a long time on each iteration, so it only
 * returns promptly if StoppableThreadStop() wakes it up. */
static void LoopUntilStopped(StoppableThread *thread, void *arg)
{
    RoutineData *data = arg;
    data->should_stop_at_start = StoppableThreadShouldStop(thread);
    ThreadedQueuePush(data->started, data);

    while (!StoppableThreadShouldStop(thread))
    {
        data->iterations++;
        StoppableThreadSleep(thread, 60);
    }
    data->saw_stop = true;
}

/* Returns straight away without waiting for a stop. */
static void ReturnImmediately(ARG_UNUSED StoppableThread *thread, void *arg)
{
    RoutineData *data = arg;
    data->iterations++;
}

/* Sleeping for 0 or a negative number of seconds must return straight away,
 * which is checked by the test timing the whole routine. */
static void SleepNonPositive(StoppableThread *thread, void *arg)
{
    RoutineData *data = arg;
    StoppableThreadSleep(thread, 0);
    StoppableThreadSleep(thread, -1);
    data->iterations++;
}

/* Sleeping after a stop was requested must return straight away. */
static void SleepAfterStop(StoppableThread *thread, void *arg)
{
    RoutineData *data = arg;
    ThreadedQueuePush(data->started, data);

    while (!StoppableThreadShouldStop(thread))
    {
        StoppableThreadSleep(thread, 60);
    }
    StoppableThreadSleep(thread, 60);
    data->saw_stop = true;
}

static void test_stop_running_thread(void)
{
    RoutineData *data = RoutineDataNew();

    StoppableThread *thread = StoppableThreadStart(LoopUntilStopped, data);
    assert_true(thread != NULL);
    WaitForStart(data);

    assert_true(StoppableThreadStop(thread, TEST_TIMEOUT_SECS));

    /* The routine got the right arg, saw no stop before one was requested,
     * and returned because of it. Reading data is safe here: the thread has
     * been joined. */
    assert_false(data->should_stop_at_start);
    assert_true(data->saw_stop);
    assert_true(data->iterations >= 1);

    RoutineDataDestroy(data);
}

static void test_stop_wakes_sleep(void)
{
    RoutineData *data = RoutineDataNew();

    StoppableThread *thread = StoppableThreadStart(LoopUntilStopped, data);
    assert_true(thread != NULL);
    WaitForStart(data);

    /* The routine sleeps 60s per iteration, so this only returns quickly if
     * the stop interrupts the sleep. */
    const time_t start = time(NULL);
    assert_true(StoppableThreadStop(thread, TEST_TIMEOUT_SECS));
    assert_true(time(NULL) - start < 2);

    RoutineDataDestroy(data);
}

static void test_stop_already_returned(void)
{
    RoutineData *data = RoutineDataNew();

    StoppableThread *thread = StoppableThreadStart(ReturnImmediately, data);
    assert_true(thread != NULL);

    /* Whether or not the routine has already returned, it gets joined. */
    assert_true(StoppableThreadStop(thread, TEST_TIMEOUT_SECS));
    assert_int_equal(data->iterations, 1);

    RoutineDataDestroy(data);
}

static void test_sleep_non_positive(void)
{
    RoutineData *data = RoutineDataNew();

    const time_t start = time(NULL);
    StoppableThread *thread = StoppableThreadStart(SleepNonPositive, data);
    assert_true(thread != NULL);
    assert_true(StoppableThreadStop(thread, TEST_TIMEOUT_SECS));
    assert_true(time(NULL) - start < 2);
    assert_int_equal(data->iterations, 1);

    RoutineDataDestroy(data);
}

static void test_sleep_after_stop(void)
{
    RoutineData *data = RoutineDataNew();

    StoppableThread *thread = StoppableThreadStart(SleepAfterStop, data);
    assert_true(thread != NULL);
    WaitForStart(data);

    const time_t start = time(NULL);
    assert_true(StoppableThreadStop(thread, TEST_TIMEOUT_SECS));
    assert_true(time(NULL) - start < 2);
    assert_true(data->saw_stop);

    RoutineDataDestroy(data);
}

/* State for the timeout test. It's static rather than passed through arg,
 * because the thread is abandoned and may outlive the test function. */
static pthread_mutex_t ignore_stop_lock = PTHREAD_MUTEX_INITIALIZER;
static bool ignore_stop_release = false;
static ThreadedQueue *ignore_stop_started = NULL;
/* Keeps the abandoned handle reachable, so that it doesn't show up as a
 * leak under valgrind. */
static StoppableThread *abandoned_thread = NULL;

static bool IgnoreStopReleased(void)
{
    ThreadLock(&ignore_stop_lock);
    bool ret = ignore_stop_release;
    ThreadUnlock(&ignore_stop_lock);
    return ret;
}

/* Ignores stop requests, like a routine stuck in a blocking call would, until
 * the test releases it (or it's cancelled while in usleep()). */
static void IgnoreStop(ARG_UNUSED StoppableThread *thread, ARG_UNUSED void *arg)
{
    ThreadedQueuePush(ignore_stop_started, NULL);

    const time_t give_up = time(NULL) + TEST_TIMEOUT_SECS;
    while (!IgnoreStopReleased() && time(NULL) < give_up)
    {
        usleep(10000);
    }
}

static void test_stop_timeout(void)
{
    ignore_stop_started = ThreadedQueueNew(1, NULL);

    abandoned_thread = StoppableThreadStart(IgnoreStop, NULL);
    assert_true(abandoned_thread != NULL);
    void *item;
    assert_true(ThreadedQueuePop(ignore_stop_started, &item, TEST_TIMEOUT_SECS));

    /* The routine never checks for a stop, so this must give up after the
     * timeout, not wait for the routine to return. */
    const time_t start = time(NULL);
    assert_false(StoppableThreadStop(abandoned_thread, 1));
    const time_t elapsed = time(NULL) - start;
    assert_true(elapsed >= 1);
    assert_true(elapsed < TEST_TIMEOUT_SECS);

    /* Let the routine return in case it wasn't cancelled (no pthread_cancel()
     * on this platform). ignore_stop_started is leaked on purpose, since the
     * thread may not have finished with it. */
    ThreadLock(&ignore_stop_lock);
    ignore_stop_release = true;
    ThreadUnlock(&ignore_stop_lock);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_stop_running_thread),
        unit_test(test_stop_wakes_sleep),
        unit_test(test_stop_already_returned),
        unit_test(test_sleep_non_positive),
        unit_test(test_sleep_after_stop),
        unit_test(test_stop_timeout),
    };

    return run_tests(tests);
}
