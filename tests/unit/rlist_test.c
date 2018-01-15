#include <test.h>

#include <stdlib.h>

#include <rlist.h>
#include <string_lib.h>

#include <assoc.h>
#include <eval_context.h>

/* Stubs */

void FatalError(ARG_UNUSED char *s, ...)
{
    mock_assert(0, "0", __FILE__, __LINE__);
    abort();
}

static void test_length(void)
{
    Rlist *list = NULL;

    assert_int_equal(RlistLen(list), 0);

    RlistPrepend(&list, "stuff", RVAL_TYPE_SCALAR);
    assert_int_equal(RlistLen(list), 1);

    RlistPrepend(&list, "more-stuff", RVAL_TYPE_SCALAR);
    assert_int_equal(RlistLen(list), 2);

    RlistDestroy(list);
}

static void test_prepend_scalar_idempotent(void)
{
    Rlist *list = NULL;

    RlistPrependScalarIdemp(&list, "stuff");
    RlistPrependScalarIdemp(&list, "stuff");

    assert_string_equal(RlistScalarValue(list), "stuff");
    assert_int_equal(RlistLen(list), 1);

    RlistDestroy(list);
}

static void test_copy(void)
{
    Rlist *list = NULL, *copy = NULL;

    RlistPrepend(&list, "stuff", RVAL_TYPE_SCALAR);
    RlistPrepend(&list, "more-stuff", RVAL_TYPE_SCALAR);

    copy = RlistCopy(list);

    assert_string_equal(RlistScalarValue(list), RlistScalarValue(copy));
    assert_string_equal(RlistScalarValue(list->next), RlistScalarValue(copy->next));

    RlistDestroy(list);
    RlistDestroy(copy);
}

static void test_rval_to_scalar(void)
{
    Rval rval = { "abc", RVAL_TYPE_SCALAR };
    assert_string_equal("abc", RvalScalarValue(rval));
}

static void test_rval_to_scalar2(void)
{
    Rval rval = { NULL, RVAL_TYPE_FNCALL };
    expect_assert_failure(RvalScalarValue(rval));
}

static void test_rval_to_list(void)
{
    Rval rval = { NULL, RVAL_TYPE_SCALAR };
    expect_assert_failure(RvalRlistValue(rval));
}

static void test_rval_to_list2(void)
{
    Rval rval = { NULL, RVAL_TYPE_LIST };
    assert_false(RvalRlistValue(rval));
}

static void test_rval_to_fncall(void)
{
    Rval rval = { NULL, RVAL_TYPE_SCALAR };
    expect_assert_failure(RvalFnCallValue(rval));
}

static void test_rval_to_fncall2(void)
{
    Rval rval = { NULL, RVAL_TYPE_FNCALL };
    assert_false(RvalFnCallValue(rval));
}

static void test_last(void)
{
    Rlist *l = NULL;
    assert_true(RlistLast(l) == NULL);
    RlistAppendScalar(&l, "a");
    assert_string_equal("a", RlistScalarValue(RlistLast(l)));
    RlistAppendScalar(&l, "b");
    assert_string_equal("b", RlistScalarValue(RlistLast(l)));
    RlistDestroy(l);
}

static bool is_even(void *item, void *data)
{
    long d = StringToLong(data);
    long i = StringToLong(item);
    return i % 2 == d;
}

static void test_filter(void)
{
    Rlist *list = NULL;
    for (int i = 0; i < 10; i++)
    {
        char *item = StringFromLong(i);
        RlistAppend(&list, item, RVAL_TYPE_SCALAR);
    }

    assert_int_equal(10, RlistLen(list));
    int mod_by = 0;
    RlistFilter(&list, is_even, &mod_by, free);
    assert_int_equal(5, RlistLen(list));

    int i = 0;
    for (Rlist *rp = list; rp; rp = rp->next)
    {
        int k = StringToLong(rp->val.item);
        assert_int_equal(i, k);

        i += 2;
    }

    RlistDestroy(list);
}

