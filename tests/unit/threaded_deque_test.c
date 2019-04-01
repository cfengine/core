#include <test.h>

#include <alloc.h>
#include <mutex.h>
#include <threaded_deque.h>

/* Memory illustration legend:                      *
 *      | : memory bounds                           *
 *      > : left  (first non-empty index of data)   *
 *      < : right (first empty index after data)    *
 *      ^ : left + right (empty)                    *
 *      v : left + right (at capacity)              *
 *      x : used memory                             *
 *      - : unused memory                           */

static void test_push_pop(void)
{
    // Initialised with DEFAULT_CAPACITY = 16
    ThreadedDeque *deque = ThreadedDequeNew(0, free);
    // |^---------------|
    ThreadedDequePushLeft(deque, xstrdup("1"));
    // |<-------------->|
    ThreadedDequePushRight(deque, xstrdup("2"));
    // |x<------------->|
    ThreadedDequePushLeft(deque, xstrdup("3"));
    // |x<------------>x|

    char *str1; ThreadedDequePopRight(deque, (void **)&str1, 0);
    // |<------------->x|
    char *str2; ThreadedDequePopLeft(deque, (void **)&str2, 0);
    // |<-------------->|
    char *str3; ThreadedDequePopRight(deque, (void **)&str3, 0);
    // |---------------^|

    assert_string_equal(str1, "2");
    assert_string_equal(str2, "3");
    assert_string_equal(str3, "1");

    free(str1);
    free(str2);
    free(str3);

    ThreadedDequeDestroy(deque);
}

static void test_pop_empty_and_push_null(void)
{
    ThreadedDeque *deque = ThreadedDequeNew(1, NULL);
    // |^|

    assert(ThreadedDequeIsEmpty(deque));

    void *i_am_null = NULL;
    bool ret = ThreadedDequePopLeft(deque, &i_am_null, 0);

    // |^|
    assert(i_am_null == NULL);
    assert_false(ret);
    ThreadedDequePushLeft(deque, i_am_null);
    // |v|
    ret = ThreadedDequePopLeft(deque, &i_am_null, 0);
    assert(i_am_null == NULL);
    assert_true(ret);
    // |^|

    ret = ThreadedDequePopRight(deque, &i_am_null, 0);

    // |^|
    assert(i_am_null == NULL);
    assert_false(ret);
    ThreadedDequePushRight(deque, i_am_null);
    // |v|
    ret = ThreadedDequePopRight(deque, &i_am_null, 0);
    assert(i_am_null == NULL);
    assert_true(ret);
    // |^|

    ThreadedDequeDestroy(deque);
}

static void test_copy(void)
{
    ThreadedDeque *deque = ThreadedDequeNew(4, free);
    // deque: |^---|

    ThreadedDequePushRight(deque, xstrdup("1"));
    // deque: |><--|
    ThreadedDequePushRight(deque, xstrdup("2"));
    // deque: |>x<-|
    ThreadedDequePushRight(deque, xstrdup("3"));
    // deque: |>xx<|

    ThreadedDeque *new_deque = ThreadedDequeCopy(deque);
    // new_deque: |>xx<|

    assert(new_deque != NULL);
    assert_int_equal(ThreadedDequeCount(deque),
                     ThreadedDequeCount(new_deque));
    assert_int_equal(ThreadedDequeCapacity(deque),
                     ThreadedDequeCapacity(new_deque));

    char *old_str1; ThreadedDequePopLeft(deque, (void **)&old_str1, 0);
    // deque: |->x<|
    char *old_str2; ThreadedDequePopLeft(deque, (void **)&old_str2, 0);
    // deque: |--><|
    char *old_str3; ThreadedDequePopLeft(deque, (void **)&old_str3, 0);
    // deque: |---^|

    char *new_str1; ThreadedDequePopLeft(new_deque, (void **)&new_str1, 0);
    // new_deque: |->x<|
    char *new_str2; ThreadedDequePopLeft(new_deque, (void **)&new_str2, 0);
    // new_deque: |--><|
    char *new_str3; ThreadedDequePopLeft(new_deque, (void **)&new_str3, 0);
    // new_deque: |---^|

    // Check if pointers are equal (since this is a shallow copy)
    assert(old_str1 == new_str1);
    assert(old_str2 == new_str2);
    assert(old_str3 == new_str3);

    free(old_str1);
    free(old_str2);
    free(old_str3);

    ThreadedDequeSoftDestroy(deque);

    // Tests expanding the copied deque
    ThreadedDequePushLeft(new_deque, xstrdup("1"));
    // new_deque: |--><|
    ThreadedDequePushRight(new_deque, xstrdup("2"));
    // new_deque: |<->x|
    ThreadedDequePushLeft(new_deque, xstrdup("3"));
    // new_deque: |<>xx|
    ThreadedDequePushRight(new_deque, xstrdup("4"));
    // new_deque: |xvxx|
    ThreadedDequePushLeft(new_deque, xstrdup("5"));
    // Internal array restructured, array moved to end:
    // new_deque: |>xxxx<--|

    assert_int_equal(ThreadedDequeCount(new_deque), 5);
    assert_int_equal(ThreadedDequeCapacity(new_deque), 8);

    ThreadedDequePopRight(new_deque, (void **)&new_str1, 0);
    // new_deque: |>xxx<---|
    ThreadedDequePopLeft(new_deque, (void **)&new_str2, 0);
    // new_deque: |->xx<---|
    ThreadedDequePopRight(new_deque, (void **)&new_str3, 0);
    // new_deque: |->x<----|
    char *new_str4; ThreadedDequePopLeft(new_deque, (void **)&new_str4, 0);
    // new_deque: |--><----|
    char *new_str5; ThreadedDequePopRight(new_deque, (void **)&new_str5, 0);
    // new_deque: |--^-----|

    assert_string_equal(new_str1, "4");
    assert_string_equal(new_str2, "5");
    assert_string_equal(new_str3, "2");
    assert_string_equal(new_str4, "3");
    assert_string_equal(new_str5, "1");

    free(new_str1);
    free(new_str2);
    free(new_str3);
    free(new_str4);
    free(new_str5);

    ThreadedDequeDestroy(new_deque);
}

