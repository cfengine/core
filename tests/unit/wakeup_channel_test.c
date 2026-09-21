#include <test.h>

#include <wakeup_channel.h>
#include <logging.h>
#include <logging_priv.h>       /* LoggingPrivContext, LoggingPrivSetContext() */

static int captured_err_count = 0;

static char *CaptureErrorLogHook(ARG_UNUSED LoggingPrivContext *pctx, LogLevel level, const char *message)
{
    if (level == LOG_LEVEL_ERR)
    {
        captured_err_count++;
    }
    return (char *) message;
}

/* Non-blocking check of whether fd is readable, the way the reactor's
 * select() loop sees it. */
static bool IsReadable(int fd)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    struct timeval timeout = { 0 };
    int ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
    assert_true(ret >= 0);
    return ret > 0 && FD_ISSET(fd, &readfds);
}

static void test_open_close(void)
{
    WakeupChannel channel;
    assert_true(WakeupChannelOpen(&channel));

    assert_true(channel.fds[0] >= 0);
    assert_true(channel.fds[1] >= 0);
    assert_int_equal(WakeupChannelReadFd(&channel), channel.fds[0]);

    WakeupChannelClose(&channel);
    assert_int_equal(channel.fds[0], -1);
    assert_int_equal(channel.fds[1], -1);

    /* Closing again must be a no-op, not close whatever now reuses the fds. */
    WakeupChannelClose(&channel);
    assert_int_equal(channel.fds[0], -1);
    assert_int_equal(channel.fds[1], -1);
}

static void test_close_unopened(void)
{
    /* A channel that was never opened must not close fd 0 (stdin). */
    WakeupChannel channel = { .fds = { -1, -1 } };
    WakeupChannelClose(&channel);
    assert_int_equal(channel.fds[0], -1);
    assert_int_equal(channel.fds[1], -1);
}

static void test_not_readable_initially(void)
{
    WakeupChannel channel;
    assert_true(WakeupChannelOpen(&channel));

    assert_false(IsReadable(WakeupChannelReadFd(&channel)));

    WakeupChannelClose(&channel);
}

static void test_notify_then_drain(void)
{
    WakeupChannel channel;
    assert_true(WakeupChannelOpen(&channel));
    const int fd = WakeupChannelReadFd(&channel);

    WakeupChannelNotify(&channel);
    assert_true(IsReadable(fd));

    WakeupChannelDrain(&channel);
    assert_false(IsReadable(fd));

    WakeupChannelClose(&channel);
}

static void test_drain_clears_multiple_notifies(void)
{
    WakeupChannel channel;
    assert_true(WakeupChannelOpen(&channel));
    const int fd = WakeupChannelReadFd(&channel);

    for (int i = 0; i < 10; i++)
    {
        WakeupChannelNotify(&channel);
    }
    assert_true(IsReadable(fd));

    /* One drain must consume every pending notification, otherwise select()
     * would keep returning immediately. */
    WakeupChannelDrain(&channel);
    assert_false(IsReadable(fd));

    WakeupChannelClose(&channel);
}

static void test_drain_empty_does_not_block(void)
{
    WakeupChannel channel;
    assert_true(WakeupChannelOpen(&channel));

    /* The channel is non-blocking, so this must return straight away. */
    WakeupChannelDrain(&channel);
    assert_false(IsReadable(WakeupChannelReadFd(&channel)));

    WakeupChannelClose(&channel);
}

static void test_notify_when_full(void)
{
    WakeupChannel channel;
    assert_true(WakeupChannelOpen(&channel));
    const int fd = WakeupChannelReadFd(&channel);

    captured_err_count = 0;
    LoggingPrivContext log_ctx = { .log_hook = CaptureErrorLogHook };
    LoggingPrivSetContext(&log_ctx);

    /* Far more notifications than the socket buffer holds: once it's full,
     * Notify() must neither block nor log an error, since the reader is
     * already guaranteed to wake up. */
    for (int i = 0; i < 1000000; i++)
    {
        WakeupChannelNotify(&channel);
    }

    LoggingPrivSetContext(NULL);

    assert_int_equal(captured_err_count, 0);
    assert_true(IsReadable(fd));

    WakeupChannelDrain(&channel);
    assert_false(IsReadable(fd));

    /* Still usable after having been full. */
    WakeupChannelNotify(&channel);
    assert_true(IsReadable(fd));

    WakeupChannelClose(&channel);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_open_close),
        unit_test(test_close_unopened),
        unit_test(test_not_readable_initially),
        unit_test(test_notify_then_drain),
        unit_test(test_drain_clears_multiple_notifies),
        unit_test(test_drain_empty_does_not_block),
        unit_test(test_notify_when_full),
    };

    return run_tests(tests);
}
