#include <test.h>

#include <cf3.defs.h>
#include <cf3.extern.h>

#include <syslog_client.h>
#include <string_lib.h>

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
int sendto(ARG_UNUSED int sockfd, ARG_UNUSED const void *buf,
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

#define check_level(str, lvl) \
{\
    assert_int_equal(LogLevelFromString(str), lvl);\
    assert_true(StringSafeEqual_IgnoreCase(str, LogLevelToString(lvl)));\
}

static void test_log_level(void)
{
    check_level("CRITICAL", LOG_LEVEL_CRIT);
    check_level("Error", LOG_LEVEL_ERR);
    check_level("warning", LOG_LEVEL_WARNING);
    check_level("notice", LOG_LEVEL_NOTICE);
    check_level("info", LOG_LEVEL_INFO);
    check_level("verbose", LOG_LEVEL_VERBOSE);
    check_level("debug", LOG_LEVEL_DEBUG);

    // LogLevelFromString should accept half typed strings:
    assert_int_equal(LogLevelFromString("CRIT"), LOG_LEVEL_CRIT);
    assert_int_equal(LogLevelFromString("ERR"), LOG_LEVEL_ERR);
    assert_int_equal(LogLevelFromString("warn"), LOG_LEVEL_WARNING);
    assert_int_equal(LogLevelFromString("I"), LOG_LEVEL_INFO);
    assert_int_equal(LogLevelFromString("i"), LOG_LEVEL_INFO);
    assert_int_equal(LogLevelFromString("information"), LOG_LEVEL_INFO);
    assert_int_equal(LogLevelFromString("v"), LOG_LEVEL_VERBOSE);

    //LogLevelFromString should return NOTHING in case of error:
    assert_int_equal(LogLevelFromString(""), LOG_LEVEL_NOTHING);
    assert_int_equal(LogLevelFromString("IX"), LOG_LEVEL_NOTHING);
    assert_int_equal(LogLevelFromString("Infotmation"), LOG_LEVEL_NOTHING);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_set_port),
        unit_test(test_set_host),
        unit_test(test_log_level),
    };

    return run_tests(tests);
}
