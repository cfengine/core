#include "cf3.defs.h"
#include "cf3.extern.h"

#include "transaction.h"

#include <setjmp.h>
#include <cmockery.h>

static struct sockaddr *got_address;

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    got_address = xmemdup(dest_addr, sizeof(struct sockaddr_in));
    return len;
}

static void test_set_port(void **state)
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

static void test_set_host(void **state)
{
    SetSyslogHost("127.0.0.55");
    RemoteSysLog(LOG_EMERG, "Test string");

    assert_int_equal(got_address->sa_family, AF_INET);

    assert_int_equal(ntohl(((struct sockaddr_in *) got_address)->sin_addr.s_addr), 0x7f000037);
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_set_port),
        unit_test(test_set_host),
    };

    return run_tests(tests);
}
