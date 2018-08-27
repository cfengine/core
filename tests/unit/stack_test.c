#include "test.h"

#include <alloc.h>
#include <stack.h>

static void test_push_pop(void)
{
    Stack *stack = StackNew(0, free);

    StackPush(stack, xstrdup("1"));
    StackPush(stack, xstrdup("2"));
    StackPush(stack, xstrdup("3"));

    char *str1 = StackPop(stack);
    char *str2 = StackPop(stack);
    char *str3 = StackPop(stack);
    assert_int_equal(strcmp(str1, "3"), 0);
    assert_int_equal(strcmp(str2, "2"), 0);
    assert_int_equal(strcmp(str3, "1"), 0);

    free(str1);
    free(str2);
    free(str3);

    StackDestroy(stack);
}

static void test_pop_empty_and_push_null(void)
{
    Stack *stack = StackNew(1, NULL);

    assert(StackIsEmpty(stack));

    void *i_am_null = StackPop(stack);
    assert(i_am_null == NULL);
    StackPush(stack, i_am_null);
    assert(StackPop(stack) == NULL);

    StackDestroy(stack);
}

static void test_copy(void)
{
    Stack *stack = StackNew(4, free);

    StackPush(stack, xstrdup("1"));
    StackPush(stack, xstrdup("2"));
    StackPush(stack, xstrdup("3"));

    Stack *new_stack = StackCopy(stack);

    assert(new_stack != NULL);
    assert_int_equal(StackCount(stack), StackCount(new_stack));
    assert_int_equal(StackCapacity(stack), StackCapacity(new_stack));

    char *old_str1 = StackPop(stack); char *new_str1 = StackPop(new_stack);
    char *old_str2 = StackPop(stack); char *new_str2 = StackPop(new_stack);
    char *old_str3 = StackPop(stack); char *new_str3 = StackPop(new_stack);

    assert(old_str1 == new_str1);
    assert(old_str2 == new_str2);
    assert(old_str3 == new_str3);

    free(old_str1);
    free(old_str2);
    free(old_str3);

    StackSoftDestroy(stack);

    // Tests expanding the copied stack
    StackPush(new_stack, xstrdup("1"));
    StackPush(new_stack, xstrdup("2"));
    StackPush(new_stack, xstrdup("3"));
    StackPush(new_stack, xstrdup("4"));
    StackPush(new_stack, xstrdup("5"));

    assert_int_equal(StackCount(new_stack), 5);
    assert_int_equal(StackCapacity(new_stack), 8);

    new_str1 = StackPop(new_stack);
    new_str2 = StackPop(new_stack);
    new_str3 = StackPop(new_stack);
    char *new_str4 = StackPop(new_stack);
    char *new_str5 = StackPop(new_stack);

    assert_int_equal(strcmp(new_str1, "5"), 0);
    assert_int_equal(strcmp(new_str2, "4"), 0);
    assert_int_equal(strcmp(new_str3, "3"), 0);
    assert_int_equal(strcmp(new_str4, "2"), 0);
    assert_int_equal(strcmp(new_str5, "1"), 0);

    free(new_str1);
    free(new_str2);
    free(new_str3);
    free(new_str4);
    free(new_str5);

    StackDestroy(new_stack);
}

static void test_push_report_count(void)
{
    Stack *stack = StackNew(0, free);

    size_t size1 = StackPushReportCount(stack, xstrdup("1"));
    size_t size2 = StackPushReportCount(stack, xstrdup("2"));
    size_t size3 = StackPushReportCount(stack, xstrdup("3"));
    size_t size4 = StackPushReportCount(stack, xstrdup("4"));

    assert_int_equal(size1, 1);
    assert_int_equal(size2, 2);
    assert_int_equal(size3, 3);
    assert_int_equal(size4, 4);

    StackDestroy(stack);
}

static void test_expand(void)
{
    Stack *stack = StackNew(1, free);

    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));
    StackPush(stack, xstrdup("spam"));

    assert_int_equal(StackCount(stack), 9);
    assert_int_equal(StackCapacity(stack), 16);

    StackDestroy(stack);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_push_pop),
        unit_test(test_pop_empty_and_push_null),
        unit_test(test_copy),
        unit_test(test_push_report_count),
        unit_test(test_expand),
    };
    return run_tests(tests);
}