static void test_push_report_count(void)
{
    ThreadedDeque *deque = ThreadedDequeNew(0, free);

    size_t size1 = ThreadedDequePushLeft(deque, xstrdup("1"));
    size_t size2 = ThreadedDequePushLeft(deque, xstrdup("2"));
    size_t size3 = ThreadedDequePushLeft(deque, xstrdup("3"));
    size_t size4 = ThreadedDequePushLeft(deque, xstrdup("4"));

    assert_int_equal(size1, 1);
    assert_int_equal(size2, 2);
    assert_int_equal(size3, 3);
    assert_int_equal(size4, 4);

    ThreadedDequeDestroy(deque);
}

static void test_expand(void)
{
    ThreadedDeque *deque = ThreadedDequeNew(1, free);
    // |^|

    ThreadedDequePushRight(deque, xstrdup("spam"));
    // |v|
    ThreadedDequePushRight(deque, xstrdup("spam"));
    // |vx|

    char *tmp; ThreadedDequePopLeft(deque, (void **)&tmp, 0);
    // |<>|
    free(tmp);

    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // |vx|
    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // Internal array restructured:
    // |xx<>|
    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // |xxvx|
    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // Internal array restructured:
    // |->xxxx<-|
    ThreadedDequePushRight(deque, xstrdup("spam"));
    // |->xxxxx<|

    ThreadedDequePopLeft(deque, (void **)&tmp, 0);
    // |-->xxxx<|
    free(tmp);
    ThreadedDequePopLeft(deque, (void **)&tmp, 0);
    // |--->xxx<|
    free(tmp);

    ThreadedDequePushRight(deque, xstrdup("spam"));
    // |<-->xxxx|
    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // |<->xxxxx|
    ThreadedDequePushRight(deque, xstrdup("spam"));
    // |x<>xxxxx|
    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // |xxvxxxxx|
    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // Internal array restructured
    // |->xxxxxxxx<-----|
    ThreadedDequePushRight(deque, xstrdup("spam"));
    // |->xxxxxxxxx<----|
    ThreadedDequePushLeft(deque, xstrdup("spam"));
    // |>xxxxxxxxxx<----|

    assert_int_equal(ThreadedDequeCount(deque), 11);
    assert_int_equal(ThreadedDequeCapacity(deque), 16);

    ThreadedDequeDestroy(deque);
}

