#include "cf3.defs.h"
#include "item_lib.h"
#include "server.h"


#include <setjmp.h>
#include <cmockery.h>

const int CONNECTION_MAX_AGE_SECONDS = SECONDS_PER_HOUR * 2;

/* NOTE: Invalid memory access has been seen in PurgeOldConnections().
         This does not always result in a segfault, but running this test
         in valgrind will detect it.                                     */


static void test_purge_old_connections_nochange(void **state)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS);
    PrependItem(&connections, "123.123.123.3", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 1);
    PrependItem(&connections, "123.123.123.2", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 100);
    PrependItem(&connections, "123.123.123.1", time_str);

    assert_int_equal(ListLen(connections), 3);

    PurgeOldConnections(&connections, time_now);

    assert_int_equal(ListLen(connections), 3);

    assert_true(IsItemIn(connections, "123.123.123.1"));
    assert_true(IsItemIn(connections, "123.123.123.2"));
    assert_true(IsItemIn(connections, "123.123.123.3"));

    DeleteItemList(connections);
}


static void test_purge_old_connections_purge_first(void **state)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 100);
    PrependItem(&connections, "123.123.123.3", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 2);
    PrependItem(&connections, "123.123.123.2", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS - 5);
    PrependItem(&connections, "123.123.123.1", time_str);

    assert_int_equal(ListLen(connections), 3);

    PurgeOldConnections(&connections, time_now);

    assert_int_equal(ListLen(connections), 2);

    assert_false(IsItemIn(connections, "123.123.123.1"));
    assert_true(IsItemIn(connections, "123.123.123.2"));
    assert_true(IsItemIn(connections, "123.123.123.3"));

    DeleteItemList(connections);
}


static void test_purge_old_connections_purge_middle(void **state)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS);
    PrependItem(&connections, "123.123.123.3", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS - 1);
    PrependItem(&connections, "123.123.123.2", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 100);
    PrependItem(&connections, "123.123.123.1", time_str);

    assert_int_equal(ListLen(connections), 3);

    PurgeOldConnections(&connections, time_now);

    assert_int_equal(ListLen(connections), 2);

    assert_true(IsItemIn(connections, "123.123.123.1"));
    assert_false(IsItemIn(connections, "123.123.123.2"));
    assert_true(IsItemIn(connections, "123.123.123.3"));

    DeleteItemList(connections);
}


static void test_purge_old_connections_purge_last(void **state)
{
    const time_t time_now = 100000;

    Item *connections = NULL;
    char time_str[64];

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS - 100);
    PrependItem(&connections, "123.123.123.3", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS + 10);
    PrependItem(&connections, "123.123.123.2", time_str);

    snprintf(time_str, sizeof(time_str), "%ld", time_now - CONNECTION_MAX_AGE_SECONDS);
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
    const UnitTest tests[] =
        {
            unit_test(test_purge_old_connections_nochange),
            unit_test(test_purge_old_connections_purge_first),
            unit_test(test_purge_old_connections_purge_middle),
            unit_test(test_purge_old_connections_purge_last)
        };

    return run_tests(tests);
}


/* stubs */

int ReceiveCollectCall(struct ServerConnectionState *conn, char *sendbuffer)
{
    return false;
}

int ReturnLiteralData(char *handle, char *ret)
{
    return 0;
}

int Nova_ReturnQueryData(ServerConnectionState *conn, char *menu)
{
    return false;
}
