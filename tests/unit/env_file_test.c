#include <test.h>
#include <misc_lib.h>                       // xsnprintf()


#include <json-utils.c>

// Check the output of a single ParseEnvLine call:
#define single_line_check(inp, expected_key, expected_value) \
{                                                            \
    char *buf = malloc(strlen(inp)+1);                       \
    strcpy(buf, inp);                                        \
    char *key;                                               \
    char *value;                                             \
    ParseEnvLine(buf, &key, &value, NULL, 0);                \
    assert_string_int_equal(key, expected_key);              \
    assert_string_int_equal(value, expected_value);          \
    free(buf);                                               \
}

static void test_ParseEnvLine(void)
{
    // VALID INPUTS:
    single_line_check("KEY=VALUE", "KEY", "VALUE");
    single_line_check("NAME=\"Test Linux by CFEngine\"",
                      "NAME","Test Linux by CFEngine");
    single_line_check("ID=cfengineos", "ID", "cfengineos");
    single_line_check("VERSION=1234.5.0", "VERSION", "1234.5.0");
    single_line_check("BUILD_ID=2017-02-14-2245",
                      "BUILD_ID", "2017-02-14-2245");
    single_line_check("PRETTY_NAME=\"Test Linux by CFEngine 1234.5.0 (Leafant)\"",
                      "PRETTY_NAME","Test Linux by CFEngine 1234.5.0 (Leafant)");
    single_line_check("ANSI_COLOR=\"37;4;65\"", "ANSI_COLOR", "37;4;65");
    single_line_check("HOME_URL=\"https://cfengine.com/\"",
                      "HOME_URL", "https://cfengine.com/");
    single_line_check("BUG_REPORT_URL=\"https://tracker.mender.io/projects/CFE/issues\"",
                      "BUG_REPORT_URL","https://tracker.mender.io/projects/CFE/issues");

    // COMMENTS:
    single_line_check("#COMMENTED_OUT=TRUE",      NULL, NULL);
    single_line_check("#COMMENTED_OUT=\"TRUE\"",  NULL, NULL);
    single_line_check("# COMMENTED_OUT=TRUE",     NULL, NULL);
    single_line_check(" # COMMENTED_OUT=TRUE",    NULL, NULL);
    single_line_check("  # COMMENTED_OUT=TRUE  ", NULL, NULL);

    // Not a comment if line doesn't start with #:
    single_line_check("COLOR=\"#00FFFF\"","COLOR","#00FFFF");

    // QUOTE CLOSING:
    single_line_check("KEY=\"VALUE\" This is not parsed", "KEY", "VALUE");
    single_line_check("KEY='VALUE'   This is not parsed", "KEY", "VALUE");

    // ESCAPE CHARACTERS:
    single_line_check("KEY=\\'",        "KEY", "'");
    single_line_check("KEY=\\\"",       "KEY", "\"");
    single_line_check("KEY=\" \\\" \"", "KEY", " \" ");
    single_line_check("KEY=\"\\\"\"",   "KEY", "\"");

    // SINGLE QUOTES:
    single_line_check("KEY='VALUE'", "KEY", "VALUE")
    single_line_check("KEY='\"'",    "KEY", "\"")
    single_line_check("KEY='\"\"'",  "KEY", "\"\"")

    // SLIGHTLY WEIRD BUT ACCEPTED:
    single_line_check("KEY=VALUE\n",     "KEY", "VALUE");
    single_line_check("KEY=VALUE \n",    "KEY", "VALUE");
    single_line_check("KEY= \"VALUE\" ", "KEY", "VALUE");
    single_line_check("KEY=\"\"",        "KEY", "");
    single_line_check("KEY=\\\\",        "KEY", "\\");
    single_line_check("KEY=",            "KEY", "");
    single_line_check("KEY=\"\" <- String terminated", "KEY", "");

    // UNCLOSED QUOTES:
    single_line_check("KEY=\"Oops", "KEY", "Oops");
    single_line_check("KEY='Woops", "KEY", "Woops");

    // NEWLINES:
    single_line_check("KEY=\\n",       "KEY", "\n");
    single_line_check("KEY=\"\\n",     "KEY", "\n");
    single_line_check("KEY=\"\\n\"",   "KEY", "\n");
    single_line_check("KEY='\\n'",     "KEY", "\n");
    single_line_check("KEY='AB\\nCD'", "KEY", "AB\nCD");

    // INVALID INPUTS:
    single_line_check("=VALUE",        NULL, NULL);
    single_line_check(" =\"VALUE\"",   NULL, NULL);
    single_line_check("  =\"VALUE\"",  NULL, NULL);
    single_line_check("   =\"VALUE\"", NULL, NULL);

    // CORNER CASES:
    single_line_check("",    NULL, NULL);
    single_line_check(" ",   NULL, NULL);
    single_line_check(" = ", NULL, NULL);
    single_line_check("= ",  NULL, NULL);
    single_line_check(" =",  NULL, NULL);
    single_line_check("\n",  NULL, NULL);
    single_line_check(" \n", NULL, NULL);
}

