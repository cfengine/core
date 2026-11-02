#include <stddef.h>
#include <test.h>

/* Included, to test its static functions */
#include <package_watcher.c>

#include <unistd.h>

/* How long to wait for the listing thread before failing */
#define WAIT_TIMEOUT_SECS 10

/* Returns a body of a package module that doesn't exist, with the given
 * option if not NULL. */
static PackageModuleBody *NewMissingModuleBody(const char *option)
{
    PackageModuleBody *module = xcalloc(1, sizeof(PackageModuleBody));
    module->name = xstrdup("missing");
    module->module_path = xstrdup("/nonexistent/package_watcher_test");
    if (option != NULL)
    {
        RlistAppendScalar(&module->options, option);
    }
    return module;
}

static bool IsListed(PackageListing *listing)
{
    ThreadLock(&listing->mutex);
    const bool listed = (listing->listed_at != 0);
    ThreadUnlock(&listing->mutex);
    return listed;
}

/* Waits until the listing thread has finished its first listing */
static void WaitForFirstListing(PackageListing *listing)
{
    const time_t deadline = time(NULL) + WAIT_TIMEOUT_SECS;
    while (!IsListed(listing))
    {
        assert_true(time(NULL) < deadline);
        usleep(10000); /* 20ms */
    }
}

/* A listing without a thread, whose result the tests set */
static PackageListing *NewFakeListing(void)
{
    PackageListing *listing = xcalloc(1, sizeof(PackageListing));
    pthread_mutex_init(&listing->mutex, NULL);
    return listing;
}

/* Sets the result of a fake listing: only name installed, or nothing if
 * name is NULL */
static void SetFakeListing(PackageListing *listing, const char *name)
{
    StringSetDestroy(listing->installed);
    listing->installed = StringSetNew();
    if (name != NULL)
    {
        StringSetAdd(listing->installed, xstrdup(name));
    }
    listing->listed_at = time(NULL);
}

static void SetFakeListingFailed(PackageListing *listing)
{
    StringSetDestroy(listing->installed);
    listing->installed = NULL;
    listing->listed_at = time(NULL);
}

/* Returns a state watching package foo in a fake listing */
static PackageWatcherState *NewFakeState(void)
{
    PackageWatcherState *state = xcalloc(1, sizeof(PackageWatcherState));
    state->name = xstrdup("foo");
    state->listing = NewFakeListing();
    return state;
}

static void DestroyFakeState(PackageWatcherState *state)
{
    PackageListingDestroy(state->listing);
    free(state->name);
    free(state);
}

static void test_parse_installed_packages(void)
{
    Rlist *response = NULL;
    RlistAppendScalar(&response, "Name=foo");
    RlistAppendScalar(&response, "Version=1.0");
    RlistAppendScalar(&response, "Architecture=amd64");
    RlistAppendScalar(&response, "Name=bar");
    RlistAppendScalar(&response, "Version=2.0");
    RlistAppendScalar(&response, "Architecture=amd64");
    /* The same package for another architecture */
    RlistAppendScalar(&response, "Name=bar");
    RlistAppendScalar(&response, "Version=2.0");
    RlistAppendScalar(&response, "Architecture=i386");

    StringSet *installed = ParseInstalledPackages(response, "fake");
    assert_true(installed != NULL);
    assert_int_equal(StringSetSize(installed), 2);
    assert_true(StringSetContains(installed, "foo"));
    assert_true(StringSetContains(installed, "bar"));

    StringSetDestroy(installed);
    RlistDestroy(response);
}

static void test_parse_skips_malformed_records(void)
{
    Rlist *response = NULL;
    /* No architecture */
    RlistAppendScalar(&response, "Name=foo");
    RlistAppendScalar(&response, "Version=1.0");
    RlistAppendScalar(&response, "Name=bar");
    RlistAppendScalar(&response, "Version=2.0");
    RlistAppendScalar(&response, "Architecture=amd64");
    /* No version, in the last record */
    RlistAppendScalar(&response, "Name=baz");
    RlistAppendScalar(&response, "Architecture=amd64");

    /* The malformed records of foo and baz are skipped, and the listing
     * still succeeds with the complete record of bar */
    StringSet *installed = ParseInstalledPackages(response, "fake");
    assert_true(installed != NULL);
    assert_int_equal(StringSetSize(installed), 1);
    assert_true(StringSetContains(installed, "bar"));

    StringSetDestroy(installed);
    RlistDestroy(response);
}

static void test_parse_empty_reply(void)
{
    /* Nothing installed, not a failure */
    StringSet *installed = ParseInstalledPackages(NULL, "fake");
    assert_true(installed != NULL);
    assert_int_equal(StringSetSize(installed), 0);
    StringSetDestroy(installed);
}

static void test_parse_error_reply(void)
{
    Rlist *response = NULL;
    RlistAppendScalar(&response, "Name=foo");
    RlistAppendScalar(&response, "Version=1.0");
    RlistAppendScalar(&response, "Architecture=amd64");
    RlistAppendScalar(&response, "ErrorMessage=Something went wrong");

    assert_true(ParseInstalledPackages(response, "fake") == NULL);
    RlistDestroy(response);
}

