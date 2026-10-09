#include <test.h>

#include <cmockery.h>
#include <client_code.h>       /* RemoteDirList */
#include <connection_info.h>   /* ConnectionInfo* */
#include <cfnet.h>             /* AgentConnection, CF_MORE, CF_DONE */
#include <net.h>               /* SendTransaction */
#include <item_lib.h>          /* Item, DeleteItemList */
#include <cf3.defs.h>          /* CFD_TERMINATOR */

#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#define TERM CFD_TERMINATOR

/* Entry names are read out of the struct dirent that RemoteDirList()
 * allocates for each of them. */
static const char *EntryName(const Item *ip)
{
    return ((const struct dirent *) ip->name)->d_name;
}

static size_t ItemListLen(const Item *list)
{
    size_t len = 0;
    for (const Item *ip = list; ip != NULL; ip = ip->next)
    {
        len++;
    }
    return len;
}

/* A server reply that does not end in the double '\0' must not make
 * RemoteDirList() read the bytes left in the receive buffer by the previous
 * reply. Here the stale bytes are a CFD_TERMINATOR, so an unbounded scan ends
 * the listing early and silently drops the entries of the last packet. */
static void test_dirlist_stops_at_end_of_packet(void)
{
    int sv[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    assert_int_equal(ret, 0);

    ConnectionInfo *server = ConnectionInfoNew();
    ConnectionInfoSetSocket(server, sv[1]);
    ConnectionInfoSetProtocolVersion(server, CF_PROTOCOL_CLASSIC);

    /* "entry1", end of packet, and a CFD_TERMINATOR that the client must not
     * reach because the double '\0' comes first. It stays in the receive
     * buffer at offset 8. */
    const char packet1[] = "entry1\0\0" TERM "\0";
    ret = SendTransaction(server, packet1, sizeof(packet1) - 1, CF_MORE);
    assert_int_equal(ret, 0);

    /* 7 bytes with no terminating '\0' of their own: the scan resumes at
     * offset 8, i.e. exactly on top of the CFD_TERMINATOR above. */
    const char packet2[] = "entry2x";
    ret = SendTransaction(server, packet2, sizeof(packet2) - 1, CF_MORE);
    assert_int_equal(ret, 0);

    /* The real last packet, which an unbounded scan never gets to read. */
    const char packet3[] = "entry3\0" TERM "\0\0";
    ret = SendTransaction(server, packet3, sizeof(packet3) - 1, CF_DONE);
    assert_int_equal(ret, 0);

    AgentConnection conn;
    memset(&conn, 0, sizeof(conn));
    conn.conn_info = ConnectionInfoNew();
    ConnectionInfoSetSocket(conn.conn_info, sv[0]);
    ConnectionInfoSetProtocolVersion(conn.conn_info, CF_PROTOCOL_CLASSIC);
    conn.this_server = "test-server";

    Item *list = RemoteDirList("/tmp", false, &conn);

    assert_true(list != NULL);
    assert_int_equal(ItemListLen(list), 3);
    assert_string_equal(EntryName(list), "entry1");
    assert_string_equal(EntryName(list->next), "entry2x");
    assert_string_equal(EntryName(list->next->next), "entry3");

    DeleteItemList(list);
    ConnectionInfoDestroy(&conn.conn_info);
    ConnectionInfoDestroy(&server);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_dirlist_stops_at_end_of_packet),
    };

    return run_tests(tests);
}
