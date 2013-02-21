#include "cf3.defs.h"
#include "sort.h"

#include "rlist.h"
#include "item_lib.h"

#include <setjmp.h>
#include <cmockery.h>
#include <stdarg.h>

/*
 * Those testcases only perform smoke testing of sorting functionality.
 */

void test_sort_item_list_names(void **ctx)
{
    Item *head = xcalloc(1, sizeof(Item));
    head->name = "c";
    head->next = xcalloc(1, sizeof(Item));
    head->next->name = "b";
    head->next->next = xcalloc(1, sizeof(Item));
    head->next->next->name = "a";

    Item *sorted = SortItemListNames(head);

    assert_string_equal(sorted->name, "a");
    assert_string_equal(sorted->next->name, "b");
    assert_string_equal(sorted->next->next->name, "c");
    assert_int_equal(sorted->next->next->next, NULL);
}

void test_sort_item_list_classes(void **ctx)
{
    Item *head = xcalloc(1, sizeof(Item));
    head->classes = "b";
    head->next = xcalloc(1, sizeof(Item));
    head->next->classes = "c";
    head->next->next = xcalloc(1, sizeof(Item));
    head->next->next->classes = "a";

    Item *sorted = SortItemListClasses(head);

    assert_string_equal(sorted->classes, "a");
    assert_string_equal(sorted->next->classes, "b");
    assert_string_equal(sorted->next->next->classes, "c");
    assert_int_equal(sorted->next->next->next, NULL);
}

void test_sort_item_list_counters(void **ctx)
{
    Item *head = xcalloc(1, sizeof(Item));
    head->counter = -1;
    head->next = xcalloc(1, sizeof(Item));
    head->next->counter = 42;
    head->next->next = xcalloc(1, sizeof(Item));
    head->next->next->counter = 146;

    Item *sorted = SortItemListCounters(head);

    /* Weird. Counters are sorted backwards */
    assert_int_equal(sorted->counter, 146);
    assert_int_equal(sorted->next->counter, 42);
    assert_int_equal(sorted->next->next->counter, -1);
    assert_int_equal(sorted->next->next->next, NULL);
}

void test_sort_item_list_times(void **ctx)
{
    Item *head = xcalloc(1, sizeof(Item));
    head->time = 1;
    head->next = xcalloc(1, sizeof(Item));
    head->next->time = 1998;
    head->next->next = xcalloc(1, sizeof(Item));
    head->next->next->time = 4000;

    Item *sorted = SortItemListCounters(head);

    assert_int_equal(sorted->time, 4000);
    assert_int_equal(sorted->next->time, 1998);
    assert_int_equal(sorted->next->next->time, 1);
    assert_int_equal(sorted->next->next->next, NULL);
}

int FirstItemShorter(const char *lhs, const char *rhs)
{
    return strlen(lhs) < strlen(rhs);
}

void test_sort_rlist(void **ctx)
{
    Rlist *head = xcalloc(1, sizeof(Rlist));
    head->item = "a";
    head->next = xcalloc(1, sizeof(Rlist));
    head->next->item = "bbb";
    head->next->next = xcalloc(1, sizeof(Rlist));
    head->next->next->item = "cc";

    Rlist *sorted = SortRlist(head, &FirstItemShorter);

    assert_string_equal(sorted->item, "a");
    assert_string_equal(sorted->next->item, "cc");
    assert_string_equal(sorted->next->next->item, "bbb");
    assert_int_equal(sorted->next->next->next, NULL);
}

void test_alpha_sort_rlist_names(void **ctx)
{
    Rlist *head = xcalloc(1, sizeof(Rlist));
    head->item = "c";
    head->next = xcalloc(1, sizeof(Rlist));
    head->next->item = "a";
    head->next->next = xcalloc(1, sizeof(Rlist));
    head->next->next->item = "b";

    Rlist *sorted = AlphaSortRListNames(head);

    assert_string_equal(sorted->item, "a");
    assert_string_equal(sorted->next->item, "b");
    assert_string_equal(sorted->next->next->item, "c");
    assert_int_equal(sorted->next->next->next, NULL);
}

int main()
{
    const UnitTest tests[] =
        {
            unit_test(test_sort_item_list_names),
            unit_test(test_sort_item_list_classes),
            unit_test(test_sort_item_list_counters),
            unit_test(test_sort_item_list_times),
            unit_test(test_sort_rlist),
            unit_test(test_alpha_sort_rlist_names),
        };

    return run_tests(tests);
}

/* STUBS */

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    fail();
    exit(42);
}

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}

