#include <test.h>
#include <string.h>
#include <queue.c>
#include <queue.h>

/*
 * Helper functions:
 * copy: Copies an element, in this case strings.
 * destroy: frees the memory, in this case strings.
 */
static void copy(const void *source, void **destination)
{
    *destination = xstrdup(source);
}

static void destroy(void *element)
{
    free(element);
}

/*
 * String to use on the tests, they need to be strdup'ed otherwise there will be
 * a segfault caused by this data being on the stack.
 */
char first_string[] = "first string";
char second_string[] = "second string";
char third_string[] = "third string";

/* Flag to make sure we only run other tests if the basic are in place */
static bool basic_test_passed = false;

/* This test just checks that the basics are in place, not the functionality */
static void queue_basic_test(void)
{
    Queue *queue = NULL;

    QueueDestroy(&queue);
    assert_int_equal(-1, QueueCopy(queue, NULL));
    assert_int_equal(-1, QueueEnqueue(queue, NULL));
    assert_true(NULL == QueueDequeue(queue));
    assert_true(NULL == QueueHead(queue));
    assert_int_equal(-1, QueueCount(queue));

    assert_true(NULL == QueueNew(NULL, NULL));
    assert_true(NULL == QueueNew(NULL, destroy));

    assert_true(queue == NULL);
    queue = QueueNew(copy, NULL);
    assert_true(queue != NULL);
    assert_int_equal(0, queue->node_count);
    assert_true(NULL == queue->head);
    assert_true(NULL == queue->tail);
    assert_true(NULL == queue->queue);
    assert_true(NULL != queue->ref_count);
    assert_true(NULL != queue->copy);
    assert_true(NULL == queue->destroy);
    QueueDestroy(&queue);
    assert_true(queue == NULL);

    queue = QueueNew(copy, destroy);
    assert_true(queue != NULL);
    assert_int_equal(0, queue->node_count);
    assert_true(NULL == queue->head);
    assert_true(NULL == queue->tail);
    assert_true(NULL == queue->queue);
    assert_true(NULL != queue->ref_count);
    assert_true(NULL != queue->copy);
    assert_true(NULL != queue->destroy);
    QueueDestroy(&queue);
    assert_true(queue == NULL);

    /* Mark as passed so other tests can run */
    basic_test_passed = true;
}

static void queue_enqueue_dequeue_test(void)
{
    /* If basic tests failed, there is no point in running this */
    assert_true(basic_test_passed);

    Queue *queue = NULL;
    char *first = xstrdup(first_string);
    char *second = xstrdup(second_string);
    char *third = xstrdup(third_string);

    queue = QueueNew(copy, destroy);
    assert_true(queue != NULL);

    /* Enqueue some elements */
    char *head = NULL;
    assert_int_equal(0, QueueEnqueue(queue, first));
    assert_int_equal(1, queue->node_count);
    assert_true(queue->head->data == first);
    assert_true(queue->tail->data == first);
    assert_true(queue->queue->data == first);
    head = QueueHead(queue);
    assert_true(NULL != head);
    assert_string_equal(head, first);

    assert_int_equal(0, QueueEnqueue(queue, second));
    assert_int_equal(2, queue->node_count);
    assert_true(queue->head->data == first);
    assert_true(queue->tail->data == second);
    assert_true(queue->queue->data == first);
    head = QueueHead(queue);
    assert_true(NULL != head);
    assert_string_equal(head, first);

    assert_int_equal(0, QueueEnqueue(queue, third));
    assert_int_equal(3, queue->node_count);
    assert_true(queue->head->data == first);
    assert_true(queue->head->next->data == second);
    assert_true(queue->tail->previous->data == second);
    assert_true(queue->tail->data == third);
    assert_true(queue->queue->data == first);
    head = QueueHead(queue);
    assert_true(NULL != head);
    assert_string_equal(head, first);

    /* Dequeue some elements */
    char *data = QueueDequeue(queue);
    assert_true(NULL != data);
    assert_string_equal(data, first);
    assert_int_equal(2, queue->node_count);
    assert_true(queue->head->data == second);
    assert_true(queue->tail->data == third);
    assert_true(queue->queue->data == second);
    head = QueueHead(queue);
    assert_true(NULL != head);
    assert_string_equal(head, second);

    data = QueueDequeue(queue);
    assert_true(NULL != data);
    assert_string_equal(data, second);
    assert_int_equal(1, queue->node_count);
    assert_true(queue->head->data == third);
    assert_true(queue->tail->data == third);
    assert_true(queue->queue->data == third);
    head = QueueHead(queue);
    assert_true(NULL != head);
    assert_string_equal(head, third);

    data = QueueDequeue(queue);
    assert_true(NULL != data);
    assert_string_equal(data, third);
    assert_int_equal(0, queue->node_count);
    assert_true(queue->head == NULL);
    assert_true(queue->tail == NULL);
    assert_true(queue->queue == NULL);
    head = QueueHead(queue);
    assert_true(NULL == head);

    /* Destroy the empty queue no data should be freed now */
    QueueDestroy(&queue);
    assert_true(NULL == queue);
    assert_true(NULL != first);
    assert_true(NULL != second);
    assert_true(NULL != third);

    /* Create a new queue and fill it */
    queue = QueueNew(copy, destroy);
    assert_true(queue != NULL);
    assert_int_equal(0, QueueEnqueue(queue, first));
    assert_int_equal(0, QueueEnqueue(queue, second));
    assert_int_equal(0, QueueEnqueue(queue, third));
    /* Destroy the queue, all data should be freed now */
    QueueDestroy(&queue);
    assert_true(NULL == queue);
}

