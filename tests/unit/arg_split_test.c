#include "cf3.defs.h"

#include <setjmp.h>
#include <cmockery.h>

#include "conversion.h"
#include "exec_tools.h"

static void test_split_empty(void **state)
{
    char **s = ArgSplitCommand("");

    assert_true(s);
    assert_false(*s);
    ArgFree(s);
}

static void test_split_easy(void **state)
{
    char **s = ArgSplitCommand("zero one two");

    assert_string_equal(s[0], "zero");
    assert_string_equal(s[1], "one");
    assert_string_equal(s[2], "two");
    assert_false(s[3]);

    ArgFree(s);
}

static void test_split_quoted_beginning(void **state)
{
    char **s = ArgSplitCommand("\"quoted string\" atbeginning");

    assert_string_equal(s[0], "quoted string");
    assert_string_equal(s[1], "atbeginning");
    assert_false(s[2]);
    ArgFree(s);
}

static void test_split_quoted_end(void **state)
{
    char **s = ArgSplitCommand("atend 'quoted string'");

    assert_string_equal(s[0], "atend");
    assert_string_equal(s[1], "quoted string");
    assert_false(s[2]);
    ArgFree(s);
}

static void test_split_quoted_middle(void **state)
{
    char **s = ArgSplitCommand("at `quoted string` middle");

    assert_string_equal(s[0], "at");
    assert_string_equal(s[1], "quoted string");
    assert_string_equal(s[2], "middle");
    assert_false(s[3]);
    ArgFree(s);
}

static void test_complex_quoting(void **state)
{
    char **s = ArgSplitCommand("\"foo`'bar\"");

    assert_string_equal(s[0], "foo`'bar");
    assert_false(s[1]);
    ArgFree(s);
}

static void test_arguments_resize_for_null(void **state)
{
/* This test checks that extending returned argument list for NULL terminator
 * works correctly */
    char **s = ArgSplitCommand("0 1 2 3 4 5 6 7");

    assert_string_equal(s[7], "7");
    assert_false(s[8]);
    ArgFree(s);
}

static void test_arguments_resize(void **state)
{
    char **s = ArgSplitCommand("0 1 2 3 4 5 6 7 8");

    assert_string_equal(s[7], "7");
    assert_string_equal(s[8], "8");
    assert_false(s[9]);
    ArgFree(s);
}

static void test_command_promiser(void **state)
{
    char *t1 = "/bin/echo";
    assert_string_equal(CommandArg0(t1), "/bin/echo");

    char *t2 = "/bin/rpm -qa --queryformat \"i | repos | %{name} | %{version}-%{release} | %{arch}\n\"";
    assert_string_equal(CommandArg0(t2), "/bin/rpm");
    
    char *t3 = "/bin/mount -va";
    assert_string_equal(CommandArg0(t3), "/bin/mount");

    char *t4 = "\"/bin/echo\"";
    assert_string_equal(CommandArg0(t4), "/bin/echo");
    
    char *t5 = "\"/bin/echo\" 123";
    assert_string_equal(CommandArg0(t5), "/bin/echo");

    char *t6 = "\"/bin/echo with space\" 123";
    assert_string_equal(CommandArg0(t6), "/bin/echo with space");

    char *t7 = "c:\\Windows\\System32\\cmd.exe";
    assert_string_equal(CommandArg0(t7), "c:\\Windows\\System32\\cmd.exe");

    char *t8 = "\"c:\\Windows\\System32\\cmd.exe\"";
    assert_string_equal(CommandArg0(t8), "c:\\Windows\\System32\\cmd.exe");

    char *t9 = "\"c:\\Windows\\System32\\cmd.exe\" /some args here";
    assert_string_equal(CommandArg0(t9), "c:\\Windows\\System32\\cmd.exe");

    char *t10 = "\"c:\\Windows\\System32 with space\\cmd.exe\"";
    assert_string_equal(CommandArg0(t10), "c:\\Windows\\System32 with space\\cmd.exe");

    char *t11 = "\"c:\\Windows\\System32 with space\\cmd.exe\" /some args here";
    assert_string_equal(CommandArg0(t11), "c:\\Windows\\System32 with space\\cmd.exe");

    char *t12 = "\"c:\\Windows\\System32 with space\\cmd.exe\" /some \"args here\"";
    assert_string_equal(CommandArg0(t12), "c:\\Windows\\System32 with space\\cmd.exe");

    char *t13 = "\\\\mycommand";
    assert_string_equal(CommandArg0(t13), "\\\\mycommand");

    char *t14 = "\\\\myhost\\share\\command.exe";
    assert_string_equal(CommandArg0(t14), "\\\\myhost\\share\\command.exe");

    char *t15 = "\"\\\\myhost\\share\\command.exe\"";
    assert_string_equal(CommandArg0(t15), "\\\\myhost\\share\\command.exe");


    /* bad input */

    char *b1 = "\"/bin/echo 123";
    assert_string_equal(CommandArg0(b1), "/bin/echo 123");

    char *b2 = "/bin/echo\" 123";
    assert_string_equal(CommandArg0(b2), "/bin/echo\"");

    char *b3 = "";
    assert_string_equal(CommandArg0(b3), "");
    
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_split_empty),
        unit_test(test_split_easy),
        unit_test(test_split_quoted_beginning),
        unit_test(test_split_quoted_middle),
        unit_test(test_split_quoted_end),
        unit_test(test_complex_quoting),
        unit_test(test_arguments_resize_for_null),
        unit_test(test_arguments_resize),
        unit_test(test_command_promiser),
    };

    return run_tests(tests);
}
