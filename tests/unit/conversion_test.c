#include <test.h>

#include <cmockery.h>
#include <conversion.h>

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
        unit_test(test_double_from_string),
        unit_test(test_CommandArg0_bound),
    };

    return run_tests(tests);
}