static void test_check_installed_transitions(void)
{
    PackageWatcherState *state = NewFakeState();

    /* Not listed yet: no observation */
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);
    assert_false(state->observed);

    /* Not installed (the first observation is the reference) */
    SetFakeListing(state->listing, NULL);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);

    /* Installed: the event happens once, not while it stays installed */
    SetFakeListing(state->listing, "foo");
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_EVENT);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);

    /* Uninstalled, then installed again: the event happens again */
    SetFakeListing(state->listing, NULL);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);
    SetFakeListing(state->listing, "foo");
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_EVENT);

    /* Another package installed: no event */
    SetFakeListing(state->listing, NULL);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);
    SetFakeListing(state->listing, "bar");
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);

    DestroyFakeState(state);
}

static void test_check_installed_at_start(void)
{
    PackageWatcherState *state = NewFakeState();

    /* Installed at the first observation: the reference, not an event */
    SetFakeListing(state->listing, "foo");
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);

    DestroyFakeState(state);
}

static void test_check_uninstalled_transitions(void)
{
    PackageWatcherState *state = NewFakeState();

    /* Installed (the first observation is the reference) */
    SetFakeListing(state->listing, "foo");
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_NO_EVENT);

    /* Uninstalled: the event happens once, not while it stays uninstalled */
    SetFakeListing(state->listing, NULL);
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_EVENT);
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_NO_EVENT);

    /* Installed, then uninstalled again: the event happens again */
    SetFakeListing(state->listing, "foo");
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_NO_EVENT);
    SetFakeListing(state->listing, NULL);
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_EVENT);

    DestroyFakeState(state);
}

static void test_check_uninstalled_absent_at_start(void)
{
    PackageWatcherState *state = NewFakeState();

    /* Not installed at the first observation: the reference, not an event */
    SetFakeListing(state->listing, NULL);
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_NO_EVENT);
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_NO_EVENT);

    DestroyFakeState(state);
}

static void test_check_failed_listing(void)
{
    PackageWatcherState *state = NewFakeState();

    /* A failed first listing: an error, and no observation */
    SetFakeListingFailed(state->listing);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_ERROR);
    assert_false(state->observed);

    /* Not installed (the reference) */
    SetFakeListing(state->listing, NULL);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_NO_EVENT);

    /* A failed listing is not taken as "nothing installed", and keeps the
     * last observation, so the installation is seen after it */
    SetFakeListingFailed(state->listing);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_ERROR);
    SetFakeListing(state->listing, "foo");
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_EVENT);

    /* Installed, then a failed listing: no uninstallation */
    SetFakeListingFailed(state->listing);
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_ERROR);
    SetFakeListing(state->listing, "foo");
    assert_int_equal(PackageWatcherCheckPackageUninstalled(state), WATCHER_CHECK_NO_EVENT);

    DestroyFakeState(state);
}

static void test_listings_shared(void)
{
    PackageModuleBody *module = NewMissingModuleBody(NULL);
    PackageModuleBody *other = NewMissingModuleBody("option=1");

    /* The same module shares a listing, other options don't */
    PackageListing *first = PackageListingAcquire(module);
    PackageListing *second = PackageListingAcquire(module);
    PackageListing *third = PackageListingAcquire(other);
    assert_true(first != NULL);
    assert_true(third != NULL);
    assert_true(first == second);
    assert_true(first != third);
    assert_int_equal(first->refcount, 2);
    assert_int_equal(SeqLength(listings), 2);

    /* Removed once the last watcher releases it */
    PackageListingRelease(first);
    assert_int_equal(SeqLength(listings), 2);
    PackageListingRelease(second);
    assert_int_equal(SeqLength(listings), 1);
    PackageListingRelease(third);
    assert_int_equal(SeqLength(listings), 0);

    PackageModuleBodyDestroy(module);
    PackageModuleBodyDestroy(other);
}

static void test_listing_module_cannot_run(void)
{
    PackageModuleBody *module = NewMissingModuleBody(NULL);

    PackageWatcherState *state = PackageWatcherStateNewFromModule("foo", module);
    assert_true(state != NULL);

    /* A failed listing, not "nothing installed" */
    WaitForFirstListing(state->listing);
    assert_int_equal(PackageWatcherCheckPackageInstalled(state), WATCHER_CHECK_ERROR);
    assert_false(state->observed);

    PackageWatcherStateDestroy(state);
    assert_int_equal(SeqLength(listings), 0);
    PackageModuleBodyDestroy(module);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_parse_installed_packages),
        unit_test(test_parse_skips_malformed_records),
        unit_test(test_parse_empty_reply),
        unit_test(test_parse_error_reply),
        unit_test(test_check_installed_transitions),
        unit_test(test_check_installed_at_start),
        unit_test(test_check_uninstalled_transitions),
        unit_test(test_check_uninstalled_absent_at_start),
        unit_test(test_check_failed_listing),
        unit_test(test_listings_shared),
        unit_test(test_listing_module_cannot_run),
    };

    return run_tests(tests);
}
