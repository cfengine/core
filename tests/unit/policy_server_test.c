#include <test.h>

#include <alloc.h>
#include <policy_server.h>

// PolicyServerGet functions should always return NULL instead of empty:
bool test_empty(const char *a)
{
    if(a == NULL)
    {
        return false;
    }
    if(a[0] == '\0')
    {
        return true;
    }
    return false;
}

// assert_int_equal is not ideal for this(prints "1 != 0"), but works
#define test_never_empty(a, b, c, d)\
{\
    assert_int_equal(test_empty(a), false);\
    assert_int_equal(test_empty(b), false);\
    assert_int_equal(test_empty(c), false);\
    assert_int_equal(test_empty(d), false);\
}\

// General test of all variables (get functions)
#define test_one_case_generic(set, get, host, port, ip)\
{\
    const char *a, *b, *c, *d;\
    PolicyServerSet(set);\
    assert_string_int_equal((a = PolicyServerGet()),     get);\
    assert_string_int_equal((b = PolicyServerGetHost()), host);\
    assert_string_int_equal((c = PolicyServerGetPort()), port);\
    assert_string_int_equal((d = PolicyServerGetIP()),   ip);\
    test_never_empty(a, b, c, d);\
}\

// For testing hostnames, doesn't do any hostname->ip resolution:
#define test_no_ip_generic(set, get, host, port)\
{\
    const char *a, *b, *c;\
    PolicyServerSet(set);\
    assert_string_int_equal((a = PolicyServerGet()),     get);\
    assert_string_int_equal((b = PolicyServerGetHost()), host);\
    assert_string_int_equal((c = PolicyServerGetPort()), port);\
    test_never_empty(a, b, c, NULL);\
}\

// Abbreviation of test_one_case_generic
#define test_one_case(bootstrap, host, port, ip)\
{\
    test_one_case_generic(bootstrap, bootstrap, host, port, ip);\
}\

// For inputs where we expect everything to return NULL
#define test_null_server(input)\
{\
    test_one_case_generic(input, NULL, NULL, NULL, NULL);\
}\

// For testing hostnames, doesn't do any hostname->ip resolution:
#define test_no_ip(bootstrap, host, port)\
{\
    test_no_ip_generic(bootstrap, bootstrap, host, port);\
}\


static void test_PolicyServer_IPv4()
{
    // IPv4:
    test_one_case( /* Set & Get */ "1.2.3.4",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "1.2.3.4");

    test_one_case( /* Set & Get */ "255.255.255.255:80",
                   /* Host,port */ NULL, "80",
                   /* IP Addr   */ "255.255.255.255");

    test_one_case( /* Set & Get */ "   1.2.3.4:",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "1.2.3.4");
}

static void test_PolicyServer_IPv6()
{
    // IPv6 with square brackets:
    test_one_case( /* Set & Get */ "[ffff::dd:12:34]",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "ffff::dd:12:34");

    test_one_case( /* Set & Get */ "[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:64000",
                   /* Host,port */ NULL, "64000",
                   /* IP Addr   */ "2001:0db8:85a3:0000:0000:8a2e:0370:7334");

    test_one_case( /* Set & Get */ "[ffff::dd:12:34]:",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "ffff::dd:12:34");

    test_one_case( /* Set & Get */ "[FF01:0:0:0:0:0:0:FB]:12345",
                   /* Host,port */ NULL, "12345",
                   /* IP Addr   */ "FF01:0:0:0:0:0:0:FB");

    // IPv6 without square brackets:
    test_one_case( /* Set & Get */ "ffff::dd:12:34",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "ffff::dd:12:34");

    test_one_case( /* Set & Get */ "FF01:0:0:0:0:0:0:FB",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "FF01:0:0:0:0:0:0:FB");

    test_one_case( /* Set & Get */ "::",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "::");

    // IPv4 mapped IPv6 addresses:
    test_one_case( /* Set & Get */ "::ffff:192.0.2.128",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "::ffff:192.0.2.128");

    test_one_case( /* Set & Get */ "[::ffff:192.0.2.128]",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "::ffff:192.0.2.128");

    // Other:
    test_one_case( /* Set & Get */ "ff02::1:ff00:0/104",
                   /* Host,port */ NULL, NULL,
                   /* IP Addr   */ "ff02::1:ff00:0/104");


    test_one_case( /* Set & Get */ "[ff02::1:ff00:0/104]:9",
                   /* Host,port */ NULL, "9",
                   /* IP Addr   */ "ff02::1:ff00:0/104");
}

static void test_PolicyServer_Host()
{
    // Hostnames:
    test_no_ip(    /* Set & Get */ "localhost",
                   /* Host,port */ "localhost", NULL);

    test_no_ip(    /* Set & Get */ "localhost:",
                   /* Host,port */ "localhost", NULL);

    test_no_ip(    /* Set & Get */ "localhost:80",
                   /* Host,port */ "localhost", "80");

    test_no_ip(    /* Set & Get */ "[localhost]",
                   /* Host,port */ "localhost", NULL);

    test_no_ip(    /* Set & Get */ "[localhost]:",
                   /* Host,port */ "localhost", NULL);

    test_no_ip(    /* Set & Get */ "[localhost]:59999",
                   /* Host,port */ "localhost", "59999");

    test_no_ip(    /* Set & Get */ "[www.cfengine.com]",
                   /* Host,port */ "www.cfengine.com", NULL);

    test_no_ip(    /* Set & Get */ "[www.cfengine.com]:8080",
                   /* Host,port */ "www.cfengine.com", "8080");

    test_no_ip(    /* Set & Get */ "[www.cfengine.com]:",
                   /* Host,port */ "www.cfengine.com", NULL);
}

static void test_PolicyServer_Corner()
{
    // These tests expect PolicyServerGet() to return NULL:
    test_null_server("");
    test_null_server(NULL);
    test_null_server("   ");
    test_null_server(" \n");
    test_null_server("\n");
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_PolicyServer_IPv4),
        unit_test(test_PolicyServer_IPv6),
        unit_test(test_PolicyServer_Host),
        unit_test(test_PolicyServer_Corner)
    };

    return run_tests(tests);
}
