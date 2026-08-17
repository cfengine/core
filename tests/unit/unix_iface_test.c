#include <test.h>

#include <unix_iface.c>

/*
 * FindLowestMetricDefaultRoute() operates on the JSON produced by parsing
 * /proc/net/route, after NetworkingRoutesPostProcessInfo() has annotated
 * each route with "active_default_gateway" and a numeric "metric". The
 * routes here are constructed directly in that shape, so the selection
 * logic can be exercised on any platform.
 */
static JsonElement *AppendRoute(
    JsonElement *routes, const char *gateway, int metric, bool active)
{
    JsonElement *route = JsonObjectCreate(3);
    JsonObjectAppendString(route, "gateway", gateway);
    JsonObjectAppendInteger(route, "metric", metric);
    JsonObjectAppendBool(route, "active_default_gateway", active);
    JsonArrayAppendElement(routes, route);
    return route;
}

static void test_no_routes(void)
{
    JsonElement *routes = JsonArrayCreate(1);

    assert_true(FindLowestMetricDefaultRoute(routes) == NULL);

    JsonDestroy(routes);
}

static void test_no_active_default_route(void)
{
    JsonElement *routes = JsonArrayCreate(2);
    AppendRoute(routes, "192.168.0.1", 0, false);
    AppendRoute(routes, "192.168.0.2", 100, false);

    assert_true(FindLowestMetricDefaultRoute(routes) == NULL);

    JsonDestroy(routes);
}

static void test_single_active_default_route(void)
{
    JsonElement *routes = JsonArrayCreate(2);
    AppendRoute(routes, "192.168.0.1", 0, false);
    JsonElement *expected = AppendRoute(routes, "192.168.0.2", 1024, true);

    assert_true(FindLowestMetricDefaultRoute(routes) == expected);

    JsonDestroy(routes);
}

static void test_lowest_metric_first(void)
{
    JsonElement *routes = JsonArrayCreate(2);
    JsonElement *expected = AppendRoute(routes, "192.168.0.1", 100, true);
    AppendRoute(routes, "192.168.0.2", 600, true);

    assert_true(FindLowestMetricDefaultRoute(routes) == expected);

    JsonDestroy(routes);
}

static void test_lowest_metric_last(void)
{
    /* The lowest metric must win even when it appears after another
     * active default route; this is the case CFE-4723 got wrong. */
    JsonElement *routes = JsonArrayCreate(3);
    AppendRoute(routes, "192.168.0.1", 600, true);
    AppendRoute(routes, "192.168.0.2", 100, true);
    JsonElement *expected = AppendRoute(routes, "192.168.0.3", 50, true);

    const JsonElement *found = FindLowestMetricDefaultRoute(routes);
    assert_true(found != NULL);
    assert_string_equal(JsonObjectGetAsString(found, "gateway"), "192.168.0.3");
    assert_true(found == expected);

    JsonDestroy(routes);
}

static void test_equal_metrics_keep_first(void)
{
    JsonElement *routes = JsonArrayCreate(2);
    JsonElement *expected = AppendRoute(routes, "192.168.0.1", 100, true);
    AppendRoute(routes, "192.168.0.2", 100, true);

    assert_true(FindLowestMetricDefaultRoute(routes) == expected);

    JsonDestroy(routes);
}

static void test_inactive_and_incomplete_routes_are_skipped(void)
{
    JsonElement *routes = JsonArrayCreate(3);

    /* Lower metric, but not an active default gateway. */
    AppendRoute(routes, "192.168.0.1", 1, false);

    /* Active, but without a usable metric. */
    JsonElement *no_metric = JsonObjectCreate(2);
    JsonObjectAppendString(no_metric, "gateway", "192.168.0.2");
    JsonObjectAppendBool(no_metric, "active_default_gateway", true);
    JsonArrayAppendElement(routes, no_metric);

    JsonElement *expected = AppendRoute(routes, "192.168.0.3", 600, true);

    assert_true(FindLowestMetricDefaultRoute(routes) == expected);

    JsonDestroy(routes);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_no_routes),
        unit_test(test_no_active_default_route),
        unit_test(test_single_active_default_route),
        unit_test(test_lowest_metric_first),
        unit_test(test_lowest_metric_last),
        unit_test(test_equal_metrics_keep_first),
        unit_test(test_inactive_and_incomplete_routes_are_skipped),
    };

    return run_tests(tests);
}