// Tests filtered_copy from src to different dst
// Keeps a 'backup' of original contents of src to check
// that it's not modified.
#define single_filter_check_moved(src, expected)            \
{                                                           \
    char* backup = malloc(strlen(src)+1);                   \
    strcpy(backup, src);                                    \
    char* buf = malloc(strlen(src)+1);                      \
    char* start = filtered_copy(src, buf);                  \
    assert_string_int_equal(start, expected);               \
    assert_string_int_equal(backup, src)                    \
    free(backup);                                           \
    free(buf);                                              \
}                                                           \

// Tests filtered_copy for src=dst (in place)
#define single_filter_check_in_place(src, expected)         \
{                                                           \
    char* buf = malloc(strlen(src)+1);                      \
    strcpy(buf, src);                                       \
    char *value = buf;                                      \
    value = filtered_copy(value, value);                    \
    assert_string_int_equal(value, expected);               \
    free(buf);                                              \
}                                                           \

#define double_filter_check(src, expected)                  \
{                                                           \
    single_filter_check_in_place(src, expected)             \
    single_filter_check_moved(src, expected)                \
}                                                           \

#define filter_expect_null(invalid_input)                   \
{                                                           \
    char *buf = malloc(1024);                               \
    strcpy(buf, "AB\"CD");                                  \
    char *ret = filtered_copy(buf,buf);                     \
    assert(ret == NULL);                                    \
    free(buf);                                              \
}                                                           \

static void test_filtered_copy(void)
{
    // STRING COPY:
    double_filter_check("ABC123abc123", "ABC123abc123");
    double_filter_check("", "");
    double_filter_check(" ", " ");
    double_filter_check("  ", "  ");
    double_filter_check("\t", "\t");

    // ESCAPE CHARACTERS:
    double_filter_check("\\\\", "\\");
    double_filter_check("\\\\\\\\", "\\\\");
    double_filter_check("\\\"", "\"");
    double_filter_check("\\'", "'");

    // QUOTES:
    double_filter_check("\"\"", "");
    double_filter_check("''", "");
    double_filter_check("\"Hello world!\"", "Hello world!");
    double_filter_check("'Hello world!'", "Hello world!");
    double_filter_check("'\"'", "\"");
    double_filter_check("\"'\"", "'");

    // QUOTES AND ESCAPE CHARACTERS:
    double_filter_check("\"\\\"\"", "\"")    // "\"" -> "
    double_filter_check("'\\''", "'")        // '\'' -> '

    // AUTO CLOSE QUOTES:
    double_filter_check("\"Hello ", "Hello ");
    double_filter_check("'Hello ", "Hello ");

    // CLOSE QUOTES:
    double_filter_check("\"This\" Not this", "This");
    double_filter_check("'This' Not this", "This");

    // EXPECTED TO RETURN NULL (INVALID):
    filter_expect_null("AB\"CD");
    filter_expect_null("AB'CD");
    filter_expect_null(" ' ");
    filter_expect_null(" \" ");
}

int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_filtered_copy),
        unit_test(test_ParseEnvLine),
    };

    int ret = run_tests(tests);

    return ret;
}
