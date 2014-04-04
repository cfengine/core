#include <test.h>

#include <misc_lib.h>

static void test_unsigned_modulus(void)
{
    assert_int_equal(UnsignedModulus(0, 3), 0);
    assert_int_equal(UnsignedModulus(1, 3), 1);
    assert_int_equal(UnsignedModulus(2, 3), 2);
    assert_int_equal(UnsignedModulus(3, 3), 0);
    assert_int_equal(UnsignedModulus(4, 3), 1);

    assert_int_equal(UnsignedModulus(-1, 3), 2);
    assert_int_equal(UnsignedModulus(-2, 3), 1);
    assert_int_equal(UnsignedModulus(-3, 3), 0);
    assert_int_equal(UnsignedModulus(-4, 3), 2);
}

static void test_upper_power_of_two(void)
{
    assert_int_equal(0, UpperPowerOfTwo(0));
    assert_int_equal(1, UpperPowerOfTwo(1));
    assert_int_equal(2, UpperPowerOfTwo(2));
    assert_int_equal(4, UpperPowerOfTwo(3));
    assert_int_equal(16, UpperPowerOfTwo(13));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_unsigned_modulus),
        unit_test(test_upper_power_of_two),
    };

    return run_tests(tests);
}

// STUBS
