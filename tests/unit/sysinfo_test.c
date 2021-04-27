#include <test.h>

#include <sysinfo.h>
#include <sysinfo.c>

static void test_uptime(void)
{
    /*
     * Assume we have been online at least one minute, and less than 5 years.
     * Should be good enough for everyone...
     */
    int uptime = GetUptimeMinutes(time(NULL));
    printf("Uptime: %.2f days\n", uptime / (60.0 * 24));
    assert_in_range(uptime, 1, 60*24*365*5);
}

static void FindNextIntegerTestWrapper(char *str, char expected[][KIBIBYTE(1)], int num_expected)
{
    Seq *seq = SeqNew(num_expected, NULL);
    char *integer;

    char *next = FindNextInteger(str, &integer);
    if (integer != NULL)
    {
        SeqAppend(seq, integer);
    }
    while (next != NULL && integer != NULL)
    {
        next = FindNextInteger(next, &integer);
        if (integer != NULL)
        {
            SeqAppend(seq, integer);
        }
    }

    assert_int_equal(num_expected, SeqLength(seq));
    for (int i = 0; i < num_expected; i++)
    {
        assert_string_equal((char *) SeqAt(seq, i), expected[i]);
    }

    SeqDestroy(seq);
}

static void test_find_next_integer(void)
{
    {
        char str[] = "Ubuntu 20.04.1 LTS";
        char expected[3][KIBIBYTE(1)] = { "20", "04", "1" };
        FindNextIntegerTestWrapper(str, expected, 3);
    }
    {
        char str[] = "canonified_5";
        char expected[1][KIBIBYTE(1)] = { "5" };
        FindNextIntegerTestWrapper(str, expected, 1);
    }
    {
        char str[] = "";
        char expected[0][KIBIBYTE(1)] = { };
        FindNextIntegerTestWrapper(str, expected, 0);
    }
    {
        char str[] = " ";
        char expected[0][KIBIBYTE(1)] = { };
        FindNextIntegerTestWrapper(str, expected, 0);
    }
    {
        char str[] = " no numbers in sight ";
        char expected[0][KIBIBYTE(1)] = { };
        FindNextIntegerTestWrapper(str, expected, 0);
    }
    {
        char str[] = "0";
        char expected[1][KIBIBYTE(1)] = { "0" };
        FindNextIntegerTestWrapper(str, expected, 1);
    }
    {
        char str[] = "1k";
        char expected[1][KIBIBYTE(1)] = { "1" };
        FindNextIntegerTestWrapper(str, expected, 1);
    }
    {
        char str[] = "1234";
        char expected[1][KIBIBYTE(1)] = { "1234" };
        FindNextIntegerTestWrapper(str, expected, 1);
    }
    {
        char str[] = "1 2 3 4";
        char expected[4][KIBIBYTE(1)] = { "1", "2", "3", "4" };
        FindNextIntegerTestWrapper(str, expected, 4);
    }
    {
        char str[] = "Debian 9";
        char expected[1][KIBIBYTE(1)] = { "9" };
        FindNextIntegerTestWrapper(str, expected, 1);
    }
    {
        char str[] = "Cent OS 6.7";
        char expected[2][KIBIBYTE(1)] = { "6", "7" };
        FindNextIntegerTestWrapper(str, expected, 2);
    }
    {
        char str[] = "CFEngine 3.18.0-2deadbeef";
        char expected[4][KIBIBYTE(1)] = { "3", "18", "0", "2" };
        FindNextIntegerTestWrapper(str, expected, 4);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_uptime),
        unit_test(test_find_next_integer)
    };

    return run_tests(tests);
}
