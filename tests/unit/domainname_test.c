#include <test.h>

#include <cf3.defs.h>

char fqname[CF_BUFSIZE];
char uqname[CF_BUFSIZE];
char domain[CF_BUFSIZE];

void CalculateDomainName(const char *nodename, const char *dnsname,
                         char *fqname, size_t fqname_size,
                         char *uqname, size_t uqname_size,
                         char *domain, size_t domain_size);

static void test_fqname(void)
{
    const char nodename[] = "mylaptop.example.com";
    const char dnsname[] = "mylaptop.example.com";

    CalculateDomainName(nodename, dnsname,
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "mylaptop.example.com");
    assert_string_equal(uqname, "mylaptop");
    assert_string_equal(domain, "example.com");
}

static void test_uqname(void)
{
    CalculateDomainName("mylaptop", "mylaptop.example.com",
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "mylaptop.example.com");
    assert_string_equal(uqname, "mylaptop");
    assert_string_equal(domain, "example.com");
}

static void test_uqname2(void)
{
    CalculateDomainName("user.laptop", "user.laptop.example.com",
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "user.laptop.example.com");
    assert_string_equal(uqname, "user.laptop");
    assert_string_equal(domain, "example.com");
}

static void test_fqname_not_really_fq(void)
{
    CalculateDomainName("user.laptop", "user.laptop",
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "user.laptop");
    assert_string_equal(uqname, "user");
    assert_string_equal(domain, "laptop");
}

static void test_fqname_not_really_fq2(void)
{
    CalculateDomainName("laptop", "laptop",
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "laptop");
    assert_string_equal(uqname, "laptop");
    assert_string_equal(domain, "");
}

static void test_fqname_unresolvable(void)
{
    CalculateDomainName("laptop", "",
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "laptop");
    assert_string_equal(uqname, "laptop");
    assert_string_equal(domain, "");
}

static void test_no_names(void)
{
    CalculateDomainName("", "",
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "");
    assert_string_equal(uqname, "");
    assert_string_equal(domain, "");
}

static void test_wrong_fqname(void)
{
    CalculateDomainName("laptop", "a1006.cfengine.com",
                        fqname, sizeof(fqname), uqname, sizeof(uqname), domain, sizeof(domain));

    assert_string_equal(fqname, "a1006.cfengine.com");
    assert_string_equal(uqname, "laptop");
    assert_string_equal(domain, "");
}

int main()
{
    PRINT_TEST_BANNER();
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
