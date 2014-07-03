#include <test.h>

#include <cf3.defs.h>
#include <cf3.extern.h>

#include <syslog_client.h>

char VFQNAME[CF_MAXVARSIZE];
char VPREFIX[CF_MAXVARSIZE];

static struct sockaddr *got_address;

#if SENDTO_RETURNS_SSIZE_T > 0
ssize_t sendto(ARG_UNUSED int sockfd, ARG_UNUSED const void *buf,
               size_t len,
               ARG_UNUSED int flags,
               const struct sockaddr *dest_addr,
               ARG_UNUSED socklen_t addrlen)
{
    got_address = xmemdup(dest_addr, sizeof(struct sockaddr_in));
    return len;
}
#else
/*
 * We might be naives by thinking that size_t, socklen_t and such are the same size as int.
 * Given that we are not using them here, we can live with that assumption.
 */
ssize_t sendto(ARG_UNUSED int sockfd, ARG_UNUSED const void *buf,
               int len,
               ARG_UNUSED int flags,
               const void *dest_addr,
               ARG_UNUSED int addrlen)
{
    got_address = xmemdup(dest_addr, sizeof(struct sockaddr_in));
    return len;
}
#endif // SENDTO_RETURNS_SSIZE_T > 0

static void test_set_port(void)
{
    SetSyslogPort(5678);
    RemoteSysLog(LOG_EMERG, "Test string");

    if (got_address->sa_family == AF_INET)
    {
        assert_int_equal(ntohs(((struct sockaddr_in *) got_address)->sin_port), 5678);
    }
    else if (got_address->sa_family == AF_INET6)
    {
        assert_int_equal(ntohs(((struct sockaddr_in6 *) got_address)->sin6_port), 5678);
    }

    free(got_address);
}

static void test_set_host(void)
{
    SetSyslogHost("127.0.0.55");
    RemoteSysLog(LOG_EMERG, "Test string");

    assert_int_equal(got_address->sa_family, AF_INET);

    assert_int_equal(ntohl(((struct sockaddr_in *) got_address)->sin_addr.s_addr), 0x7f000037);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_set_port),
        unit_test(test_set_host),
    };

    return run_tests(tests);
}
