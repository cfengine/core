#include <test.h>

#include <alloc.h>
#include <addr_lib.h>


static void test_ParseHostPort()
{
    char *hostname, *port;

    char test_string[64];

    // Domain name:
    ParseHostPort(strcpy(test_string, "www.cfengine.com"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "www.cfengine.com:80"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "www.cfengine.com:"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "localhost"), &hostname, &port);
    assert_string_equal(hostname, "localhost");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "localhost:"), &hostname, &port);
    assert_string_equal(hostname, "localhost");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "localhost:80"), &hostname, &port);
    assert_string_equal(hostname, "localhost");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[localhost]"), &hostname, &port);
    assert_string_equal(hostname, "localhost");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[localhost]:"), &hostname, &port);
    assert_string_equal(hostname, "localhost");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[localhost]:80"), &hostname, &port);
    assert_string_equal(hostname, "localhost");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[www.cfengine.com]"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[www.cfengine.com]:80"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[www.cfengine.com]:"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    // IPv4:
    ParseHostPort(strcpy(test_string, "1.2.3.4"), &hostname, &port);
    assert_string_equal(hostname, "1.2.3.4");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "1.2.3.4:80"), &hostname, &port);
    assert_string_equal(hostname, "1.2.3.4");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "1.2.3.4:"), &hostname, &port);
    assert_string_equal(hostname, "1.2.3.4");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    // IPv6 with square brackets:
    ParseHostPort(strcpy(test_string, "[ffff::dd:12:34]"), &hostname, &port);
    assert_string_equal(hostname, "ffff::dd:12:34");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[ffff::dd:12:34]:80"), &hostname, &port);
    assert_string_equal(hostname, "ffff::dd:12:34");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[ffff::dd:12:34]:"), &hostname, &port);
    assert_string_equal(hostname, "ffff::dd:12:34");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    // IPv6 without square brackets:
    ParseHostPort(strcpy(test_string, "ffff::dd:12:34"), &hostname, &port);
    assert_string_equal(hostname, "ffff::dd:12:34");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    // IPv4 mapped IPv6 addresses:
    ParseHostPort(strcpy(test_string, "::ffff:192.0.2.128"), &hostname, &port);
    assert_string_equal(hostname, "::ffff:192.0.2.128");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[::ffff:192.0.2.128]"), &hostname, &port);
    assert_string_equal(hostname, "::ffff:192.0.2.128");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    /***** CORNER CASES *****/

    ParseHostPort(strcpy(test_string, "[1.2.3.4"), &hostname, &port);
    assert_string_equal(hostname, "1.2.3.4");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, ""), &hostname, &port);
    assert_int_equal(hostname, NULL);
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[]"), &hostname, &port);
    assert_int_equal(hostname, NULL);
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, "[]:"), &hostname, &port);
    assert_int_equal(hostname, NULL);
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(strcpy(test_string, ":"), &hostname, &port);
    assert_int_equal(hostname, NULL);
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;
}

static void test_ParseHostPort_address_type()
{
    char *hostname, *port;
    char test_string[64];

#define ASSERT_ADDRESS_TYPE(input, type) \
    assert_int_equal(ParseHostPort(strcpy(test_string, input), &hostname, &port), type)

    ASSERT_ADDRESS_TYPE("1.2.3.4", ADDRESS_TYPE_IPV4);
    ASSERT_ADDRESS_TYPE("1.2.3.4:80", ADDRESS_TYPE_IPV4);
    ASSERT_ADDRESS_TYPE("[1.2.3.4]:80", ADDRESS_TYPE_IPV4);
    ASSERT_ADDRESS_TYPE("ffff::dd:12:34", ADDRESS_TYPE_IPV6);
    ASSERT_ADDRESS_TYPE("[ffff::dd:12:34]:80", ADDRESS_TYPE_IPV6);
    ASSERT_ADDRESS_TYPE("::ffff:192.0.2.128", ADDRESS_TYPE_IPV6);
    ASSERT_ADDRESS_TYPE("fe80::1%eth0", ADDRESS_TYPE_IPV6);
    ASSERT_ADDRESS_TYPE("www.cfengine.com:80", ADDRESS_TYPE_OTHER);
    ASSERT_ADDRESS_TYPE("localhost", ADDRESS_TYPE_OTHER);
    ASSERT_ADDRESS_TYPE("[1.2.3.4", ADDRESS_TYPE_OTHER);
    ASSERT_ADDRESS_TYPE("", ADDRESS_TYPE_OTHER);

    /* Hostnames starting with a digit are not IPv4 addresses */
    ASSERT_ADDRESS_TYPE("1password.com", ADDRESS_TYPE_OTHER);
    ASSERT_ADDRESS_TYPE("1password.com:80", ADDRESS_TYPE_OTHER);
    ASSERT_ADDRESS_TYPE("[1password.com]:80", ADDRESS_TYPE_OTHER);
    ASSERT_ADDRESS_TYPE("1.2.3", ADDRESS_TYPE_OTHER);

#undef ASSERT_ADDRESS_TYPE
}