static void test_filter_everything(void)
{
    Rlist *list = NULL;
    for (int i = 1; i < 10; i += 2)
    {
        char *item = StringFromLong(i);
        RlistAppend(&list, item, RVAL_TYPE_SCALAR);
    }

    assert_int_equal(5, RlistLen(list));
    int mod_by = 0;
    RlistFilter(&list, is_even, &mod_by, free);
    assert_int_equal(0, RlistLen(list));

    assert_true(list == NULL);
}

static void test_reverse(void)
{
    Rlist *list = RlistFromSplitString("a,b,c", ',');

    RlistReverse(&list);
    assert_int_equal(3, RlistLen(list));
    assert_string_equal("c", RlistScalarValue(list));
    assert_string_equal("b", RlistScalarValue(list->next));
    assert_string_equal("a", RlistScalarValue(list->next->next));

    RlistDestroy(list);
}

static void test_split_escaped(void)
{
    Rlist *list = RlistFromSplitString("a\\,b\\c\\,d,w\\,x\\,y\\,z", ',');
    assert_int_equal(2, RlistLen(list));
    assert_string_equal("a,b\\c,d", RlistScalarValue(list));
    assert_string_equal("w,x,y,z", RlistScalarValue(list->next));

    RlistDestroy(list);
}

static void test_split_long(void)
{
    char buf[CF_MAXVARSIZE * 2], *tail = buf + CF_MAXVARSIZE;
    memset(buf, '$', sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    buf[CF_MAXVARSIZE - 1] = ',';

    Rlist *list = RlistFromSplitString(buf, ',');
    assert_int_equal(2, RlistLen(list));

    assert_string_equal(tail, RlistScalarValue(list));
    assert_string_equal(tail, RlistScalarValue(list->next));

    RlistDestroy(list);
}

static void test_split_long_escaped(void)
{
    char buf[CF_MAXVARSIZE * 2 + 2], *tail = buf + CF_MAXVARSIZE + 1;
    memset(buf, '$', sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    buf[CF_MAXVARSIZE] = ',';
    memcpy(buf + CF_MAXVARSIZE / 2, "\\,", 2);
    memcpy(tail + CF_MAXVARSIZE / 2, "\\,", 2);

    Rlist *list = RlistFromSplitString(buf, ',');
    assert_int_equal(2, RlistLen(list));

    tail[CF_MAXVARSIZE / 2] = '$'; /* blot out the back-slash */
    assert_string_equal(tail + 1, RlistScalarValue(list));
    assert_string_equal(tail + 1, RlistScalarValue(list->next));

    RlistDestroy(list);
}
/***************************************************************************/
static struct ParseRoulette
{
    int nfields;
    char *str;
} PR[] =
{
        /*Simple */
    {
    1, "{\"a\"}"},
    {
    2, "{\"a\",\"b\"}"},
    {
    3, "{\"a\",\"b\",\"c\"}"},
        /*Simple empty */
    {
    1, "{\"\"}"},
    {
    2, "{\"\",\"\"}"},
    {
    3, "{\"\",\"\",\"\"}"},
        /*Simple mixed kind of quotations */
    {
    1, "{\"'\"}"},
    {
    1, "{'\"'}"},
    {
    1, "{\",\"}"},
    {
    1, "{','}"},
    {
    1, "{\"\\\\\"}"},
    {
    1, "{'\\\\'}"},
    {
    1, "{\"}\"}"},
    {
    1, "{'}'}"},
    {
    1, "{\"{\"}"},
    {
    1, "{'{'}"},
    {
    1, "{\"'\"}"},
    {
    1, "{'\"'}"},
    {
    1, "{'\\\",'}"},          /*   [",]    */
    {
    1, "{\"\\',\"}"},          /*   [",]    */

    {
    1, "{',\\\"'}"},          /*   [,"]    */
    {
    1, "{\",\\'\"}"},          /*   [,"]    */

    {
    1, "{\",,\"}"},             /*   [\\]    */
    {
    1, "{',,'}"},             /*   [\\]    */

    {
    1, "{\"\\\\\\\\\"}"},       /*   [\\]    */
    {
    1, "{'\\\\\\\\'}"},       /*   [\\]    */

    {
    1, "{'\\\\\\\"'}"},       /*   [\"]    */
    {
    1, "{\"\\\\\\'\"}"},       /*   [\"]    */

    {
    1, "{'\\\"\\\\'}"},       /*   ["\]    */
    {
    1, "{\"\\'\\\\\"}"},       /*   ["\]    */

        /*Very long */
    {
    1, "{\"AaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA\"}"},
    {
    1, "{'AaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA'}"},

    {
    2, "{\"Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa''aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA\"  ,  \"Bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb''bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbB\" }"},
    {
    2, "{'Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\\\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA'  ,  'Bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbB' }"},
    {
    2, "{\"Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa''aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA\"  ,  'Bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbB' }"},

        /*Inner space (inside elements) */
    {
    1, "{\" \"}"},
    {
    1, "{\"  \"}"},
    {
    1, "{\"   \"}"},
    {
    1, "{\"\t\"}"},
        /*Outer space (outside elements) */
    {
    1, "     {\"\"}       "},
    {
    1, "     {\"a\"}       "},
    {
    2, "     {\"a\",\"b\"}       "},
    {
    1, "{    \"a\"      }"},
    {
    2, "{    \"a\",\"b\"      }"},
    {
    2, "{    \"a\"    ,\"b\"      }"},
    {
    2, "{    \"a\",    \"b\"      }"},
    {
    2, "{    \"a\",    \"b\"}       "},
        /*Normal */
    {
    4, "   { \" ab,c,d\\\\ \" ,  ' e,f\\\"g ' ,\"hi\\\\jk\", \"l''m \" }   "},
    {
    21, "   { 'A\\\"\\\\    ', \"    \\\\\",   \"}B\",   \"\\\\\\\\\"  ,   \"   \\\\C\\'\"  ,   \"\\',\"  ,   ',\\\"D'  ,   \"   ,,    \", \"E\\\\\\\\F\", \"\", \"{\",   \"   G    '\"  ,   \"\\\\\\'\", ' \\\"  H \\\\    ', \",   ,\"  ,   \"I\", \"  \",   \"\\'    J  \",   '\\\",', \",\\'\", \",\"  }   "},
    {
    3,  "{   \"   aaa    \",    \"    bbbb       \"  ,   \"   cc    \"          }    "},
    {
    3,  "  {   \"   a'a    \",    \"    b''b       \"  ,   \"   c'c   \"          }    "},
    {
    3,  "  {   '   a\"a    ',    '    b\"\"b       '  ,   '   c\"c   '          }    "},
    {
    3,  "  {   '   a\"a    ',    \"    b''b       \"  ,   '   c\"c   '          }    "},
    {
    3,  "  {   '   a,\"a } { ',    \"  } b','b       \"  ,   ' {, c\"c } '          }    "},
    {
    -1, (char *)NULL}
};

static char *PFR[] = {
    /* trim left failure */
    "",
    " ",
    "a",
    "\"",
    "'",
    "\"\"",
    "''",
    "'\"",
    "\"'",
    /* trim right failure */
    "{",
    "{ ",
    "{a",
    "{\"",
    "{'",
    "{\"\"",
    "{''",
    "{\"'",
    /* parse failure */
    /* un-even number of quotation marks */
    "{\"\"\"}",
    "{\"\",\"}",
    "{\"\"\"\"}",
    "{\"\"\"\"\"}",
    "{\"\",\"\"\"}",
    "{\"\"\"\",\"}",
    "{\"\",\"\",\"}",
    "{'''}",
    "{'','}",
    "{''''}",
    "{'''''}",
    "{'','''}",
    "{'''','}",
    "{\"}",
    "{'}",
    "{'','','}",
    "{\"\"'}",
    "{\"\",'}",
    "{\"'\"'}",
    "{\"\"'\"\"}",
    "{\"\",'\"\"}",
    "{'\"\"\",\"}",
    "{'',\"\",'}",

    /* Misplaced commas*/
    "{\"a\",}",
    "{,\"a\"}",
    "{,,\"a\"}",
    "{\"a\",,\"b\"}",
    "{'a',}",
    "{,'a'}",
    "{,,'a'}",
    "{'a',,'b'}",
    "{\"a\",,'b'}",
    "{'a',,\"b\"}",
    " {,}",
    " {,,}",
    " {,,,}",
    " {,\"\"}",
    " {\"\",}",
    " {,\"\",}",
    " {\"\",,}",
    " {\"\",,,}",
    " {,,\"\",,}",
    " {\"\",\"\",}",
    " {\"\",\"\",,}",
    " {   \"\"  ,  \"\" ,  , }",
    " {,''}",
    " {'',}",
    " {,'',}",
    " {'',,}",
    " {'',,,}",
    " {,,'',,}",
    " {'','',}",
    " {'','',,}",
    " {   ''  ,  '' ,  , }",
    " {'',\"\",}",
    " {\"\",'',,}",
    " {   ''  ,  \"\" ,  , }",
    " {   \"\"  ,  '' ,  , }",

    /*Ignore space's oddities */
    "\" {\"\"}",
    "{ {\"\"}",
    "{\"\"}\"",
    "{\"\"}\\",
    "{\"\"} } ",
    "a{\"\"}",
    " a {\"\"}",
    "{a\"\"}",
    "{ a \"\"}",
    "{\"\"}a",
    "{\"\"}  a ",
    "{\"\"a}",
    "{\"\" a }",
    "a{\"\"}b",
    "{a\"\"b}",
    "a{\"\"b}",
    "{a\"\"}b",
    "{\"\"a\"\"}",
    "{\"\",\"\"a\"\"}",
    "' {''}",
    "{ {''}",
    "{''}'",
    "{''}\\",
    "{''} } ",
    "a{''}",
    " a {''}",
    "{a''}",
    "{ a ''}",
    "{''}a",
    "{''}  a ",
    "{''a}",
    "{'' a }",
    "a{''}b",
    "{a''b}",
    "a{''b}",
    "{a''}b",
    "{''a''}",
    "{'',''a''}",
    "{''a\"\"}",
    "{\"\"a''}",
    "{\"\",''a''}",
    "{'',\"\"a''}",
    "{'',''a\"\"}",
    "{\"\",''a\"\"}",
    /* Bad type of quotation inside an element */
    "{'aa'aa'}",
    "{\"aa\"aa\"}",
    "{'aa\"''}",
    "{'aa\"\"''}",
    "{\"aa'\"\"}",
    "{\"aa''\"\"}",
    "{'aa\"'', 'aa\"\"'',\"aa'\"\"}",
    "{\"aa\"aa\", 'aa\"'', 'aa\"\"''}",
    "{ \"aa\"aa\"   ,'aa\"\"'',\"aa''\"\"   }",
    NULL
};


static void test_new_parser_success()
{
    Rlist *list = NULL;
    int i = 0;
    while (PR[i].nfields != -1)
    {
        list = RlistParseString(PR[i].str);
        assert_int_equal(PR[i].nfields, RlistLen(list));
        if (list != NULL)
        {
            RlistDestroy(list);
        }
        i++;
    }
}

static void test_new_parser_failure()
{
    int i = 0;
    Rlist *list = NULL;
    while (PFR[i] != NULL)
    {
        list = RlistParseString(PFR[i]);
        assert_true(RlistLast(list) == NULL);
        if(list) RlistDestroy(list);
        i++;
    }
}

static void test_regex_split()
{
    Rlist *list = RlistFromRegexSplitNoOverflow("one-->two-->three", "-+>", 3);

    assert_int_equal(3, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "one");
    assert_string_equal(RlistScalarValue(list->next), "two");
    assert_string_equal(RlistScalarValue(list->next->next), "three");

    RlistDestroy(list);
}

static void test_regex_split_too_few_chunks()
{
    Rlist *list = RlistFromRegexSplitNoOverflow("one:two:three", ":", 2);

    assert_int_equal(2, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "one");
    assert_string_equal(RlistScalarValue(list->next), "two:three");

    RlistDestroy(list);
}

static void test_regex_split_too_many_chunks()
{
    Rlist *list = RlistFromRegexSplitNoOverflow("one:two:three:", ":", 10);

    assert_int_equal(4, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "one");
    assert_string_equal(RlistScalarValue(list->next), "two");
    assert_string_equal(RlistScalarValue(list->next->next), "three");
    assert_string_equal(RlistScalarValue(list->next->next->next), "");

    RlistDestroy(list);
}

static void test_regex_split_empty_chunks()
{
    Rlist *list = RlistFromRegexSplitNoOverflow(":one:two:three:", ":", 5);

    assert_int_equal(5, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "");
    assert_string_equal(RlistScalarValue(list->next), "one");
    assert_string_equal(RlistScalarValue(list->next->next), "two");
    assert_string_equal(RlistScalarValue(list->next->next->next), "three");
    assert_string_equal(RlistScalarValue(list->next->next->next->next), "");

    RlistDestroy(list);
}

static void test_regex_split_no_match()
{
    Rlist *list = RlistFromRegexSplitNoOverflow(":one:two:three:", "/", 2);

    assert_int_equal(1, RlistLen(list));
    assert_string_equal(RlistScalarValue(list), ":one:two:three:");

    RlistDestroy(list);
}

static void test_regex_split_adjacent_separators()
{
    Rlist *list = RlistFromRegexSplitNoOverflow(":one::two::three:", ":", 3);

    assert_int_equal(3, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "");
    assert_string_equal(RlistScalarValue(list->next), "one");
    assert_string_equal(RlistScalarValue(list->next->next), ":two::three:");

    RlistDestroy(list);


    list = RlistFromRegexSplitNoOverflow(":one::two:::three:", ":", 4);

    assert_int_equal(4, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "");
    assert_string_equal(RlistScalarValue(list->next), "one");
    assert_string_equal(RlistScalarValue(list->next->next), "");
    assert_string_equal(RlistScalarValue(list->next->next->next), "two:::three:");

    RlistDestroy(list);


    list = RlistFromRegexSplitNoOverflow(":one::two:::three:", ":", 7);

    assert_int_equal(7, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "");
    assert_string_equal(RlistScalarValue(list->next), "one");
    assert_string_equal(RlistScalarValue(list->next->next), "");
    assert_string_equal(RlistScalarValue(list->next->next->next), "two");
    assert_string_equal(RlistScalarValue(list->next->next->next->next), "");
    assert_string_equal(RlistScalarValue(list->next->next->next->next->next), "");
    assert_string_equal(RlistScalarValue(list->next->next->next->next->next->next), "three:");

    RlistDestroy(list);
}

static void test_regex_split_real_regex()
{
    //whole string is matched by regex in below example
    Rlist *list = RlistFromRegexSplitNoOverflow("one-two-three", ".+", 3);

    assert_int_equal(2, RlistLen(list));
    assert_string_equal(RlistScalarValue(list), "");
    assert_string_equal(RlistScalarValue(list->next), "");

    RlistDestroy(list);


    list = RlistFromRegexSplitNoOverflow("one>>>two<<<three<><>four", "[<>]+", 4);

    assert_int_equal(4, RlistLen(list));
    assert_string_equal(RlistScalarValue(list), "one");
    assert_string_equal(RlistScalarValue(list->next), "two");
    assert_string_equal(RlistScalarValue(list->next->next), "three");
    assert_string_equal(RlistScalarValue(list->next->next->next), "four");

    RlistDestroy(list);
}

static void test_regex_split_overlapping_delimiters()
{
    Rlist *list = RlistFromRegexSplitNoOverflow("-one---two---three", "--", 3);

    assert_int_equal(3, RlistLen(list));

    assert_string_equal(RlistScalarValue(list), "-one");
    assert_string_equal(RlistScalarValue(list->next), "-two");
    assert_string_equal(RlistScalarValue(list->next->next), "-three");

    RlistDestroy(list);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_prepend_scalar_idempotent),
        unit_test(test_length),
        unit_test(test_copy),
        unit_test(test_rval_to_scalar),
        unit_test(test_rval_to_scalar2),
        unit_test(test_rval_to_list),
        unit_test(test_rval_to_list2),
        unit_test(test_rval_to_fncall),
        unit_test(test_rval_to_fncall2),
        unit_test(test_last),
        unit_test(test_filter),
        unit_test(test_filter_everything),
        unit_test(test_reverse),
        unit_test(test_split_escaped),
        unit_test(test_split_long),
        unit_test(test_split_long_escaped),
        unit_test(test_new_parser_success),
        unit_test(test_new_parser_failure),
        unit_test(test_regex_split),
        unit_test(test_regex_split_too_few_chunks),
        unit_test(test_regex_split_too_many_chunks),
        unit_test(test_regex_split_empty_chunks),
        unit_test(test_regex_split_no_match),
        unit_test(test_regex_split_adjacent_separators),
        unit_test(test_regex_split_real_regex),
        unit_test(test_regex_split_overlapping_delimiters)
    };

    return run_tests(tests);
}