static void queue_copy_test(void)
{
    /* If basic tests failed, there is no point in running this */
    assert_true(basic_test_passed);

    Queue *queue = NULL;
    Queue *queue_copy = NULL;
    char *first = xstrdup(first_string);
    char *second = xstrdup(second_string);
    char *third = xstrdup(third_string);

    queue = QueueNew(copy, destroy);
    assert_true(queue != NULL);

    /* Enqueue one element */
    assert_int_equal(0, QueueEnqueue(queue, first));
    assert_int_equal(1, queue->node_count);
    /* Copy the queue */
    assert_int_equal(0, QueueCopy(queue, &queue_copy));
    assert_int_equal(queue->node_count, queue_copy->node_count);
    assert_int_equal(1, queue_copy->node_count);
    assert_true(queue->ref_count == queue_copy->ref_count);
    assert_true(queue->head == queue_copy->head);
    /* Add one element to the copy, this will detach them */
    assert_int_equal(0, QueueEnqueue(queue_copy, second));
    assert_int_equal(1, queue->node_count);
    assert_int_equal(2, queue_copy->node_count);
    assert_true(queue->ref_count != queue_copy->ref_count);
    assert_true(queue->head != queue_copy->head);
    assert_string_equal(queue->head->data, queue_copy->head->data);
    /* Add the same element to the original queue */
    assert_int_equal(0, QueueEnqueue(queue, second));
    assert_int_equal(2, queue->node_count);
    assert_int_equal(2, queue_copy->node_count);
    assert_true(queue->ref_count != queue_copy->ref_count);
    assert_true(queue->head != queue_copy->head);
    assert_string_equal(queue->head->data, queue_copy->head->data);
    assert_string_equal(queue->tail->data, queue_copy->tail->data);
    /* Destroy the original queue */
    QueueDestroy(&queue);
    assert_true(NULL == queue);
    /* Make the original a copy of the copy */
    assert_int_equal(0, QueueCopy(queue_copy, &queue));
    assert_int_equal(queue->node_count, queue_copy->node_count);
    assert_int_equal(2, queue_copy->node_count);
    assert_true(queue->ref_count == queue_copy->ref_count);
    assert_true(queue->head == queue_copy->head);
    /* Destroy the copy, the copy of the copy shouldn't be affected */
    QueueDestroy(&queue_copy);
    assert_true(NULL == queue_copy);
    assert_int_equal(2, queue->node_count);
    /* Finally copy again and add one element to the original */
    assert_int_equal(0, QueueCopy(queue, &queue_copy));
    assert_int_equal(queue->node_count, queue_copy->node_count);
    assert_int_equal(2, queue_copy->node_count);
    assert_true(queue->ref_count == queue_copy->ref_count);
    assert_true(queue->head == queue_copy->head);
    /* Add the element */
    assert_int_equal(0, QueueEnqueue(queue, third));
    assert_int_equal(3, queue->node_count);
    assert_int_equal(2, queue_copy->node_count);
    assert_true(queue->ref_count != queue_copy->ref_count);
    assert_true(queue->head != queue_copy->head);
    assert_string_equal(queue->head->data, queue_copy->head->data);
    /* Destroy both queues */
    QueueDestroy(&queue);
    assert_true(NULL == queue);
    QueueDestroy(&queue_copy);
    assert_true(NULL == queue_copy);
    /* Final test, copy an empty queue */
    queue = QueueNew(copy, destroy);
    assert_true(queue != NULL);
    assert_int_equal(0, QueueCopy(queue, &queue_copy));
    assert_int_equal(0, queue->node_count);
    assert_int_equal(queue->node_count, queue_copy->node_count);
    assert_true(queue->ref_count != queue_copy->ref_count);
    assert_true(NULL == queue->head);
    assert_true(queue->head == queue_copy->head);
    assert_true(queue->tail == queue_copy->tail);
    assert_true(queue->queue == queue_copy->queue);
    QueueDestroy(&queue);
    assert_true(NULL == queue);
    QueueDestroy(&queue_copy);
    assert_true(NULL == queue_copy);
}

static void queue_api_test(void)
{
    /* If basic tests failed, there is no point in running this */
    assert_true(basic_test_passed);

    Queue *queue = NULL;
    char *first = xstrdup(first_string);

    queue = QueueNew(copy, destroy);
    assert_true(queue != NULL);
    assert_true(QueueIsEmpty(queue));
    /* Enqueue the element */
    char *head = NULL;
    char *data = NULL;
    assert_int_equal(0, QueueEnqueue(queue, first));
    assert_int_equal(1, QueueCount(queue));
    assert_false(QueueIsEmpty(queue));
    /* Ask for the first element without dequeuing it */
    head = QueueHead(queue);
    assert_int_equal(1, QueueCount(queue));
    assert_string_equal(head, first);
    assert_false(QueueIsEmpty(queue));
    /* Dequeue the first element */
    data = QueueDequeue(queue);
    assert_int_equal(0, QueueCount(queue));
    assert_string_equal(head, data);
    assert_true(QueueIsEmpty(queue));
    /* Destroy the queue */
    QueueDestroy(&queue);
    assert_true(NULL == queue);
    assert_true(QueueIsEmpty(queue));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(queue_basic_test),
        unit_test(queue_enqueue_dequeue_test),
        unit_test(queue_copy_test),
        unit_test(queue_api_test)
    };
    return run_tests(tests);
}
