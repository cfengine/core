#include <test.h>

#include <cmockery.h>
#include <conversion.h>


static void test_int_from_string(void)
{
    assert_int_equal(IntFromString("0"), 0);
    assert_int_equal(IntFromString("1"), 1);
    assert_int_equal(IntFromString(" 1"), 1);
    assert_int_equal(IntFromString("-1"), (long) -1);
    assert_int_equal(IntFromString("-1k"), (long) -1000);
    assert_int_equal(IntFromString("-123"), (long) -123);
    assert_int_equal(IntFromString("12  "), 12);
    assert_int_equal(IntFromString("12k  "), 12000);
    assert_int_equal(IntFromString("\t1m  "), 1000000);

    /* ==== SPECIAL CASES ==== */
    assert_int_equal(IntFromString("inf"), CF_INFINITY);
    assert_int_equal(IntFromString("now"), CFSTARTTIME);
    assert_int_equal(IntFromString("2k"), 2000);
    assert_int_equal(IntFromString("3K"), 3072);
    assert_int_equal(IntFromString("4m"), 4000000);
    assert_int_equal(IntFromString("1M"), 1024 * 1024);
    /* Percentages are stored as negatives TODO fix. */
    assert_int_equal(IntFromString("10%"), (long) -10);
    /* Unknown quantifiers are just being ignored. */
    assert_int_equal(IntFromString("13o"), 13);

    /* ==== CONTROLLED FAILURES ==== */
    assert_int_equal(IntFromString(NULL), CF_NOINT);
    assert_int_equal(IntFromString(""), CF_NOINT);
    assert_int_equal(IntFromString("  "), CF_NOINT);
    assert_int_equal(IntFromString("$(blah)"), CF_NOINT);
    assert_int_equal(IntFromString("123.45"), CF_NOINT);
    assert_int_equal(IntFromString("120%"), CF_NOINT);
    assert_int_equal(IntFromString("-1%"), CF_NOINT);
    assert_int_equal(IntFromString("14 o"), CF_NOINT);
    assert_int_equal(IntFromString("2ko"), CF_NOINT);
    assert_int_equal(IntFromString("3K o"), CF_NOINT);
    /* The quantifier is not expanded yet. */
    assert_int_equal(IntFromString("99$(blah)"), CF_NOINT);
}

static void test_double_from_string(void)
{
    double val;

    /* ===== TESTING SUCCESS ===== */

    assert_true(DoubleFromString("1.2k", &val));
    assert_double_close(1200.0, val);

    assert_true(DoubleFromString("1m", &val));
    assert_double_close(1000000.0, val);

    assert_true(DoubleFromString("1K", &val));
    assert_double_close(1024.0, val);

    /* Previously reserved as NO_DOUBLE define. */
    assert_true(DoubleFromString("-123.45", &val));
    assert_double_close(-123.45, val);

    assert_true(DoubleFromString("0.1", &val));
    assert_double_close(0.1, val);

    /* leading space is OK. */
    assert_true(DoubleFromString(" 0.2", &val));
    assert_double_close(0.2, val);
    assert_true(DoubleFromString(" 0.2k", &val));
    assert_double_close(200., val);

    assert_true(DoubleFromString("0.1%", &val));
    /* Currently percentages are stored as negatives; TODO FIX! */
    assert_double_close(-0.1, val);

    /* Space quantifier ignored. */
    assert_true(DoubleFromString("1233 ", &val));
    assert_double_close(1233, val);

    assert_true(DoubleFromString("1112    ", &val));
    assert_double_close(1112, val);

    /* Invalid quantifier, ignored for backwards compatibility. */
    assert_true(DoubleFromString("11.1o", &val));
    assert_double_close(11.1, val);

    /* ===== ERROR RETURN ===== */

    /* Verify that parameter is not modified. */
    double old_val = val;

    assert_false(DoubleFromString("", &val));
    assert_false(DoubleFromString(" ", &val));
    assert_false(DoubleFromString("  ", &val));
    assert_false(DoubleFromString("abc", &val));
    assert_false(DoubleFromString("G1", &val));

    /* Anomalous remainders. */
    assert_false(DoubleFromString("123adf", &val));
    assert_false(DoubleFromString("123 adf", &val));
    assert_false(DoubleFromString("123   adf", &val));
    assert_false(DoubleFromString("123   adf", &val));

    assert_false(DoubleFromString("123$(remainder)", &val));


    assert_true(val == old_val);
}

static void test_CommandArg0_bound(void)
{
    char dst[128];
    size_t zret;

    zret = CommandArg0_bound(dst, "", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, " ", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, " blah", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, "blah", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "blah blue", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "\"\"", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, "\"blah\"", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "\"blah", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "\"blah blue", sizeof(dst));
    assert_string_equal(dst, "blah blue");
    assert_int_equal(zret, 9);

    zret = CommandArg0_bound(dst, "\"\" blus", sizeof(dst));
    assert_string_equal(dst, "");
    assert_int_equal(zret, 0);
    zret = CommandArg0_bound(dst, "\"blah\" blue", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);
    zret = CommandArg0_bound(dst, "\"blah\"blue", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "blah ", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "\" \"", sizeof(dst));
    assert_string_equal(dst, " ");
    assert_int_equal(zret, 1);
    zret = CommandArg0_bound(dst, "\" \" ", sizeof(dst));
    assert_string_equal(dst, " ");
    assert_int_equal(zret, 1);

    zret = CommandArg0_bound(dst, "blah \"blue\"", sizeof(dst));
    assert_string_equal(dst, "blah");
    assert_int_equal(zret, 4);

    zret = CommandArg0_bound(dst, "blah\"blue\"", sizeof(dst));
    assert_string_equal(dst, "blah\"blue\"");
    assert_int_equal(zret, 10);

    zret = CommandArg0_bound(dst, "blah\"blue", sizeof(dst));
    assert_string_equal(dst, "blah\"blue");
    assert_int_equal(zret, 9);

    /* TEST OVERFLOW */

    zret = CommandArg0_bound(dst, "", 0);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "blah", 0);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, " ", 0);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "\"blah\"", 0);
    assert_int_equal(zret, (size_t) -1);

    zret = CommandArg0_bound(dst, "blah", 1);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "\"blah\"", 1);
    assert_int_equal(zret, (size_t) -1);

    zret = CommandArg0_bound(dst, "b", 1);
    assert_int_equal(zret, (size_t) -1);
    zret = CommandArg0_bound(dst, "\"b\"", 1);
    assert_int_equal(zret, (size_t) -1);

    zret = CommandArg0_bound(dst, "", 1);
    assert_int_equal(zret, 0);                         /* empty string fits */
    zret = CommandArg0_bound(dst, " ", 1);
    assert_int_equal(zret, 0);                         /* empty string fits */
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_int_from_string),
        unit_test(test_double_from_string),
        unit_test(test_CommandArg0_bound),
    };

    return run_tests(tests);
}