#define ASSERT_MATCH(pattern, address) \
    assert_true(IPRangeMatch(pattern, address))
#define ASSERT_NO_MATCH(pattern, address) \
    assert_false(IPRangeMatch(pattern, address))

static void test_IPRangeMatch_ipv4()
{
    /* Prefix */
    ASSERT_MATCH("1.2.3", "1.2.3.4");
    ASSERT_NO_MATCH("1.2.3", "1.2.34.5");
    ASSERT_NO_MATCH("1.2.3.4", "1.2.3.45");

    /* Range */
    ASSERT_MATCH("10.0.0.1-50", "10.0.0.1");
    ASSERT_MATCH("10.0.0.1-50", "10.0.0.25");
    ASSERT_MATCH("10.0.0.1-50", "10.0.0.50");
    ASSERT_NO_MATCH("10.0.0.1-50", "10.0.0.0");
    ASSERT_NO_MATCH("10.0.0.1-50", "10.0.0.51");
    ASSERT_NO_MATCH("10.0.0.1-50", "10.0.1.25");
    ASSERT_MATCH("10.1-5", "10.3.7.8");
    ASSERT_NO_MATCH("10.1-5", "10.6.7.8");
    ASSERT_NO_MATCH("10.0.0.1-50", "10.0.0.abc");
    ASSERT_NO_MATCH("10.0.0.50-1", "10.0.0.25");
    ASSERT_NO_MATCH("10.x.0.1-50", "10.0.0.25");

    /* CIDR */
    ASSERT_MATCH("10.0.0.0/8", "10.200.3.4");
    ASSERT_NO_MATCH("10.0.0.0/8", "11.0.0.1");
    ASSERT_MATCH("8.8.8.8/31", "8.8.8.9");
    ASSERT_NO_MATCH("8.8.8.8/31", "8.8.8.10");
    ASSERT_MATCH("1.2.3.4/32", "1.2.3.4");
    ASSERT_MATCH("0.0.0.0/0", "1.2.3.4");
    ASSERT_NO_MATCH("0.0.0.0/0", "not-an-ip-address");
    ASSERT_NO_MATCH("1.2.3.4/33", "1.2.3.4");
    /* The pattern's address used to be read uninitialized when invalid */
    ASSERT_NO_MATCH("bogus.address/0", "1.2.3.4");
    ASSERT_NO_MATCH("1.2.3.999/24", "1.2.3.4");

    /* Mixed notations */
    ASSERT_NO_MATCH("10.0.0.1-5/8", "10.0.0.1");
    ASSERT_NO_MATCH("10.0.0.0/8", "::1");
}

static void test_IPRangeMatch_ipv6()
{
    /* CIDR */
    ASSERT_MATCH("2001:db8::/32", "2001:db8:ffff::1");
    ASSERT_NO_MATCH("2001:db8::/32", "2001:db9::1");
    ASSERT_MATCH("2001:db8::/31", "2001:db9::1");
    ASSERT_MATCH("::/0", "fe80::1");
    ASSERT_NO_MATCH("2001:db8::/129", "2001:db8::1");

    /* Range: both bounds are inclusive */
    ASSERT_MATCH("2001:0-ffff:0:0:0:0:0:1", "2001:0:0:0:0:0:0:1");
    ASSERT_MATCH("2001:0-ffff:0:0:0:0:0:1", "2001:ffff:0:0:0:0:0:1");
    ASSERT_MATCH("2001:10-20:0:0:0:0:0:1", "2001:10:0:0:0:0:0:1");
    ASSERT_MATCH("2001:10-20:0:0:0:0:0:1", "2001:20:0:0:0:0:0:1");
    ASSERT_NO_MATCH("2001:10-20:0:0:0:0:0:1", "2001:21:0:0:0:0:0:1");

    /* Groups are hexadecimal */
    ASSERT_MATCH("2001:a-f:0:0:0:0:0:1", "2001:c:0:0:0:0:0:1");
    ASSERT_NO_MATCH("fe80:1-3:0:0:0:0:0:1", "fe81:2:0:0:0:0:0:1");
    ASSERT_NO_MATCH("2001:1-3:0:0:0:0:0:a", "2001:2:0:0:0:0:0:b");
    ASSERT_MATCH("2001:1-3:0:0:0:0:0:A", "2001:2:0:0:0:0:0:a");

    /* "::" compression in pattern and address */
    ASSERT_MATCH("2001:db8:1-5::1", "2001:db8:3::1");
    ASSERT_MATCH("2001:db8:1-5::1", "2001:db8:3:0:0:0:0:1");
    ASSERT_MATCH("2001:db8:1-5:0:0:0:0:1", "2001:db8:3::1");
    ASSERT_NO_MATCH("2001:db8:1-5::1", "2001:db8:3::2");
    ASSERT_MATCH("::1-5", "::3");
    ASSERT_MATCH("fe80::0-ff:1", "fe80::12:1");

    /* Invalid range patterns never match */
    ASSERT_NO_MATCH("2001:db8:1-5", "2001:db8:3::1");
    ASSERT_NO_MATCH("2001::db8::1-5", "2001::db8:0:0:3");
    ASSERT_NO_MATCH("2001:db8:1-10000::1", "2001:db8:3::1");
}

