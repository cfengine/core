#include <test.h>

#include <alloc.h>
#include <addr_lib.h>


/**
 *  @WARNING this test leaks all over the place. I'm strdup'ing because
 *           ParseHostPort() might modify the string, and a string literal
 *           will cause segfault since it is in read-only memory segment.
 */
static void test_ParseHostPort()
{
    char *hostname, *port;

    ParseHostPort(xstrdup("www.cfengine.com"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("www.cfengine.com:80"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("www.cfengine.com:"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[www.cfengine.com]"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[www.cfengine.com]:80"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[www.cfengine.com]:"), &hostname, &port);
    assert_string_equal(hostname, "www.cfengine.com");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("1.2.3.4"), &hostname, &port);
    assert_string_equal(hostname, "1.2.3.4");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("1.2.3.4:80"), &hostname, &port);
    assert_string_equal(hostname, "1.2.3.4");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("1.2.3.4:"), &hostname, &port);
    assert_string_equal(hostname, "1.2.3.4");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[ffff::dd:12:34]"), &hostname, &port);
    assert_string_equal(hostname, "ffff::dd:12:34");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[ffff::dd:12:34]:80"), &hostname, &port);
    assert_string_equal(hostname, "ffff::dd:12:34");
    assert_string_equal(port, "80");
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[ffff::dd:12:34]:"), &hostname, &port);
    assert_string_equal(hostname, "ffff::dd:12:34");
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    /***** CORNER CASES *****/

    ParseHostPort(xstrdup(""), &hostname, &port);
    assert_int_equal(hostname, NULL);
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[]"), &hostname, &port);
    assert_int_equal(hostname, NULL);
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup("[]:"), &hostname, &port);
    assert_int_equal(hostname, NULL);
    assert_int_equal(port, NULL);
    hostname = NULL; port = NULL;

    ParseHostPort(xstrdup(":"), &hostname, &port);
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
