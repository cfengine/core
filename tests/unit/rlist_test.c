#include "test.h"

#include <stdlib.h>
#include <assert.h>

#include "rlist.h"

#include "assoc.h"
#include "env_context.h"

/* Stubs */

void FatalError(char *s, ...)
{
    mock_assert(0, "0", __FILE__, __LINE__);
    abort();
}

/* Test cases */

static void test_prepend_scalar(void)
{
    Rlist *list = NULL;

    RlistPrependScalar(&list, "stuff");
    RlistPrependScalar(&list, "more-stuff");

    assert_string_equal(list->item, "more-stuff");

    RlistDestroy(list);
}

static void test_length(void)
{
    Rlist *list = NULL;

    assert_int_equal(RlistLen(list), 0);

    RlistPrependScalar(&list, "stuff");
    assert_int_equal(RlistLen(list), 1);

    RlistPrependScalar(&list, "more-stuff");
    assert_int_equal(RlistLen(list), 2);

    RlistDestroy(list);
}

static void test_prepend_scalar_idempotent(void)
{
    Rlist *list = NULL;

    RlistPrependScalarIdemp(&list, "stuff");
    RlistPrependScalarIdemp(&list, "stuff");

    assert_string_equal(list->item, "stuff");
    assert_int_equal(RlistLen(list), 1);

    RlistDestroy(list);
}

static void test_copy(void)
{
    Rlist *list = NULL, *copy = NULL;

    RlistPrependScalar(&list, "stuff");
    RlistPrependScalar(&list, "more-stuff");

    copy = RlistCopy(list);

    assert_string_equal(list->item, copy->item);
    assert_string_equal(list->next->item, copy->next->item);

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
    int *d = data;

    int *i = item;
    return *i % 2 == *d;
}

static void test_filter(void)
{
    Rlist *list = NULL;
    for (int i = 0; i < 10; i++)
    {
        void *item = xmemdup(&i, sizeof(int));
        RlistAppendAlien(&list, item);
    }

    assert_int_equal(10, RlistLen(list));
    int mod_by = 0;
    RlistFilter(&list, is_even, &mod_by, free);
    assert_int_equal(5, RlistLen(list));

    int i = 0;
    for (Rlist *rp = list; rp; rp = rp->next)
    {
        int *k = rp->item;
        assert_int_equal(i, *k);

        free(k);
        rp->item = NULL;

        i += 2;
    }

    RlistDestroy(list);
}

static void test_filter_everything(void)
{
    Rlist *list = NULL;
    for (int i = 1; i < 10; i += 2)
    {
        void *item = xmemdup(&i, sizeof(int));
        RlistAppendAlien(&list, item);
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
    assert_string_equal("c", list->item);
    assert_string_equal("b", list->next->item);
    assert_string_equal("a", list->next->next->item);

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
        /*Single escaped */
    {
    1, "{\"\\\"\"}"},
    {
    1, "{\",\"}"},
    {
    1, "{\"\\\\\"}"},
    {
    1, "{\"}\"}"},
    {
    1, "{\"{\"}"},
    {
    1, "{\"'\"}"},
        /*Couple double-escaped */
    {
    1, "{\"\\\",\"}"},          /*   [",]    */
    {
    1, "{\",\\\"\"}"},          /*   [,"]    */
    {
    1, "{\",,\"}"},             /*   [\\]    */
    {
    1, "{\"\\\\\\\\\"}"},       /*   [\\]    */
    {
    1, "{\"\\\\\\\"\"}"},       /*   [\"]    */
    {
    1, "{\"\\\"\\\\\"}"},       /*   ["\]    */
        /*Very long */
    {
    1, "{\"AaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA\"}"},
    {
    2, "{\"Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\\\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA\"  ,  \"Bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbB\" }"},
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
    4, "   { \" ab,c,d\\\\ \" ,  \" e,f\\\"g \" ,\"hi\\\\jk\", \"l''m \" }   "},
    {
    21, "   { \"A\\\"\\\\    \", \"    \\\\\",   \"}B\",   \"\\\\\\\\\"  ,   \"   \\\\C\\\"\"  ,   \"\\\",\"  ,   \",\\\"D\"  ,   \"   ,,    \", \"E\\\\\\\\F\", \"\", \"{\",   \"   G    '\"  ,   \"\\\\\\\"\", \" \\\"  H \\\\    \", \",   ,\"  ,   \"I\", \"  \",   \"\\\"    J  \",   \"\\\",\", \",\\\"\", \",\"  }   "},
    {
    -1, (char *)NULL}
};

static char *PFR[] = {
    /* trim left failure */
    "",
    " ",
    "a",
    "\"",
    "\"\"",
    /* trim right failure */
    "{",
    "{ ",
    "{a",
    "{\"",
    "{\"\"",
    /* parse failure */
    /* un-even number of quotation marks */
    "{\"\"\"}",
    "{\"\",\"}",
    "{\"\"\"\"}",
    "{\"\"\"\"\"}",
    "{\"\",\"\"\"}",
    "{\"\"\"\",\"}",
    "{\"\",\"\",\"}",
    /* Misplaced commas*/
    "{\"a\",}",
    "{,\"a\"}",
    "{,,\"a\"}",
    "{\"a\",,\"b\"}",
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
    /*Incomplete */
    NULL
};


static void test_new_parser_success()
{
    Rlist *list = NULL;
    int i = 0;
    while (PR[i].nfields != -1)
    {
        list = RlistParseString(PR[i].str, NULL);
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
        list = RlistParseString(PFR[i], NULL);
        assert_true(RlistLast(list) == NULL);
        if(list) RlistDestroy(list);
        i++;
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_prepend_scalar),
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
        unit_test(test_new_parser_success),
        unit_test(test_new_parser_failure),
    };

    return run_tests(tests);
}

/* Stub out functionality we don't really use */

int DEBUG;
char CONTEXTID[32];

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    mock_assert(0, "0", __FILE__, __LINE__);
}

int FullTextMatch(const char *regptr, const char *cmpptr)
{
    fail();
}

bool EvalContextVariableGet(const EvalContext *ctx, VarRef lval, Rval *rval_out, DataType *type_out)
{
    fail();
}

Scope *ScopeGetCurrent(void)
{
    fail();
}

pthread_mutex_t *cft_lock;
int ThreadLock(pthread_mutex_t *name)
{
    return true;
}

int ThreadUnlock(pthread_mutex_t *name)
{
    return true;
}

void FnCallShow(FILE *fout, const FnCall *fp)
{
    fail();
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    fail();
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
