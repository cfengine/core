#include "test.h"

#include "cfnet.h"


/* Test IsIPAddress() with various IP addresses. This also is a good test on
 * whether getaddrinfo() is buggy. TODO run this during ./configure.  */


/* Valid IPv4 addresses. */
static void test_isipaddress_ipv4(void)
{
    assert_int_equal(IsIPAddress("0.0.0.0"), AF_INET);
    assert_int_equal(IsIPAddress("255.255.255.255"), AF_INET);
    assert_int_equal(IsIPAddress("1.1.1.1"), AF_INET);
    assert_int_equal(IsIPAddress("1.2.3.4"), AF_INET);
    assert_int_equal(IsIPAddress("192.168.0.1"), AF_INET);
}

/* Valid IPv6 addresses. */
static void test_isipaddress_ipv6(void)
{
    assert_int_equal(IsIPAddress("0000:0000:0000:0000:0000:0000:0000:0000"), AF_INET6);
    assert_int_equal(IsIPAddress("0:0:0:0:0:0:0:0"), AF_INET6);
    assert_int_equal(IsIPAddress("a:b:c:d::1"), AF_INET6);
    assert_int_equal(IsIPAddress("a:b:c:d:0:1:2:3"), AF_INET6);
    assert_int_equal(IsIPAddress("0:1:2::4"), AF_INET6);
    assert_int_equal(IsIPAddress("0::2:3:4"), AF_INET6);
    assert_int_equal(IsIPAddress("::3:4"), AF_INET6);
    /* CAPS should be ok but not recommended */
    assert_int_equal(IsIPAddress("A:B:C:D:E:F:0:1"), AF_INET6);

    /* Various addresses from wikipedia, article IPv6_address */
    assert_int_equal(IsIPAddress("2001:0db8:85a3:0000:0000:8a2e:0370:7334"), AF_INET6);
    assert_int_equal(IsIPAddress("2001:db8:85a3:0:0:8a2e:370:7334"), AF_INET6);
    assert_int_equal(IsIPAddress("2001:db8:85a3::8a2e:370:7334"), AF_INET6);
    /* localhost */
    assert_int_equal(IsIPAddress("0:0:0:0:0:0:0:1"), AF_INET6);
    assert_int_equal(IsIPAddress("::"), AF_INET6);
    /* unspecified */
    assert_int_equal(IsIPAddress("0:0:0:0:0:0:0:0"), AF_INET6);
    assert_int_equal(IsIPAddress("::1"), AF_INET6);

    assert_int_equal(IsIPAddress("::ffff:c000:0280"), AF_INET6);
    /* dotted quad notation of the previous */
    assert_int_equal(IsIPAddress("::ffff:192.0.2.128"), AF_INET6);
}

/* Invalid addresses. */
static void test_isipaddress_invalid(void)
{
    assert_true(!IsIPAddress("1.1.1.256"));
    assert_true(!IsIPAddress("1.1.256.1"));
    assert_true(!IsIPAddress("1.256.1.1"));
    assert_true(!IsIPAddress("256.1.1.1"));
    assert_true(!IsIPAddress("256.256.256.256"));
    assert_true(!IsIPAddress("a.b.c.d"));
    assert_true(!IsIPAddress("blah"));
    assert_true(!IsIPAddress(""));
    assert_true(!IsIPAddress("   "));
    assert_true(!IsIPAddress("2_3_4_5"));
    assert_true(!IsIPAddress("localhost"));
    assert_true(!IsIPAddress("localhost.localdomain"));
    assert_true(!IsIPAddress("-1.2.3.4"));

    /* glibc 2.15 accepts the following as valid IP addresses trying to
     * imitate BSD inet_aton() for class C, B, and A respectively. See page 4
     * of http://tools.ietf.org/html/draft-main-ipaddr-text-rep-02 */
//    assert_true(!IsIPAddress("34"));
//    assert_true(!IsIPAddress("34.98"));
//    assert_true(!IsIPAddress("34.98.23"));
//    assert_true(!IsIPAddress("1.2.3.4 blah"));

    assert_true(!IsIPAddress(":::"));
    assert_true(!IsIPAddress(":::::::"));
    assert_true(!IsIPAddress("A:B:C:D:E::F:0::1"));
    assert_true(!IsIPAddress(":A:B:C:D:E::F:0:1:"));
    assert_true(!IsIPAddress("A:B:C:D:E:F:0:1:::"));
    assert_true(!IsIPAddress("2001:db8::1::1"));
}


int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] = {
        unit_test(test_isipaddress_ipv4),
#if (HAVE_GETADDRINFO)
/* Our libcompat getaddrinfo() is IPv4 only so it fails here */
        unit_test(test_isipaddress_ipv6),
#endif
        unit_test(test_isipaddress_invalid)
    };

    return run_tests(tests);
}
