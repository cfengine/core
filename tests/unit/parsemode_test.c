#include <test.h>

#include <cf3.defs.h>

typedef struct {
   char *string;
   mode_t plus;
   mode_t minus;
} mode_definition;

mode_definition modes[] = {
   { "666", S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH, S_IXUSR|S_IXGRP|S_IXOTH|S_ISUID|S_ISGID|S_ISVTX },
   { "g+w", S_IWGRP, 0 },
   { "u+r,u+w,g-w,o-rw", S_IRUSR|S_IWUSR, S_IWGRP|S_IWOTH|S_IROTH },
   { NULL, 0, 0 } // last case, still tested
};

void test_mode(void)
{
    bool ret = false;
    mode_t plus = 0;
    mode_t minus = 0;

    int mode = 0;
    do
    {
        ret = ParseModeString(modes[mode].string, &plus, &minus);
        assert_true(ret);
        assert_int_equal(modes[mode].plus, plus);
        assert_int_equal(modes[mode].minus, minus);
    } while (modes[mode++].string);
}

typedef struct {
    char *string;
    bool valid;
} validation_mode;

validation_mode validation_modes[] = {
    { "", false },
    { "abc", false },
    { "222222", false },
    { "22222", true },
    { NULL, true } // last case, still tested
};

void test_validation(void)
{
    int ret = false;
    mode_t minus = 0;
    mode_t plus = 0;

    int mode = 0;
    do
    {
       ret = ParseModeString( validation_modes[mode].string, &plus, &minus);
       assert_int_equal(validation_modes[mode].valid, ret);
    } while (validation_modes[mode++].string);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_validation),
        unit_test(test_mode)
    };
    return run_tests(tests);
}
