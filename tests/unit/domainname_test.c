#include "cf3.defs.h"

#include <setjmp.h>
#include <cmockery.h>

char fqname[CF_BUFSIZE];
char uqname[CF_BUFSIZE];
char domain[CF_BUFSIZE];

void CalculateDomainName(const char *nodename, const char *dnsname, char *fqname, char *uqname, char *domain);

static void test_fqname(void **state)
{
    const char nodename[] = "mylaptop.example.com";
    const char dnsname[] = "mylaptop.example.com";

    CalculateDomainName(nodename, dnsname, fqname, uqname, domain);

    assert_string_equal(fqname, "mylaptop.example.com");
    assert_string_equal(uqname, "mylaptop");
    assert_string_equal(domain, "example.com");
}

static void test_uqname(void **state)
{
    CalculateDomainName("mylaptop", "mylaptop.example.com", fqname, uqname, domain);

    assert_string_equal(fqname, "mylaptop.example.com");
    assert_string_equal(uqname, "mylaptop");
    assert_string_equal(domain, "example.com");
}

static void test_uqname2(void **state)
{
    CalculateDomainName("user.laptop", "user.laptop.example.com", fqname, uqname, domain);

    assert_string_equal(fqname, "user.laptop.example.com");
    assert_string_equal(uqname, "user.laptop");
    assert_string_equal(domain, "example.com");
}

static void test_fqname_not_really_fq(void **state)
{
    CalculateDomainName("user.laptop", "user.laptop", fqname, uqname, domain);

    assert_string_equal(fqname, "user.laptop");
    assert_string_equal(uqname, "user");
    assert_string_equal(domain, "laptop");
}

static void test_fqname_not_really_fq2(void **state)
{
    CalculateDomainName("laptop", "laptop", fqname, uqname, domain);

    assert_string_equal(fqname, "laptop");
    assert_string_equal(uqname, "laptop");
    assert_string_equal(domain, "");
}

static void test_fqname_unresolvable(void **state)
{
    CalculateDomainName("laptop", "", fqname, uqname, domain);

    assert_string_equal(fqname, "laptop");
    assert_string_equal(uqname, "laptop");
    assert_string_equal(domain, "");
}

static void test_no_names(void **state)
{
    CalculateDomainName("", "", fqname, uqname, domain);

    assert_string_equal(fqname, "");
    assert_string_equal(uqname, "");
    assert_string_equal(domain, "");
}

static void test_wrong_fqname(void **state)
{
    CalculateDomainName("laptop", "a1006.cfengine.com", fqname, uqname, domain);

    assert_string_equal(fqname, "a1006.cfengine.com");
    assert_string_equal(uqname, "laptop");
    assert_string_equal(domain, "");
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_fqname),
        unit_test(test_uqname),
        unit_test(test_uqname2),
        unit_test(test_fqname_not_really_fq),
        unit_test(test_fqname_not_really_fq2),
        unit_test(test_fqname_unresolvable),
        unit_test(test_no_names),
        unit_test(test_wrong_fqname),
    };

    return run_tests(tests);
}
