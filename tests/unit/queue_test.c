#include <test.h>

#include <string.h>
#include <queue.c>
#include <queue.h>

static void test_basics(void)
{
    Queue *q = QueueNew(free);

    assert_int_equal(0, QueueCount(q));
    assert_true(QueueIsEmpty(q));


    QueueEnqueue(q, xstrdup("hello"));
    assert_int_equal(1, QueueCount(q));
    assert_false(QueueIsEmpty(q));
    assert_string_equal("hello", QueueHead(q));


    QueueEnqueue(q, xstrdup("world"));
    assert_int_equal(2, QueueCount(q));
    assert_string_equal("hello", QueueHead(q));


    char *head = QueueDequeue(q);
    assert_string_equal("hello", head);
    free(head);


    assert_string_equal("world", QueueHead(q));
    head = QueueDequeue(q);
    assert_string_equal("world", head);
    free(head);


    QueueDestroy(q);
}


static void test_destroy(void)
{
    Queue *q = QueueNew(free);

    QueueEnqueue(q, xstrdup("1"));
    QueueEnqueue(q, xstrdup("2"));
    QueueEnqueue(q, xstrdup("3"));

    assert_int_equal(3, QueueCount(q));

    QueueDestroy(q);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_basics),
        unit_test(test_destroy)
    };
    return run_tests(tests);
}
