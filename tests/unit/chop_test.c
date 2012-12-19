#include "cf3.defs.h"
#include "files_names.h"

#include <setjmp.h>
#include <cmockery.h>

static void test_no_spaces(void **state)
{
    char s[] = "abc";
    Chop(s);
    assert_string_equal("abc", s);
}

static void test_single_space(void **state)
{
    char s[] = "abc ";
    Chop(s);
    assert_string_equal("abc", s);
}

static void test_two_spaces(void **state)
{
    char s[] = "abc  ";
    Chop(s);
    assert_string_equal("abc", s);
}

static void test_empty(void **state)
{
    char s[] = "";
    Chop(s);
    assert_string_equal("", s);
}

static void test_empty_single_space(void **state)
{
    char s[] = " ";
    Chop(s);
    assert_string_equal("", s);
}

static void test_empty_two_spaces(void **state)
{
    char s[] = "  ";
    Chop(s);
    assert_string_equal("", s);
}

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_no_spaces),
        unit_test(test_single_space),
        unit_test(test_two_spaces),
        unit_test(test_empty),
        unit_test(test_empty_single_space),
        unit_test(test_empty_two_spaces),
    };

    return run_tests(tests);
}

/* Stub out stuff we do not use in test */

#include "dir.h"

char CFWORKDIR[CF_BUFSIZE];
int DEBUG;
enum classes VSYSTEMHARDCLASS;

int cfstat(const char *path, struct stat *buf)
{
    fail();
}

void CfOut(enum cfreport level, const char *errstr, const char *fmt, ...)
{
    fail();
}

void CloseDir(Dir *dir)
{
    fail();
}

int ConsiderFile(const char *nodename, char *path, Attributes attr, Promise *pp)
{
    fail();
}

void FatalError(char *s, ...)
{
    fail();
}

char *MapName(char *s)
{
    fail();
}

Dir *OpenDirLocal(const char *dirname)
{
    fail();
}

const struct dirent *ReadDir(Dir *dir)
{
    fail();
}

void yyerror(const char *s)
{
    fail();
}
