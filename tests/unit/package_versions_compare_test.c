#include <test.h>

#include <cf3.defs.h>
#include <eval_context.h>
#include <verify_packages.h>

void test_different_name(void)
{
    EvalContext *ctx = EvalContextNew();
    PromiseResult result;

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

    assert_int_equal(ComparePackages(ctx, "pkgtwo", "1", "arch", &pi, attr, NULL, "test", &result), VERCMP_NO_MATCH);

    EvalContextDestroy(ctx);
}

void test_wildcard_arch(void)
{
    EvalContext *ctx = EvalContextNew();
    PromiseResult result;

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

    assert_int_equal(ComparePackages(ctx, "foobar", "1", "*", &pi, attr, NULL, "test", &result), VERCMP_MATCH);

    EvalContextDestroy(ctx);
}

void test_non_matching_arch(void)
{
    EvalContext *ctx = EvalContextNew();
    PromiseResult result;

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

    assert_int_equal(ComparePackages(ctx, "foobar", "1", "s390", &pi, attr, NULL, "test", &result), VERCMP_NO_MATCH);

    EvalContextDestroy(ctx);
}

VersionCmpResult DoCompare(const char *lhs, const char *rhs, PackageVersionComparator cmp)
{
    EvalContext *ctx = EvalContextNew();
    PromiseResult result;

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

    VersionCmpResult cmp_result = ComparePackages(ctx, "foobar", rhs, "somearch", &pi, a, NULL, "test", &result);

    EvalContextDestroy(ctx);

    return cmp_result;
}

void test_wildcard_version(void)
{
    assert_int_equal(DoCompare("1.0-1", "*", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_MATCH);
}

void test_eq(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_NONE), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_NO_MATCH);
}

void test_ne(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_NEQ), VERCMP_NO_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_NEQ), VERCMP_MATCH);
}

void test_gt_lt(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_NO_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_NO_MATCH);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_NO_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_NO_MATCH);
}

void test_gte_lte(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_NO_MATCH);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-1", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-1", "1.0-2", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_MATCH);
    assert_int_equal(DoCompare("1.0-2", "1.0-1", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_NO_MATCH);
}

void wrong_separators(void)
{
    assert_int_equal(DoCompare("1.0", "1,0", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_ERROR);
}

void uneven_lengths_1(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_NO_MATCH);
}

void uneven_lengths_2(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_MATCH);
}

void uneven_lengths_3(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_NO_MATCH);
}

void uneven_lengths_4(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_MATCH);
}

void uneven_lengths_5(void)
{
    assert_int_equal(DoCompare("1.0.1", "1.0", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_NO_MATCH);
}

void uneven_lengths_6(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_NO_MATCH);
}

void uneven_lengths_7(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_NO_MATCH);
}

void uneven_lengths_8(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_MATCH);
}

void uneven_lengths_9(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_NO_MATCH);
}

void uneven_lengths_10(void)
{
    assert_int_equal(DoCompare("1.0", "1.0.1", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_MATCH);
}

void uneven_lengths_11(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_NO_MATCH);
}

void uneven_lengths_12(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_MATCH);
}

void uneven_lengths_13(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_NO_MATCH);
}

void uneven_lengths_14(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_MATCH);
}

void uneven_lengths_15(void)
{
    assert_int_equal(DoCompare("1.0-1", "1.0", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_NO_MATCH);
}

void uneven_lengths_16(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_EQ), VERCMP_NO_MATCH);
}

void uneven_lengths_17(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_NO_MATCH);
}

void uneven_lengths_18(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_MATCH);
}

void uneven_lengths_19(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_NO_MATCH);
}

void uneven_lengths_20(void)
{
    assert_int_equal(DoCompare("1.0", "1.0-1", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_MATCH);
}

void invalid_01(void)
{
    assert_int_equal(DoCompare("text-1.0", "1.0", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_ERROR);
}

void invalid_02(void)
{
    assert_int_equal(DoCompare("text-1.0", "1.0", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_ERROR);
}

void invalid_03(void)
{
    assert_int_equal(DoCompare("1.0", "text-1.0", PACKAGE_VERSION_COMPARATOR_LE), VERCMP_ERROR);
}

void invalid_04(void)
{
    assert_int_equal(DoCompare("1.0", "text-1.0", PACKAGE_VERSION_COMPARATOR_GE), VERCMP_ERROR);
}

void invalid_05(void)
{
    assert_int_equal(DoCompare("text-1.0", "1.0", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_ERROR);
}

void invalid_06(void)
{
    assert_int_equal(DoCompare("text-1.0", "1.0", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_ERROR);
}

void invalid_07(void)
{
    assert_int_equal(DoCompare("1.0", "text-1.0", PACKAGE_VERSION_COMPARATOR_LT), VERCMP_ERROR);
}

void invalid_08(void)
{
    assert_int_equal(DoCompare("1.0", "text-1.0", PACKAGE_VERSION_COMPARATOR_GT), VERCMP_ERROR);
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
            unit_test(invalid_01),
            unit_test(invalid_02),
            unit_test(invalid_03),
            unit_test(invalid_04),
            unit_test(invalid_05),
            unit_test(invalid_06),
            unit_test(invalid_07),
            unit_test(invalid_08),
        };

    return run_tests(tests);
}
