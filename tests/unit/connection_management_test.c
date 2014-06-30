#include <test.h>

#include <item_lib.h>
#include <misc_lib.h>                                          /* xsnprintf */
#include <server.h>
#include <server_common.h>


#include <server.c>                                  /* PurgeOldConnections */


const int CONNECTION_MAX_AGE_SECONDS = SECONDS_PER_HOUR * 2;

/* NOTE: Invalid memory access has been seen in PurgeOldConnections().
         This does not always result in a segfault, but running this test
         in valgrind will detect it.                                     */


static void test_purge_old_connections_nochange(void)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS);
    PrependItem(&connections, "123.123.123.3", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 1);
    PrependItem(&connections, "123.123.123.2", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 100);
    PrependItem(&connections, "123.123.123.1", time_str);

    assert_int_equal(ListLen(connections), 3);

    PurgeOldConnections(&connections, time_now);

    assert_int_equal(ListLen(connections), 3);

    assert_true(IsItemIn(connections, "123.123.123.1"));
    assert_true(IsItemIn(connections, "123.123.123.2"));
    assert_true(IsItemIn(connections, "123.123.123.3"));

    DeleteItemList(connections);
}


static void test_purge_old_connections_purge_first(void)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 100);
    PrependItem(&connections, "123.123.123.3", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 2);
    PrependItem(&connections, "123.123.123.2", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS - 5);
    PrependItem(&connections, "123.123.123.1", time_str);

    assert_int_equal(ListLen(connections), 3);

    PurgeOldConnections(&connections, time_now);

    assert_int_equal(ListLen(connections), 2);

    assert_false(IsItemIn(connections, "123.123.123.1"));
    assert_true(IsItemIn(connections, "123.123.123.2"));
    assert_true(IsItemIn(connections, "123.123.123.3"));

    DeleteItemList(connections);
}


static void test_purge_old_connections_purge_middle(void)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS);
    PrependItem(&connections, "123.123.123.3", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS - 1);
    PrependItem(&connections, "123.123.123.2", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 100);
    PrependItem(&connections, "123.123.123.1", time_str);

    assert_int_equal(ListLen(connections), 3);

    PurgeOldConnections(&connections, time_now);

    assert_int_equal(ListLen(connections), 2);

    assert_true(IsItemIn(connections, "123.123.123.1"));
    assert_false(IsItemIn(connections, "123.123.123.2"));
    assert_true(IsItemIn(connections, "123.123.123.3"));

    DeleteItemList(connections);
}


static void test_purge_old_connections_purge_last(void)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS - 100);
    PrependItem(&connections, "123.123.123.3", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 10);
    PrependItem(&connections, "123.123.123.2", time_str);

    xsnprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS);
    PrependItem(&connections, "123.123.123.1", time_str);

    assert_int_equal(ListLen(connections), 3);

    PurgeOldConnections(&connections, time_now);

    assert_int_equal(ListLen(connections), 2);

    assert_true(IsItemIn(connections, "123.123.123.1"));
    assert_true(IsItemIn(connections, "123.123.123.2"));
    assert_false(IsItemIn(connections, "123.123.123.3"));

    DeleteItemList(connections);
}


int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_purge_old_connections_nochange),
        unit_test(test_purge_old_connections_purge_first),
        unit_test(test_purge_old_connections_purge_middle),
        unit_test(test_purge_old_connections_purge_last)
    };

    return run_tests(tests);
}