/* ===== Stub out functionality we don't really use. ===== */

/* Silence all "unused parameter" warnings. */
#pragma GCC diagnostic ignored "-Wunused-parameter"



char CONTEXTID[32];

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    mock_assert(0, "0", __FILE__, __LINE__);
}

int FullTextMatch(const char *regptr, const char *cmpptr)
{
    fail();
}

const void *EvalContextVariableGet(const EvalContext *ctx, const VarRef *lval, DataType *type_out)
{
    fail();
}

pthread_mutex_t *cft_lock;
int __ThreadLock(pthread_mutex_t *name)
{
    return true;
}

int __ThreadUnlock(pthread_mutex_t *name)
{
    return true;
}

int IsNakedVar(const char *str, char vtype)
{
    fail();
}

void FnCallPrint(Writer *writer, const FnCall *fp)
{
    fail();
}

void GetNaked(char *s1, const char *s2)
{
    fail();
}

/*
void Log(LogLevel level, const char *fmt, ...)
{
    fail();
}
*/

DataType ScopeGetVariable(const char *scope, const char *lval, Rval *returnv)
{
    fail();
}

void DeleteAssoc(CfAssoc *ap)
{
    fail();
}

CfAssoc *CopyAssoc(CfAssoc *old)
{
    fail();
}

FnCall *FnCallCopy(const FnCall *f)
{
    fail();
}

void FnCallDestroy(FnCall *fp)
{
    fail();
}

int SubStrnCopyChr(char *to, const char *from, int len, char sep)
{
    fail();
}

int BlockTextMatch(const char *regexp, const char *teststring, int *s, int *e)
{
    fail();
}

JsonElement *FnCallToJson(const FnCall *fp)
{
    fail();
}

JsonElement *JsonObjectCreate(size_t initialCapacity)
{
    fail();
}

void JsonObjectAppendArray(JsonElement *object, const char *key, JsonElement *array)
{
    fail();
}

void JsonObjectAppendString(JsonElement *obj, const char *key, const char *value)
{
    fail();
}

JsonElement *JsonArrayCreate(size_t initialCapacity)
{
    fail();
}

void JsonArrayAppendString(JsonElement *array, const char *value)
{
    fail();
}

void JsonArrayAppendArray(JsonElement *array, JsonElement *childArray)
{
    fail();
}

void JsonArrayAppendObject(JsonElement *array, JsonElement *object)
{
    fail();
}

JsonElement *JsonStringCreate(const char *value)
{
    fail();
}
