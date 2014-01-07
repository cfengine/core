#include <test.h>

#include <item_lib.h>
#include <eval_context.h>

static void test_prepend_item(void)
{
    Item *ip = NULL, *list = NULL;

    ip = PrependItem(&list, "hello", "classes");
    assert_int_not_equal(ip, NULL);
    assert_int_not_equal(list, NULL);
    DeleteItem(&list, ip);
    assert_int_equal(list, NULL);
}

static void test_list_len(void)
{
    Item *list = NULL;

    PrependItem(&list, "one", "classes");
    PrependItem(&list, "two", NULL);
    PrependItem(&list, "three", NULL);
    assert_int_equal(ListLen(list), 3);
    DeleteItemList(list);
}

/* FIXME: those functions are now internal to cf-agent */
#if 0
static void test_list_select_last_matching_finds_first(void)
{
    EvalContext *ctx = EvalContextNew();
    Item *list = NULL, *match = NULL, *prev = NULL;
    bool result = false;

    AppendItem(&list, "abc", NULL);
    AppendItem(&list, "def", NULL);
    AppendItem(&list, "ghi", NULL);
    AppendItem(&list, "jkl", NULL);

    result = SelectLastItemMatching(ctx, "abc", list, NULL, &match, &prev);
    assert_true(result);
    assert_int_equal(match, list);
    assert_int_equal(prev, CF_UNDEFINED_ITEM);
    DeleteItemList(list);
    EvalContextDestroy(ctx);
}

static void test_list_select_last_matching_finds_last(void)
{
    EvalContext *ctx = EvalContextNew();
    Item *list = NULL, *match = NULL, *prev = NULL;
    bool result;

    AppendItem(&list, "abc", NULL);
    AppendItem(&list, "def", NULL);
    AppendItem(&list, "ghi", NULL);
    AppendItem(&list, "abc", NULL);

    result = SelectLastItemMatching(ctx, "abc", list, NULL, &match, &prev);
    assert_true(result);
    assert_int_equal(match, list->next->next->next);
    assert_int_equal(prev, list->next->next);
    DeleteItemList(list);
    EvalContextDestroy(ctx);
}

static void test_list_select_last_matching_not_found(void)
{
    EvalContext *ctx = EvalContextNew();
    Item *list = NULL, *match = NULL, *prev = NULL;
    bool result;

    AppendItem(&list, "abc", NULL);
    AppendItem(&list, "def", NULL);
    AppendItem(&list, "ghi", NULL);
    AppendItem(&list, "abc", NULL);

    result = SelectLastItemMatching(ctx, "xyz", list, NULL, &match, &prev);
    assert_false(result);
    assert_int_equal(match, CF_UNDEFINED_ITEM);
    assert_int_equal(prev, CF_UNDEFINED_ITEM);
    DeleteItemList(list);
    EvalContextDestroy(ctx);
}
#endif

static void test_list_compare(void)
{
    Item *list1 = NULL, *list2 = NULL;
    bool result;

    result = ListsCompare(list1, list2);
    assert_true(result);

    AppendItem(&list1, "abc", NULL);
    AppendItem(&list1, "def", NULL);

    result = ListsCompare(list1, list2);
    assert_false(result);

    AppendItem(&list2, "def", NULL);
    AppendItem(&list2, "abc", NULL);

    result = ListsCompare(list1, list2);

    assert_true(result);

    DeleteItemList(list1);
    DeleteItemList(list2);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_prepend_item),
        unit_test(test_list_len),
        unit_test(test_list_compare)
    };

    return run_tests(tests);
}
