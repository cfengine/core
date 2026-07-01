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

static void test_FuzzySetMatch()
{
    /* Sanity: exact and subnet matches still work. */
    assert_int_equal(FuzzySetMatch("128.39.74.10/23", "128.39.75.56"), 0);
    assert_int_not_equal(FuzzySetMatch("128.39.74.10/24", "128.39.75.56"), 0);
    assert_int_equal(FuzzySetMatch("1.2.3.4", "1.2.3.4"), 0);
    assert_int_not_equal(FuzzySetMatch("1.2.3.4", "1.2.3.5"), 0);

    /* An address with fewer octets than the pattern used to walk the scan
     * pointer past the end of the string. Heap-allocate the address so that
     * the over-read is caught under ASan. It must simply not match. */
    char *s = xstrdup("1");
    assert_int_not_equal(FuzzySetMatch("1.2.3.4", s), 0);
    free(s);

    s = xstrdup("1.2");
    assert_int_not_equal(FuzzySetMatch("1.2.3.4", s), 0);
    free(s);

    /* Same for an IPv6 range pattern against a short address. */
    s = xstrdup("2001");
    assert_int_not_equal(FuzzySetMatch("2001:0-ffff:0:0:0:0:0:1", s), 0);
    free(s);
}

static void test_FuzzyMatchParse()
{
    assert_true(FuzzyMatchParse("1.2.3.4"));
    assert_true(FuzzyMatchParse("1-10.2.3.4"));

    /* A range with fewer octets used to read past the end of the string. */
    char *s = xstrdup("1-2.3");
    FuzzyMatchParse(s);
    free(s);
}


int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_ParseHostPort),
        unit_test(test_FuzzySetMatch),
        unit_test(test_FuzzyMatchParse)
    };

    return run_tests(tests);
}
