#include "cf3.defs.h"

#include <setjmp.h>
#include <cmockery.h>
#include <stdarg.h>


typedef enum
{
    VERCMP_ERROR = -1,
    VERCMP_NO_MATCH = 0,
    VERCMP_MATCH = 1
} VersionCmpResult;

VersionCmpResult ComparePackages(const char *n, const char *v, const char *a, PackageItem * pi, Attributes attr, Promise *pp);

void test_different_name(void **context)
{
    PackageItem pi = {
        .name = "pkgone",
        .version = "1",
        .arch = "arch"
    };
    Attributes attr = {
        .packages = {
            .package_select = cfa_eq
        }
    };

    assert_int_equal(ComparePackages("pkgtwo", "1", "arch", &pi, attr, NULL), false);
}

void test_wildcard_arch(void **context)
{
    PackageItem pi = {
        .name = "foobar",
        .version = "1",
        .arch = "arch"
    };
    Attributes attr = {
        .packages = {
            .package_select = cfa_eq
        }
    };

    assert_int_equal(ComparePackages("foobar", "1", "*", &pi, attr, NULL), true);
}

void test_non_matching_arch(void **context)
{
    PackageItem pi = {
        .name = "foobar",
        .version = "1",
        .arch = "s390x"
    };
    Attributes attr = {
        .packages = {
            .package_select = cfa_eq
        }
    };

    assert_int_equal(ComparePackages("foobar", "1", "s390", &pi, attr, NULL), false);
}

bool DoCompare(const char *lhs, const char *rhs, enum version_cmp cmp)
{
    PackageItem pi = {
        .name = "foobar",
        .version = (char*)lhs,
        .arch = "somearch"
    };
    Attributes a = {
        .packages = {
            .package_select = cmp,
        }
    };

    return ComparePackages("foobar", rhs, "somearch", &pi, a, NULL);
}

void test_wildcard_version(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "*", cfa_eq), true);
}

void test_eq(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", cfa_eq), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", cfa_cmp_none), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", cfa_eq), false);
}

void test_ne(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", cfa_neq), false);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", cfa_neq), true);
}

void test_gt_lt(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", cfa_gt), false);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", cfa_gt), false);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", cfa_gt), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", cfa_lt), false);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", cfa_lt), true);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", cfa_lt), false);
}

void test_gte_lte(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", cfa_ge), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", cfa_ge), false);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", cfa_ge), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", cfa_le), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", cfa_le), true);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", cfa_le), false);
}

void wrong_separators(void **context)
{
    assert_int_equal(DoCompare("1.0", "1,0", cfa_eq), false);
}

void uneven_lengths_1(void **context)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", cfa_eq), false);
}

void uneven_lengths_2(void **context)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", cfa_gt), true);
}

void uneven_lengths_3(void **context)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", cfa_lt), false);
}

void uneven_lengths_4(void **context)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", cfa_ge), true);
}

void uneven_lengths_5(void **context)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", cfa_le), false);
}

void uneven_lengths_6(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", cfa_eq), false);
}

void uneven_lengths_7(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", cfa_gt), false);
}

void uneven_lengths_8(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", cfa_lt), true);
}

void uneven_lengths_9(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", cfa_ge), false);
}

void uneven_lengths_10(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", cfa_le), true);
}

void uneven_lengths_11(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", cfa_eq), false);
}

void uneven_lengths_12(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", cfa_gt), true);
}

void uneven_lengths_13(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", cfa_lt), false);
}

void uneven_lengths_14(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", cfa_ge), true);
}

void uneven_lengths_15(void **context)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", cfa_le), false);
}

void uneven_lengths_16(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", cfa_eq), false);
}

void uneven_lengths_17(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", cfa_gt), false);
}

void uneven_lengths_18(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", cfa_lt), true);
}

void uneven_lengths_19(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", cfa_ge), false);
}

void uneven_lengths_20(void **context)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", cfa_le), true);
}


int main()
{
    VERBOSE = 1;

    const UnitTest tests[] =
        {
            unit_test(test_different_name),
            unit_test(test_wildcard_arch),
            unit_test(test_non_matching_arch),
            unit_test(test_wildcard_version),
            unit_test(test_eq),
            unit_test(test_ne),
            unit_test(test_gt_lt),
            unit_test(test_gte_lte),
            unit_test(wrong_separators),
            unit_test(uneven_lengths_1),
            unit_test(uneven_lengths_2),
            unit_test(uneven_lengths_3),
            unit_test(uneven_lengths_4),
            unit_test(uneven_lengths_5),
            unit_test(uneven_lengths_6),
            unit_test(uneven_lengths_7),
            unit_test(uneven_lengths_8),
            unit_test(uneven_lengths_9),
            unit_test(uneven_lengths_10),
            unit_test(uneven_lengths_11),
            unit_test(uneven_lengths_12),
            unit_test(uneven_lengths_13),
            unit_test(uneven_lengths_14),
            unit_test(uneven_lengths_15),
            unit_test(uneven_lengths_16),
            unit_test(uneven_lengths_17),
            unit_test(uneven_lengths_18),
            unit_test(uneven_lengths_19),
            unit_test(uneven_lengths_20),
        };

    return run_tests(tests);
}
