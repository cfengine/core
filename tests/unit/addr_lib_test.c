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


int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_ParseHostPort)
    };

    return run_tests(tests);
}
