#include "cf3.defs.h"

#include <setjmp.h>
#include <cmockery.h>
#include "item_lib.h"

static void test_prepend_item(void **state)
{
    Item *ip = NULL, *list = NULL;

    ip = PrependItem(&list, "hello", "classes");
    assert_int_not_equal(ip, NULL);
    assert_int_not_equal(list, NULL);
    DeleteItem(&list, ip);
    assert_int_equal(list, NULL);
}

static void test_list_len(void **state)
{
    Item *list = NULL;

    PrependItem(&list, "one", "classes");
    PrependItem(&list, "two", NULL);
    PrependItem(&list, "three", NULL);
    assert_int_equal(ListLen(list), 3);
    DeleteItemList(list);
}

static void test_list_select_last_matching_finds_first(void **state)
{
    Item *list = NULL, *match = NULL, *prev = NULL;
    bool result = false;

    AppendItem(&list, "abc", NULL);
    AppendItem(&list, "def", NULL);
    AppendItem(&list, "ghi", NULL);
    AppendItem(&list, "jkl", NULL);

    result = SelectLastItemMatching("abc", list, NULL, &match, &prev);
    assert_true(result);
    assert_int_equal(match, list);
    assert_int_equal(prev, CF_UNDEFINED_ITEM);
    DeleteItemList(list);
}

static void test_list_select_last_matching_finds_last(void **state)
{
    Item *list = NULL, *match = NULL, *prev = NULL;
    bool result;

    AppendItem(&list, "abc", NULL);
    AppendItem(&list, "def", NULL);
    AppendItem(&list, "ghi", NULL);
    AppendItem(&list, "abc", NULL);

    result = SelectLastItemMatching("abc", list, NULL, &match, &prev);
    assert_true(result);
    assert_int_equal(match, list->next->next->next);
    assert_int_equal(prev, list->next->next);
    DeleteItemList(list);
}

static void test_list_select_last_matching_not_found(void **state)
{
    Item *list = NULL, *match = NULL, *prev = NULL;
    bool result;

    AppendItem(&list, "abc", NULL);
    AppendItem(&list, "def", NULL);
    AppendItem(&list, "ghi", NULL);
    AppendItem(&list, "abc", NULL);

    result = SelectLastItemMatching("xyz", list, NULL, &match, &prev);
    assert_false(result);
    assert_int_equal(match, CF_UNDEFINED_ITEM);
    assert_int_equal(prev, CF_UNDEFINED_ITEM);
    DeleteItemList(list);
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_prepend_item),
        unit_test(test_list_len),
        unit_test(test_list_select_last_matching_finds_first),
        unit_test(test_list_select_last_matching_finds_last),
        unit_test(test_list_select_last_matching_not_found)
    };

    return run_tests(tests);
}
