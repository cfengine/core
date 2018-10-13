#include <test.h>

#include <misc_lib.h>
#include <alloc.h>


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

/*
   Generated in python3:
   >>> for i in range(64): print("{:26} ||     \\".format("(x == 0x{:x})".format(2**i)))
 */
#define ISPOW2_FORSURE(x)                  \
       ((x > 0)    &&                      \
        (                                  \
         (x == 0x1)                 ||     \
         (x == 0x2)                 ||     \
         (x == 0x4)                 ||     \
         (x == 0x8)                 ||     \
         (x == 0x10)                ||     \
         (x == 0x20)                ||     \
         (x == 0x40)                ||     \
         (x == 0x80)                ||     \
         (x == 0x100)               ||     \
         (x == 0x200)               ||     \
         (x == 0x400)               ||     \
         (x == 0x800)               ||     \
         (x == 0x1000)              ||     \
         (x == 0x2000)              ||     \
         (x == 0x4000)              ||     \
         (x == 0x8000)              ||     \
         (x == 0x10000)             ||     \
         (x == 0x20000)             ||     \
         (x == 0x40000)             ||     \
         (x == 0x80000)             ||     \
         (x == 0x100000)            ||     \
         (x == 0x200000)            ||     \
         (x == 0x400000)            ||     \
         (x == 0x800000)            ||     \
         (x == 0x1000000)           ||     \
         (x == 0x2000000)           ||     \
         (x == 0x4000000)           ||     \
         (x == 0x8000000)           ||     \
         (x == 0x10000000)          ||     \
         (x == 0x20000000)          ||     \
         (x == 0x40000000)          ||     \
         (x == 0x80000000)          ||     \
         (x == 0x100000000)         ||     \
         (x == 0x200000000)         ||     \
         (x == 0x400000000)         ||     \
         (x == 0x800000000)         ||     \
         (x == 0x1000000000)        ||     \
         (x == 0x2000000000)        ||     \
         (x == 0x4000000000)        ||     \
         (x == 0x8000000000)        ||     \
         (x == 0x10000000000)       ||     \
         (x == 0x20000000000)       ||     \
         (x == 0x40000000000)       ||     \
         (x == 0x80000000000)       ||     \
         (x == 0x100000000000)      ||     \
         (x == 0x200000000000)      ||     \
         (x == 0x400000000000)      ||     \
         (x == 0x800000000000)      ||     \
         (x == 0x1000000000000)     ||     \
         (x == 0x2000000000000)     ||     \
         (x == 0x4000000000000)     ||     \
         (x == 0x8000000000000)     ||     \
         (x == 0x10000000000000)    ||     \
         (x == 0x20000000000000)    ||     \
         (x == 0x40000000000000)    ||     \
         (x == 0x80000000000000)    ||     \
         (x == 0x100000000000000)   ||     \
         (x == 0x200000000000000)   ||     \
         (x == 0x400000000000000)   ||     \
         (x == 0x800000000000000)   ||     \
         (x == 0x1000000000000000)  ||     \
         (x == 0x2000000000000000)  ||     \
         (x == 0x4000000000000000)  ||     \
         (x == 0x8000000000000000)         \
           ))

long count_pow2;

#define TYPED_TEST_ISPOW2(orig, T)                                      \
    do {                                                                \
        typeof(T) t = (typeof(T)) orig;                                 \
                                                                        \
        bool ispow =         ISPOW2(t);                                 \
        bool ispow_forsure = ISPOW2_FORSURE(t);                         \
                                                                        \
        if (ispow_forsure && !ispow)                                    \
        {                                                               \
            printf("ISPOW2 FAIL for %jd (original value %jd), should be true but is false!\n", \
                   (intmax_t) t, (intmax_t) orig);                      \
            assert_true(false);                                         \
        }                                                               \
        else if (!ispow_forsure && ispow)                               \
        {                                                               \
            printf("ISPOW2 FAIL for %jd (original value %jd), should be false but is true!\n", \
                   (intmax_t) t, (intmax_t) orig);                      \
            assert_true(false);                                         \
        }                                                               \
        if (ispow)                                                      \
        {                                                               \
            count_pow2++;                                               \
        }                                                               \
    } while(false);

#if 0
        else                                                            \
        {                                                               \
            printf("PASS: ISPOW2(%jd) == %s\n",                         \
                   (intmax_t) t,                                        \
                   ispow ? "true" : "false");                           \
        }
#endif

#define TEST_COUNT 100000