static void test_popn(void)
{
    ThreadedDeque *deque = ThreadedDequeNew(0, free);
    // Initialised with default size 16
    // |^---------------|

    char *strs[] = {"spam1", "spam2", "spam3", "spam4", "spam5"};

    for (int i = 0; i < 5; i++)
    {
        ThreadedDequePushLeft(deque, xstrdup(strs[i]));
    }
    // |<---------->xxxx|

    void **data = NULL;
    size_t count = ThreadedDequePopRightN(deque, &data, 5, 0);
    // |-----------^----|

    for (size_t i = 0; i < count; i++)
    {
        assert_string_equal(data[i], strs[i]);
        free(data[i]);
    }

    free(data);
    data = NULL;

    for (int i = 0; i < 5; i++)
    {
        ThreadedDequePushRight(deque, xstrdup(strs[i]));
    }
    // |<---------->xxxx|

    count = ThreadedDequePopLeftN(deque, &data, 5, 0);
    // |^---------------|

    for (size_t i = 0; i < count; i++)
    {
        assert_string_equal(data[i], strs[i]);
        free(data[i]);
    }

    free(data);
    ThreadedDequeDestroy(deque);
}

// Thread tests
static ThreadedDeque *thread_deque;

static void *thread_pop()
{
    char *tmp;
    ThreadedDequePopLeft(thread_deque, (void **)&tmp, THREAD_BLOCK_INDEFINITELY);
    assert_string_equal(tmp, "bla");
    free(tmp);

    return NULL;
}

static void *thread_push()
{
    char *str = "bla";
    ThreadedDequePushLeft(thread_deque, xstrdup(str));

    return NULL;
}

static void *thread_wait_empty()
{
    ThreadedDequeWaitEmpty(thread_deque, THREAD_BLOCK_INDEFINITELY);
    ThreadedDequePushLeft(thread_deque, xstrdup("a_test"));

    return NULL;
}

static void test_threads_wait_pop(void)
{
#define POP_ITERATIONS 100
    thread_deque = ThreadedDequeNew(0, free);

    pthread_t pops[POP_ITERATIONS] = {0};
    for (int i = 0; i < POP_ITERATIONS; i++)
    {
        int res_create = pthread_create(&(pops[i]), NULL,
                                        thread_pop, NULL);
        assert_int_equal(res_create, 0);
    }

    pthread_t pushs[POP_ITERATIONS] = {0};
    for (int i = 0; i < POP_ITERATIONS; i++)
    {
        int res_create = pthread_create(&(pushs[i]), NULL,
                                        thread_push, NULL);
        assert_int_equal(res_create, 0);
    }

    void *retval = NULL;
    int res;

    for (int i = 0; i < POP_ITERATIONS; i++)
    {
        res = pthread_join(pops[i], retval);
        assert_int_equal(res, 0);
        assert(retval == NULL);

        res = pthread_join(pushs[i], retval);
        assert_int_equal(res, 0);
        assert(retval == NULL);
    }

    ThreadedDequeDestroy(thread_deque);
}

static void test_threads_wait_empty(void)
{
#define WAIT_ITERATIONS 100
    thread_deque = ThreadedDequeNew(0, free);

    pthread_t pushs[WAIT_ITERATIONS] = {0};
    for (int i = 0; i < WAIT_ITERATIONS; i++)
    {
        int res_create = pthread_create(&(pushs[i]), NULL,
                                        thread_push, NULL);
        assert_int_equal(res_create, 0);
    }

    sleep(1);
    pthread_t wait_thread = 0;
    int res_create = pthread_create(&wait_thread, NULL,
                                    thread_wait_empty, NULL);
    assert_int_equal(res_create, 0);

    do {
        sleep(1);
    } while (ThreadedDequeCount(thread_deque) != WAIT_ITERATIONS);

    char **data_array = NULL;
    size_t arr_size = ThreadedDequePopLeftN(thread_deque,
                                            (void ***)&data_array,
                                            WAIT_ITERATIONS, 0);

    for (size_t i = 0; i < arr_size; i++)
    {
        free(data_array[i]);
    }

    free(data_array);

    char *waited_str; ThreadedDequePopLeft(thread_deque, (void **)&waited_str, 1);
    assert_string_equal(waited_str, "a_test");
    free(waited_str);

    void *retval = NULL;
    int res;

    for (int i = 0; i < WAIT_ITERATIONS; i++)
    {
        res = pthread_join(pushs[i], retval);
        assert_int_equal(res, 0);
        assert(retval == NULL);
    }

    res = pthread_join(wait_thread, retval);
    assert_int_equal(res, 0);
    assert(retval == NULL);

    ThreadedDequeDestroy(thread_deque);
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
        unit_test(test_popn),
        unit_test(test_threads_wait_pop),
        unit_test(test_threads_wait_empty),
    };
    return run_tests(tests);
}