#undef ASSERT_MATCH
#undef ASSERT_NO_MATCH

static void test_IPRangeMatch()
{
    /* Sanity: exact and subnet matches still work. */
    assert_true(IPRangeMatch("128.39.74.10/23", "128.39.75.56"));
    assert_false(IPRangeMatch("128.39.74.10/24", "128.39.75.56"));
    assert_true(IPRangeMatch("1.2.3.4", "1.2.3.4"));
    assert_false(IPRangeMatch("1.2.3.4", "1.2.3.5"));

    /* An address with fewer octets than the pattern used to walk the scan
     * pointer past the end of the string. Heap-allocate the address so that
     * the over-read is caught under ASan. It must simply not match. */
    char *s = xstrdup("1");
    assert_false(IPRangeMatch("1.2.3.4", s));
    free(s);

    s = xstrdup("1.2");
    assert_false(IPRangeMatch("1.2.3.4", s));
    free(s);

    /* Same for an IPv6 range pattern against a short address. */
    s = xstrdup("2001");
    assert_false(IPRangeMatch("2001:0-ffff:0:0:0:0:0:1", s));
    free(s);
}

static void test_IPRangeIsValid()
{
    assert_true(IPRangeIsValid("1.2.3.4"));
    assert_true(IPRangeIsValid("1-10.2.3.4"));
    assert_true(IPRangeIsValid("1.2.3"));
    assert_true(IPRangeIsValid("192.168.1.50-140"));
    assert_true(IPRangeIsValid("0.0.0.0/0"));
    assert_true(IPRangeIsValid("8.8.8.8/31"));
    assert_true(IPRangeIsValid("1.2.3.4/32"));
    assert_true(IPRangeIsValid("2001:db8::1"));
    assert_true(IPRangeIsValid("2001:db8"));
    assert_true(IPRangeIsValid("2001:db8::/64"));
    assert_true(IPRangeIsValid("::/0"));
    assert_true(IPRangeIsValid("2001:db8:1-5::1"));
    assert_true(IPRangeIsValid("2001:0-ffff:0:0:0:0:0:1"));

    assert_false(IPRangeIsValid(""));
    assert_false(IPRangeIsValid("localhost"));
    assert_false(IPRangeIsValid("www.cfengine.com"));
    assert_false(IPRangeIsValid("1.2.3.4.5"));
    assert_false(IPRangeIsValid("1.2.3."));
    assert_false(IPRangeIsValid("1.2.3.256"));
    assert_false(IPRangeIsValid("10.5-1.0.0"));
    assert_false(IPRangeIsValid("10.0.0.0/33"));
    assert_false(IPRangeIsValid("10.0.0.0/"));
    assert_false(IPRangeIsValid("10.0.0/8"));
    assert_false(IPRangeIsValid("1.2.3.4/8-9"));
    assert_false(IPRangeIsValid("1.2.3.4:5::"));
    assert_false(IPRangeIsValid("2001:db8::/129"));
    assert_false(IPRangeIsValid("2001:db8:1-5"));
    assert_false(IPRangeIsValid("2001::1::1-2"));
    assert_false(IPRangeIsValid("2001:db8:5-1::1"));
    assert_false(IPRangeIsValid("2001:db8:g::1"));

    /* A range with fewer octets used to read past the end of the string. */
    char *s = xstrdup("1-2.3");
    IPRangeIsValid(s);
    free(s);
}

static void test_HostRangeMatch()
{
    assert_true(HostRangeMatch("host", "1-10", "host1"));
    assert_true(HostRangeMatch("host", "1-10", "host10"));
    assert_true(HostRangeMatch("host", "1-10", "HOST5"));
    assert_true(HostRangeMatch("Host", "1-10", "host5"));
    assert_false(HostRangeMatch("host", "1-10", "host0"));
    assert_false(HostRangeMatch("host", "1-10", "host11"));
    assert_false(HostRangeMatch("host", "1-10", "hostx5"));
    assert_false(HostRangeMatch("host", "1-10", "myhost5"));
    assert_false(HostRangeMatch("host", "1-10", "host"));
    assert_false(HostRangeMatch("host", "1-10", "host99999999999999999999999"));

    /* Host names that are empty or all digits used to be scanned backwards
     * past the start of the buffer. */
    char *s = xstrdup("");
    assert_false(HostRangeMatch("host", "1-10", s));
    free(s);

    s = xstrdup("123");
    assert_false(HostRangeMatch("", "1-200", s));
    free(s);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_ParseHostPort),
        unit_test(test_ParseHostPort_address_type),
        unit_test(test_IPRangeMatch),
        unit_test(test_IPRangeMatch_ipv4),
        unit_test(test_IPRangeMatch_ipv6),
        unit_test(test_IPRangeIsValid),
        unit_test(test_HostRangeMatch),
    };

    return run_tests(tests);
}