static void test_rand_ISPOW2(void)
{
    for (int i = 0; i < TEST_COUNT; i++)
    {
        /* drand48() doesn't give you 64 bits of randomness. */
        /* Get two numbers between [0,1) */
        double d1 = drand48();
        double d2 = drand48();
        /* Spread them over the maximum range for 32 bits. */
        uint64_t u1 = d1 * UINT32_MAX;
        uint64_t u2 = d2 * UINT32_MAX;
        /* Compose them to one fully random 64 bit integer. */
        uint64_t x = u1 | (u2 << 32);

        // printf("====> testing %20ju, signed %20jd, hex 0x%016jx\n",
        //        (uintmax_t) x, (uintmax_t) x, (uintmax_t) x);

        TYPED_TEST_ISPOW2(x, uint8_t);
        TYPED_TEST_ISPOW2(x, uint16_t);
        TYPED_TEST_ISPOW2(x, uint32_t);
        TYPED_TEST_ISPOW2(x, uint64_t);
        TYPED_TEST_ISPOW2(x, int8_t);
        TYPED_TEST_ISPOW2(x, int16_t);
        TYPED_TEST_ISPOW2(x, int32_t);
        TYPED_TEST_ISPOW2(x, int64_t);
        TYPED_TEST_ISPOW2(x, intmax_t);
        TYPED_TEST_ISPOW2(x, uintmax_t);
        TYPED_TEST_ISPOW2(x, char);
        TYPED_TEST_ISPOW2(x, unsigned char);
        TYPED_TEST_ISPOW2(x, short int);
        TYPED_TEST_ISPOW2(x, unsigned short int);
        TYPED_TEST_ISPOW2(x, int);
        TYPED_TEST_ISPOW2(x, unsigned int);
        TYPED_TEST_ISPOW2(x, long int);
        TYPED_TEST_ISPOW2(x, unsigned long int);
        TYPED_TEST_ISPOW2(x, long long int);
        TYPED_TEST_ISPOW2(x, unsigned long long int);
    }

    printf("    Tested ISPOW2() with %8d numbers in various type combinations"
           " out of which %8ld where power of 2.\n",
           TEST_COUNT, count_pow2);
}

static void test_ISPOW2(void)
{
    assert_true(ISPOW2(1));
    assert_true(ISPOW2(0x80000000));

    assert_false(ISPOW2(0));
    assert_false(ISPOW2(0xFFFFFFFF));
}

static void test_putenv_wrapper(void)
{
    assert_true(getenv("UNIT_TEST_VAR") == NULL);

    putenv_wrapper("UNIT_TEST_VAR=VALUE");
    assert_true(getenv("UNIT_TEST_VAR") != NULL);
    assert_string_equal(getenv("UNIT_TEST_VAR"), "VALUE");

    putenv_wrapper("UNIT_TEST_VAR=NEW_VALUE");
    assert_true(getenv("UNIT_TEST_VAR") != NULL);
    assert_string_equal(getenv("UNIT_TEST_VAR"), "NEW_VALUE");

    unsetenv("UNIT_TEST_VAR");
    assert_true(getenv("UNIT_TEST_VAR") == NULL);
}

static void test_setenv_wrapper(void)
{
    assert_true(getenv("UNIT_TEST_VAR") == NULL);

    setenv_wrapper("UNIT_TEST_VAR", "VALUE", 0);
    assert_true(getenv("UNIT_TEST_VAR") != NULL);
    assert_string_equal(getenv("UNIT_TEST_VAR"), "VALUE");

    setenv_wrapper("UNIT_TEST_VAR", "NEW_VALUE", 1);
    assert_true(getenv("UNIT_TEST_VAR") != NULL);
    assert_string_equal(getenv("UNIT_TEST_VAR"), "NEW_VALUE");

    setenv_wrapper("UNIT_TEST_VAR", "NO_OVERWRITE", 0);
    assert_true(getenv("UNIT_TEST_VAR") != NULL);
    assert_string_equal(getenv("UNIT_TEST_VAR"), "NEW_VALUE");

    unsetenv("UNIT_TEST_VAR");
    assert_true(getenv("UNIT_TEST_VAR") == NULL);
}

int main()
{
    srand48(time(NULL));

    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_unsigned_modulus),
        unit_test(test_upper_power_of_two),
        unit_test(test_rand_ISPOW2),
        unit_test(test_ISPOW2),
        unit_test(test_putenv_wrapper),
        unit_test(test_setenv_wrapper),
    };

    return run_tests(tests);
}
