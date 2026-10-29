#include <test.h>

#include <user_watcher.c>
#include <rlist.h>
#include <logging_priv.h>
#include <alloc.h>
#include <string_lib.h>

#include <pwd.h>
#include <string.h>
#include <unistd.h>

/* Unix only, like file_watcher_test. Tells what the lookup found from the
 * verbose log of the first check, since the first check never fires. The
 * .c file is included so that tests can fake an earlier check. */

#define ABSENT_USER "cfengine_user_watcher_test_absent"

typedef struct
{
    LoggingPrivContext ctx;
    LogLevel old_level;
    int count;         /* Number of VERBOSE messages captured */
    char message[256]; /* Last VERBOSE message captured */
} LogCapture;

static char *CaptureVerboseLogHook(
    LoggingPrivContext *pctx, LogLevel level, const char *message)
{
    if (level == LOG_LEVEL_VERBOSE)
    {
        LogCapture *capture = pctx->param;
        capture->count++;
        strlcpy(capture->message, message, sizeof(capture->message));
    }
    return (char *) message;
}

/* Captures VERBOSE messages with the log hook. The global level is set to
 * CRIT so they aren't printed. force_hook_level still runs the hook for
 * them. Stop with StopCapture(), which frees the returned capture. */
static LogCapture *StartCapture(void)
{
    LogCapture *capture = xcalloc(1, sizeof(LogCapture));
    capture->ctx.log_hook = CaptureVerboseLogHook;
    capture->ctx.param = capture;
    capture->ctx.force_hook_level = LOG_LEVEL_VERBOSE;
    LoggingPrivSetContext(&capture->ctx);
    capture->old_level = LogGetGlobalLevel();
    LogSetGlobalLevel(LOG_LEVEL_CRIT);
    return capture;
}

static void StopCapture(LogCapture *capture)
{
    LogSetGlobalLevel(capture->old_level);
    LoggingPrivSetContext(NULL);
    free(capture);
}

/* Copies the name of the user running the test, which always exists, to
 * name */
static void GetPresentUser(char *name, size_t name_size)
{
    const struct passwd *pw = getpwuid(getuid());
    assert_true(pw != NULL);
    strlcpy(name, pw->pw_name, name_size);
}

/* Returns a new state of a user watcher of the user with the given name */
static void *NewUserState(const char *name)
{
    Rlist *args = NULL;
    RlistAppendScalar(&args, name);
    void *state = UserWatcherStateNew(args);
    RlistDestroy(args);
    return state;
}

static void test_check_user_removed_present(void)
{
    char name[256];
    GetPresentUser(name, sizeof(name));
    char expected[512];
    snprintf(
        expected,
        sizeof(expected),
        "User '%s' is present, watching for its removal",
        name);

    LogCapture *capture = StartCapture();
    void *state = NewUserState(name);

    /* The first check is the reference, so it doesn't fire */
    assert_int_equal(
        UserWatcherCheckUserRemoved(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 1);
    assert_string_equal(capture->message, expected);

    /* Still present: not removed, and not logged again */
    assert_int_equal(
        UserWatcherCheckUserRemoved(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 1);

    UserWatcherStateDestroy(state);
    StopCapture(capture);
}

static void test_check_user_removed_absent(void)
{
    LogCapture *capture = StartCapture();
    void *state = NewUserState(ABSENT_USER);

    /* Absent, so nothing to watch for yet and nothing logged */
    assert_int_equal(
        UserWatcherCheckUserRemoved(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(
        UserWatcherCheckUserRemoved(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 0);

    UserWatcherStateDestroy(state);
    StopCapture(capture);
}

static void test_check_user_added_absent(void)
{
    LogCapture *capture = StartCapture();
    void *state = NewUserState(ABSENT_USER);

    /* The first check is the reference, so it doesn't fire */
    assert_int_equal(UserWatcherCheckUserAdded(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 1);
    assert_string_equal(
        capture->message,
        "User '" ABSENT_USER "' is absent, watching for its addition");

    /* Still absent: not added, and not logged again */
    assert_int_equal(UserWatcherCheckUserAdded(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 1);

    UserWatcherStateDestroy(state);
    StopCapture(capture);
}

static void test_check_user_added_present(void)
{
    char name[256];
    GetPresentUser(name, sizeof(name));

    LogCapture *capture = StartCapture();
    void *state = NewUserState(name);

    /* Present, so nothing to watch for yet and nothing logged */
    assert_int_equal(UserWatcherCheckUserAdded(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(UserWatcherCheckUserAdded(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 0);

    UserWatcherStateDestroy(state);
    StopCapture(capture);
}

static void test_check_user_added_fires(void)
{
    char name[256];
    GetPresentUser(name, sizeof(name));
    char expected[512];
    snprintf(
        expected,
        sizeof(expected),
        "Observed addition of user '%s' at ",
        name);

    LogCapture *capture = StartCapture();
    UserWatcherState *state = NewUserState(name);

    /* Fake an earlier check that saw the user absent */
    state->last_checked = time(NULL);
    state->last_exists = false;

    assert_int_equal(UserWatcherCheckUserAdded(state), WATCHER_CHECK_EVENT);
    assert_int_equal(capture->count, 1);
    assert_true(StringStartsWith(capture->message, expected));

    /* Fires only once */
    assert_int_equal(UserWatcherCheckUserAdded(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 1);

    UserWatcherStateDestroy(state);
    StopCapture(capture);
}

static void test_check_user_removed_fires(void)
{
    LogCapture *capture = StartCapture();
    UserWatcherState *state = NewUserState(ABSENT_USER);

    /* Fake an earlier check that saw the user present */
    state->last_checked = time(NULL);
    state->last_exists = true;

    assert_int_equal(
        UserWatcherCheckUserRemoved(state), WATCHER_CHECK_EVENT);
    assert_int_equal(capture->count, 1);
    assert_true(StringStartsWith(
        capture->message,
        "Observed removal of user '" ABSENT_USER "' at "));

    /* Fires only once */
    assert_int_equal(
        UserWatcherCheckUserRemoved(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 1);

    UserWatcherStateDestroy(state);
    StopCapture(capture);
}

static void test_check_empty_name(void)
{
    LogCapture *capture = StartCapture();

    /* An empty name is a user that doesn't exist, not a lookup error */
    void *state = NewUserState("");
    assert_int_equal(UserWatcherCheckUserAdded(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(capture->count, 1);
    assert_string_equal(
        capture->message, "User '' is absent, watching for its addition");
    UserWatcherStateDestroy(state);

    StopCapture(capture);
}

static void test_state_destroy_null(void)
{
    UserWatcherStateDestroy(NULL);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] = {
        unit_test(test_check_user_removed_present),
        unit_test(test_check_user_removed_absent),
        unit_test(test_check_user_added_absent),
        unit_test(test_check_user_added_present),
        unit_test(test_check_user_added_fires),
        unit_test(test_check_user_removed_fires),
        unit_test(test_check_empty_name),
        unit_test(test_state_destroy_null),
    };

    return run_tests(tests);
}
