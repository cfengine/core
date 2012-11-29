#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>
#include <assert.h>

#include "cf3.defs.h"

#include "assoc.h"

/* Stubs */

void FatalError(char *s, ...)
{
    mock_assert(0, "0", __FILE__, __LINE__);
    abort();
}

/* Test cases */

static void test_prepend_scalar(void **state)
{
    Rlist *list = NULL;

    PrependRScalar(&list, "stuff", CF_SCALAR);
    PrependRScalar(&list, "more-stuff", CF_SCALAR);

    assert_string_equal(list->item, "more-stuff");

    DeleteRlist(list);
}

static void test_length(void **state)
{
    Rlist *list = NULL;

    assert_int_equal(RlistLen(list), 0);

    PrependRScalar(&list, "stuff", CF_SCALAR);
    assert_int_equal(RlistLen(list), 1);

    PrependRScalar(&list, "more-stuff", CF_SCALAR);
    assert_int_equal(RlistLen(list), 2);

    DeleteRlist(list);
}

static void test_prepend_scalar_idempotent(void **state)
{
    Rlist *list = NULL;

    IdempPrependRScalar(&list, "stuff", CF_SCALAR);
    IdempPrependRScalar(&list, "stuff", CF_SCALAR);

    assert_string_equal(list->item, "stuff");
    assert_int_equal(RlistLen(list), 1);

    DeleteRlist(list);
}

static void test_copy(void **state)
{
    Rlist *list = NULL, *copy = NULL;

    PrependRScalar(&list, "stuff", CF_SCALAR);
    PrependRScalar(&list, "more-stuff", CF_SCALAR);

    copy = CopyRlist(list);

    assert_string_equal(list->item, copy->item);
    assert_string_equal(list->next->item, copy->next->item);

    DeleteRlist(list);
    DeleteRlist(copy);
}

static void test_rval_to_scalar(void **state)
{
    Rval rval = { "abc", CF_SCALAR };
    assert_string_equal("abc", ScalarRvalValue(rval));
}

static void test_rval_to_scalar2(void **state)
{
    Rval rval = { NULL, CF_FNCALL };
    expect_assert_failure(ScalarRvalValue(rval));
}

static void test_rval_to_list(void **state)
{
    Rval rval = { NULL, CF_SCALAR };
    expect_assert_failure(ListRvalValue(rval));
}

static void test_rval_to_list2(void **state)
{
    Rval rval = { NULL, CF_LIST };
    assert_false(ListRvalValue(rval));
}

static void test_rval_to_fncall(void **state)
{
    Rval rval = { NULL, CF_SCALAR };
    expect_assert_failure(FnCallRvalValue(rval));
}

static void test_rval_to_fncall2(void **state)
{
    Rval rval = { NULL, CF_FNCALL };
    assert_false(FnCallRvalValue(rval));
}

static void test_last(void **state)
{
    Rlist *l = NULL;
    assert_true(RlistLast(l) == NULL);
    AppendRlist(&l, "a", CF_SCALAR);
    assert_string_equal("a", ScalarValue(RlistLast(l)));
    AppendRlist(&l, "b", CF_SCALAR);
    assert_string_equal("b", ScalarValue(RlistLast(l)));
    DeleteRlist(l);
}

static bool is_even(void *item, void *data)
{
    int *d = data;

    int *i = item;
    return *i % 2 == *d;
}

static void test_filter(void **state)
{
    Rlist *list = NULL;
    for (int i = 0; i < 10; i++)
    {
        void *item = xmemdup(&i, sizeof(int));
        AppendRlistAlien(&list, item);
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

    DeleteRlist(list);
}

static void test_filter_everything(void **state)
{
    Rlist *list = NULL;
    for (int i = 1; i < 10; i += 2)
    {
        void *item = xmemdup(&i, sizeof(int));
        AppendRlistAlien(&list, item);
    }

    assert_int_equal(5, RlistLen(list));
    int mod_by = 0;
    RlistFilter(&list, is_even, &mod_by, free);
    assert_int_equal(0, RlistLen(list));

    assert_true(list == NULL);
}

int main()
{
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
        unit_test(test_filter_everything)
    };

    return run_tests(tests);
}

/* Stub out functionality we don't really use */

int DEBUG;
char CONTEXTID[32];

int FullTextMatch(const char *regptr, const char *cmpptr)
{
    fail();
}

#if defined(HAVE_PTHREAD)
pthread_mutex_t *cft_lock;
pthread_mutex_t *cft_system;
int ThreadLock(pthread_mutex_t *name)
{
    return true;
}

int ThreadUnlock(pthread_mutex_t *name)
{
    return true;
}
#endif

void ShowFnCall(FILE *fout, const FnCall *fp)
{
    fail();
}

void CfOut(enum cfreport level, const char *errstr, const char *fmt, ...)
{
    fail();
}

int IsNakedVar(const char *str, char vtype)
{
    fail();
}

int JoinSilent(char *path, const char *leaf, int bufsize)
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

enum cfdatatype GetVariable(const char *scope, const char *lval, Rval *returnv)
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

FnCall *CopyFnCall(const FnCall *f)
{
    fail();
}

int EndJoin(char *path, char *leaf, int bufsize)
{
    fail();
}

char *EscapeQuotes(const char *s, char *out, int outSz)
{
    fail();
}

void DeleteFnCall(FnCall *fp)
{
    fail();
}

int PrintFnCall(char *buffer, int bufsize, const FnCall *fp)
{
    fail();
}

int SubStrnCopyChr(char *to, const char *from, int len, char sep)
{
    fail();
}

int StartJoin(char *path, char *leaf, int bufsize)
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
