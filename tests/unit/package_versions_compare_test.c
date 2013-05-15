#include "cf3.defs.h"

#include "env_context.h"

#include "test.h"


typedef enum
{
    VERCMP_ERROR = -1,
    VERCMP_NO_MATCH = 0,
    VERCMP_MATCH = 1
} VersionCmpResult;

VersionCmpResult ComparePackages(EvalContext *ctx, const char *n, const char *v, const char *a, PackageItem * pi, Attributes attr, Promise *pp);

void test_different_name(void)
{
    EvalContext *ctx = EvalContextNew();

    PackageItem pi = {
        .name = "pkgone",
        .version = "1",
        .arch = "arch"
    };
    Attributes attr = {
        .packages = {
            .package_select = PACKAGE_VERSION_COMPARATOR_EQ
        }
    };

    assert_int_equal(ComparePackages(ctx, "pkgtwo", "1", "arch", &pi, attr, NULL), false);

    EvalContextDestroy(ctx);
}

void test_wildcard_arch(void)
{
    EvalContext *ctx = EvalContextNew();

    PackageItem pi = {
        .name = "foobar",
        .version = "1",
        .arch = "arch"
    };
    Attributes attr = {
        .packages = {
            .package_select = PACKAGE_VERSION_COMPARATOR_EQ
        }
    };

    assert_int_equal(ComparePackages(ctx, "foobar", "1", "*", &pi, attr, NULL), true);

    EvalContextDestroy(ctx);
}

void test_non_matching_arch(void)
{
    EvalContext *ctx = EvalContextNew();

    PackageItem pi = {
        .name = "foobar",
        .version = "1",
        .arch = "s390x"
    };
    Attributes attr = {
        .packages = {
            .package_select = PACKAGE_VERSION_COMPARATOR_EQ
        }
    };

    assert_int_equal(ComparePackages(ctx, "foobar", "1", "s390", &pi, attr, NULL), false);

    EvalContextDestroy(ctx);
}

bool DoCompare(const char *lhs, const char *rhs, PackageVersionComparator cmp)
{
    EvalContext *ctx = EvalContextNew();

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

    return ComparePackages(ctx, "foobar", rhs, "somearch", &pi, a, NULL);

    EvalContextDestroy(ctx);
}

void test_wildcard_version(void)
{
    assert_int_equal(DoCompare("1.0-1", "*", PACKAGE_VERSION_COMPARATOR_EQ), true);
}

void test_eq(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_EQ), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_NONE), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_EQ), false);
}

void test_ne(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_NEQ), false);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_NEQ), true);
}

void test_gt_lt(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_GT), false);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_GT), false);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_GT), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_LT), false);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_LT), true);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_LT), false);
}

void test_gte_lte(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_GE), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_GE), false);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_GE), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_LE), true);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_LE), true);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_LE), false);
}

void wrong_separators(void)
{
    assert_int_equal(DoCompare("1.0", "1,0", PACKAGE_VERSION_COMPARATOR_EQ), false);
}

void uneven_lengths_1(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_EQ), false);
}

void uneven_lengths_2(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_GT), true);
}

void uneven_lengths_3(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_LT), false);
}

void uneven_lengths_4(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_GE), true);
}

void uneven_lengths_5(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_LE), false);
}

void uneven_lengths_6(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_EQ), false);
}

void uneven_lengths_7(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_GT), false);
}

void uneven_lengths_8(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_LT), true);
}

void uneven_lengths_9(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_GE), false);
}

void uneven_lengths_10(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_LE), true);
}

void uneven_lengths_11(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_EQ), false);
}

void uneven_lengths_12(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_GT), true);
}

void uneven_lengths_13(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_LT), false);
}

void uneven_lengths_14(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_GE), true);
}

void uneven_lengths_15(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_LE), false);
}

void uneven_lengths_16(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_EQ), false);
}

void uneven_lengths_17(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_GT), false);
}

void uneven_lengths_18(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_LT), true);
}

void uneven_lengths_19(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_GE), false);
}

void uneven_lengths_20(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_LE), true);
}


int main()
{
    PRINT_TEST_BANNER();
    LogSetGlobalLevel(LOG_LEVEL_VERBOSE);

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
