#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>

#include "alphalist.h"
#include "item_lib.h"

static void test_create_destroy(void **state)
{
    AlphaList l;

    InitAlphaList(&l);
    DeleteAlphaList(&l);
}

static void test_prepend(void **state)
{
    AlphaList l;

    InitAlphaList(&l);
    assert_int_equal(InAlphaList(&l, "mystring"), 0);
    PrependAlphaList(&l, "mystring");
    assert_int_equal(InAlphaList(&l, "mystring"), 1);
    DeleteAlphaList(&l);
}

static void test_prepend_empty(void **state)
{
    AlphaList l;

    InitAlphaList(&l);
    assert_int_equal(InAlphaList(&l, "hello"), 0);
    PrependAlphaList(&l, "hello");
    assert_int_equal(InAlphaList(&l, "hello"), 1);
    DeleteAlphaList(&l);
}

static void test_empty_iterator(void **state)
{
    AlphaList l;

    InitAlphaList(&l);
    AlphaListIterator i = AlphaListIteratorInit(&l);

    assert_false(AlphaListIteratorNext(&i));
    DeleteAlphaList(&l);
}

static void test_iterator(void **state)
{
    AlphaList l;

    InitAlphaList(&l);
    PrependAlphaList(&l, "test");
    AlphaListIterator i = AlphaListIteratorInit(&l);

    assert_string_equal(AlphaListIteratorNext(&i)->name, "test");
    assert_false(AlphaListIteratorNext(&i));
    DeleteAlphaList(&l);
}

static void test_long_iterator(void **state)
{
    AlphaList l;

    InitAlphaList(&l);
    PrependAlphaList(&l, "a_test");
    PrependAlphaList(&l, "a_test2");
    PrependAlphaList(&l, "d_test");
    PrependAlphaList(&l, "d_test2");
    PrependAlphaList(&l, "d_test3");
    PrependAlphaList(&l, "b_test");

    AlphaListIterator i = AlphaListIteratorInit(&l);

    assert_string_equal(AlphaListIteratorNext(&i)->name, "a_test2");
    assert_string_equal(AlphaListIteratorNext(&i)->name, "a_test");
    assert_string_equal(AlphaListIteratorNext(&i)->name, "b_test");
    assert_string_equal(AlphaListIteratorNext(&i)->name, "d_test3");
    assert_string_equal(AlphaListIteratorNext(&i)->name, "d_test2");
    assert_string_equal(AlphaListIteratorNext(&i)->name, "d_test");
    assert_false(AlphaListIteratorNext(&i));
    DeleteAlphaList(&l);
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_create_destroy),
        unit_test(test_prepend),
        unit_test(test_prepend_empty),
        /* unit_test(test_shallow_copy), */
        /* unit_test(test_idemp_prepend), */
        /* unit_test(test_in), */
        /* unit_test(test_match), */

        unit_test(test_empty_iterator),
        unit_test(test_iterator),
        unit_test(test_long_iterator),
    };

    return run_tests(tests);
}
