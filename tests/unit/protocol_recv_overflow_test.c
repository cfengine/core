/*
 * Regression test for CFE-4687 / cfengine/core PR #6171:
 * off-by-one in the ProtocolGet() and ProtocolOpenDir() receive buffers
 * in libcfnet/protocol.c.
 *
 * Background
 * ----------
 * The receive primitives NUL-terminate what they read, and the count read
 * can equal the requested length:
 *
 *     TLSRecv()         tls_generic.c:807   buffer[received] = '\0';
 *     RecvSocketStream() classic.c:54        buffer[already]  = '\0';
 *
 * Both are documented to write up to "toget + 1" bytes (classic.c:5-7), so a
 * caller must hand them a buffer of at least toget + 1 bytes. net.c:151 and
 * ProtocolStat() (protocol.c:260) honor this with CF_BUFSIZE buffers.
 *
 * ProtocolOpenDir() and ProtocolGet() did not: they declared
 *
 *     char buf[CF_MSGSIZE];          // CF_MSGSIZE == CF_BUFSIZE - CF_INBAND_OFFSET
 *
 * and then received up to CF_MSGSIZE bytes into it. A peer that fills the
 * record makes "received" reach CF_MSGSIZE, so the terminating NUL is written
 * to buf[CF_MSGSIZE] -- one byte past the array. The byte count is peer
 * controlled (the transaction length field for OPENDIR, accepted up to
 * CF_MSGSIZE at net.c:214), so the write crosses the trust boundary.
 *
 * What this test does
 * -------------------
 * It drives the real ProtocolOpenDir() over a socketpair using the classic
 * protocol. The classic data path uses RecvSocketStream(), whose terminator
 * write is identical to the TLS path's TLSRecv(), so the same off-by-one is
 * surfaced without needing a TLS handshake. A "server" sends a single CF_DONE
 * transaction carrying exactly CF_MSGSIZE payload bytes -- the maximum
 * ReceiveTransaction() accepts.
 *
 * On unpatched code the terminating write lands at buf[CF_MSGSIZE] inside
 * ProtocolOpenDir()'s stack frame; under AddressSanitizer (the project's
 * "ASAN Unit Tests" CI job) this aborts with a stack-buffer-overflow. With the
 * fix (buf sized CF_BUFSIZE) the write is in bounds and the test completes.
 *
 * ProtocolGet() carries the identical defect and the identical one-line fix,
 * but receives via TLSRecv() directly, which requires a live SSL session;
 * there is no TLS-handshake harness in tests/unit, so it is not driven here.
 */

#include <test.h>

#include <cmockery.h>
#include <protocol.h>          /* ProtocolOpenDir */
#include <connection_info.h>   /* ConnectionInfo* */
#include <cfnet.h>             /* AgentConnection, CF_MSGSIZE, CF_DONE */
#include <net.h>              /* SendTransaction */
#include <sequence.h>         /* Seq, SeqDestroy */

#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

static void test_opendir_recv_terminator_overflow(void)
{
    int sv[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    assert_int_equal(ret, 0);

    /* --- malicious/non-conforming server side (sv[1]) --- */
    ConnectionInfo *server = ConnectionInfoNew();
    ConnectionInfoSetSocket(server, sv[1]);
    ConnectionInfoSetProtocolVersion(server, CF_PROTOCOL_CLASSIC);

    /* A record-filling reply: exactly CF_MSGSIZE payload bytes, the maximum
     * ReceiveTransaction() permits (net.c:214). CF_DONE so the client's
     * receive loop runs exactly once. Content is irrelevant -- the overflow
     * happens during the receive, before any payload is interpreted. */
    char payload[CF_MSGSIZE];
    memset(payload, 'A', sizeof(payload));

    ret = SendTransaction(server, payload, CF_MSGSIZE, CF_DONE);
    assert_int_equal(ret, 0);

    /* --- client side (sv[0]): the real vulnerable function --- */
    AgentConnection conn;
    memset(&conn, 0, sizeof(conn));
    conn.conn_info = ConnectionInfoNew();
    ConnectionInfoSetSocket(conn.conn_info, sv[0]);
    ConnectionInfoSetProtocolVersion(conn.conn_info, CF_PROTOCOL_CLASSIC);

    /* On unpatched code this overflows buf[CF_MSGSIZE] while receiving and
     * AddressSanitizer aborts here. On patched code it returns normally. */
    Seq *result = ProtocolOpenDir(&conn, "/tmp");

    /* Reaching this point means no out-of-bounds write occurred. */
    SeqDestroy(result);
    ConnectionInfoDestroy(&conn.conn_info);
    ConnectionInfoDestroy(&server);
    close(sv[0]);
    close(sv[1]);

    assert_true(true);
}

int main(void)
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_opendir_recv_terminator_overflow),
    };

    return run_tests(tests);
}
